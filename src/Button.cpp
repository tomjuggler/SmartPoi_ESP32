#include "Button.h"

#if HAVE_BUTTON

#include "Globals.h"
#include <EEPROM.h>
#include <driver/gpio.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

// Software debounce window (the hardware only has the internal pull-up).
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;

// Hold longer than this to trigger the long-press action (LEDs off).
static constexpr uint32_t BUTTON_LONG_PRESS_MS = 700;

// Poll interval while a press is being measured.
static constexpr uint32_t BUTTON_POLL_MS = 20;

// Single-image modes run from 8 to 62 inclusive (62 images: a-z, A-Z, 0-9).
static constexpr int BUTTON_IMAGE_MODE_MIN = 8;
static constexpr int BUTTON_IMAGE_MODE_MAX = 62;

// Sequence modes.
static constexpr int BUTTON_SEQUENCE_MODE_MIN = 1;
static constexpr int BUTTON_SEQUENCE_MODE_MAX = 5;

// Pattern used to turn the LEDs off.
static constexpr int PATTERN_LEDS_OFF = 7;

static constexpr gpio_num_t BUTTON_GPIO = static_cast<gpio_num_t>(BUTTON_PIN);
static constexpr int BUTTON_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t BUTTON_TASK_PRIO = 1;

// Image reload flags live in ShowLittleFSImage.cpp / TimeFunc.cpp.
extern char currentLoadedImageChar;
extern bool fileNeedsReload;

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------

static TaskHandle_t buttonTaskHandle = nullptr;
static volatile TickType_t lastIsrTick = 0;

// -----------------------------------------------------------------------------
// Pattern actions (must run outside the ISR)
// -----------------------------------------------------------------------------

/**
 * Apply a pattern selected by the button and persist it in the same EEPROM
 * locations used by the web handler (bytes 10 and 11).
 */
static void buttonApplyPattern(int newPattern) {
  pattern = newPattern;
  patternChooser = newPattern;

  EEPROM.write(10, static_cast<uint8_t>(newPattern));
  if (newPattern >= BUTTON_SEQUENCE_MODE_MIN &&
      newPattern <= BUTTON_SEQUENCE_MODE_MAX) {
    EEPROM.write(11, static_cast<uint8_t>(newPattern));
  }
  EEPROM.commit();

  // Restart image rotation and force the file reader to pick up the change.
  imageToUse = 0;
  previousMillis3 = millis();
  fileNeedsReload = true;
  currentLoadedImageChar = '\0';

  // Allow pattern 7 to switch the LEDs off on the next POV pass.
  leds_off = false;

  #if SERIAL_DEBUG
  Serial.printf("[BUTTON] pattern -> %d\n", newPattern);
  #endif
}

/**
 * Short press:
 *   - modes 1-5: advance to the next sequence mode, wrapping 5 -> 1
 *   - modes 8-62: advance the single image, wrapping 62 -> 8
 *   - anything else: switch to mode 1
 *
 * Modes whose files are missing are skipped so the display never lands on an
 * empty pattern (mode 1 is always valid).
 */
static void buttonShortPress() {
  if (uploadInProgress) {
    return;
  }

  if (pattern >= BUTTON_SEQUENCE_MODE_MIN &&
      pattern <= BUTTON_SEQUENCE_MODE_MAX) {
    int candidate = pattern;
    bool found = false;

    for (int i = 0; i < BUTTON_SEQUENCE_MODE_MAX; i++) {
      candidate++;
      if (candidate > BUTTON_SEQUENCE_MODE_MAX) {
        candidate = BUTTON_SEQUENCE_MODE_MIN;
      }
      if (updateCurrentImagesForPattern(candidate)) {
        found = true;
        break;
      }
    }

    buttonApplyPattern(found ? candidate : BUTTON_SEQUENCE_MODE_MIN);
    return;
  }

  if (pattern >= BUTTON_IMAGE_MODE_MIN && pattern <= BUTTON_IMAGE_MODE_MAX) {
    int candidate = pattern;
    bool found = false;
    const int modeCount = BUTTON_IMAGE_MODE_MAX - BUTTON_IMAGE_MODE_MIN + 1;

    for (int i = 0; i < modeCount; i++) {
      candidate++;
      if (candidate > BUTTON_IMAGE_MODE_MAX) {
        candidate = BUTTON_IMAGE_MODE_MIN;
      }
      if (updateCurrentImagesForPattern(candidate)) {
        found = true;
        break;
      }
    }

    buttonApplyPattern(found ? candidate : BUTTON_SEQUENCE_MODE_MIN);
    return;
  }

  // Pattern 0, 6, 7, 63+ and anything unexpected: go to mode 1.
  updateCurrentImagesForPattern(BUTTON_SEQUENCE_MODE_MIN);
  buttonApplyPattern(BUTTON_SEQUENCE_MODE_MIN);
}

/**
 * Long press: turn the LEDs off (pattern 7).
 */
static void buttonLongPress() {
  if (uploadInProgress) {
    return;
  }

  pattern = PATTERN_LEDS_OFF;
  patternChooser = PATTERN_LEDS_OFF;
  EEPROM.write(10, PATTERN_LEDS_OFF);
  EEPROM.commit();

  // Reset so povDisplayTask performs the black-out.
  leds_off = false;

  #if SERIAL_DEBUG
  Serial.println("[BUTTON] long press -> LEDs off");
  #endif
}

// -----------------------------------------------------------------------------
// ISR and task
// -----------------------------------------------------------------------------

/**
 * Falling-edge ISR. Debounce is done in software here using the FreeRTOS tick
 * counter, then the button task is notified to measure the press length.
 */

static void buttonISR(void* arg) {
  (void)arg;

  const TickType_t now = xTaskGetTickCountFromISR();
  if ((now - lastIsrTick) < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
    return;
  }
  lastIsrTick = now;

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if (buttonTaskHandle != nullptr) {
    vTaskNotifyGiveFromISR(buttonTaskHandle, &higherPriorityTaskWoken);
  }
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void buttonTask(void* pvParameters) {
  (void)pvParameters;

  for (;;) {
    // Sleep until the ISR reports a press.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // The ISR recorded the edge time; use it so quick taps are not lost.
    const TickType_t pressStart = lastIsrTick;
    bool longPressHandled = false;

    // Wait for release, firing the long-press action once if held long enough.
    while (gpio_get_level(BUTTON_GPIO) == 0) {
      if (!longPressHandled &&
          (xTaskGetTickCount() - pressStart) >=
              pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS)) {
        longPressHandled = true;
        buttonLongPress();
      }
      vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }

    // Release debounce.
    vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

    if (!longPressHandled) {
      buttonShortPress();
    }
  }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void buttonInit() {
  // Configure the pin as input with the internal pull-up; the button pulls it
  // to GND when pressed.
  gpio_config_t ioConfig = {};
  ioConfig.pin_bit_mask = (1ULL << BUTTON_PIN);
  ioConfig.mode = GPIO_MODE_INPUT;
  ioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  ioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  ioConfig.intr_type = GPIO_INTR_NEGEDGE;
  gpio_config(&ioConfig);

  // Create the task before attaching the ISR so the handle is always valid.
  xTaskCreate(buttonTask, "BUTTON_TASK", BUTTON_TASK_STACK_SIZE, nullptr,
              BUTTON_TASK_PRIO, &buttonTaskHandle);

  esp_err_t isrResult = gpio_install_isr_service(0);
  if (isrResult != ESP_OK && isrResult != ESP_ERR_INVALID_STATE) {
    #if SERIAL_DEBUG
    Serial.printf("[BUTTON] ISR service init failed: %d\n", isrResult);
    #endif
    return;
  }

  gpio_isr_handler_add(BUTTON_GPIO, buttonISR, nullptr);

  #if SERIAL_DEBUG
  Serial.printf("[BUTTON] enabled on GPIO %d\n", BUTTON_PIN);
  #endif
}

#endif // HAVE_BUTTON