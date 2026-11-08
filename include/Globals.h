#ifndef GLOBALS_H
#define GLOBALS_H

// Platform detection
#if defined(ESP32)
  #define PLATFORM_ESP32
#else
#error "Unsupported platform - This project requires ESP32 platform!"
#endif

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <esp_now.h>
#include <FastLED.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>


// Configuration Constants
constexpr int NUM_LEDS = NUMLEDS; //from platformio.ini
constexpr int NUM_PX = NUMPX; //from platformio.ini
constexpr int DNS_PORT = 53;
constexpr unsigned int LOCAL_PORT = 2390;
constexpr int MAX_PX = MAXPX;
constexpr int DEFAULT_BRIGHTNESS = 20;
constexpr int BRIGHTNESS_RAMP_INTERVAL = 20;  // milliseconds
constexpr int BRIGHTNESS_RAMP_STEP = 5;       // brightness change per interval

constexpr int DATA_PIN = DATAPIN;
constexpr int CLOCK_PIN = CLOCKPIN;

// FreeRTOS Task Priorities (from blueprint)
#define POV_TASK_PRIO 5
#define WEB_TASK_PRIO 3
#define FILE_TASK_PRIO 2

// FreeRTOS Task Stack Sizes
#define POV_TASK_STACK_SIZE 8192
#define FILE_TASK_STACK_SIZE 8192
#define WEB_TASK_STACK_SIZE 8192

// Global Extern Variables
extern CRGB leds[NUM_LEDS];
extern WiFiUDP Udp;
extern uint8_t broadcastAddress[];  // ESP-NOW broadcast MAC
extern int newBrightness;  // Declaration for brightness control variable
extern int targetBrightness;  // Declaration for target brightness
extern bool routerOption;  // Declaration for router configuration flag
extern int wifiModeChooser;  // Declaration for WiFi mode selection
extern int patternChooser;
extern int pattern;
extern int apChannel;
extern int imageToUse;
extern int minImages;
extern int maxImages;
extern unsigned long previousMillis3;
extern long interval;
extern uint8_t addrNumA;
extern uint8_t addrNumB;
extern uint8_t addrNumC;
extern uint8_t addrNumD;
extern File settings;
extern String Field;
extern size_t maxPX;
extern IPAddress apIP;
// UDP Handler variables
extern unsigned long currentMillis2;
extern unsigned long previousMillis2;
extern bool checkit;
extern int len;
extern uint8_t packetBuffer[250];  // ESP-NOW v1.0 max + margin for up to 240px
extern uint8_t Y;
extern int state;
extern int X;
extern uint8_t R1;
extern uint8_t G1;
extern uint8_t M1;
extern bool channelChange;
extern bool uploadInProgress;  // Flag to disable FastLED operations during upload
extern volatile bool leds_off; // Flag to track if LEDs are already turned off (pattern 7)
extern IPAddress apIPauxillary;
extern IPAddress ipGatewayauxillary;
extern IPAddress ipSubnet;
extern const char* apName;
extern const char* apPass;
extern String images;
extern String images2;
extern String images3;
extern String images4;
extern String images5;
extern String currentImages;

// FreeRTOS Mutex Semaphores (for thread safety)
extern SemaphoreHandle_t bufferMutex;  // Protects message1Data array in RAM
extern SemaphoreHandle_t diskMutex;    // Protects SPI Flash bus

// FreeRTOS Task Handles
extern TaskHandle_t povTaskHandle;
extern TaskHandle_t fileTaskHandle;
extern TaskHandle_t webTaskHandle;

// Function declarations
extern bool updateCurrentImagesForPattern(int pattern);

// WiFi Mode Constants
  #define WIFI_STA WIFI_MODE_STA
  #define WIFI_AP WIFI_MODE_AP
  #define WIFI_AP_STA WIFI_MODE_APSTA  // AP+STA: needed for ESP-NOW send on main device

// Dual-band (5 GHz capable) Wi-Fi support.
// Enabled with -D DUAL_BAND_WIFI=true for boards with a 5 GHz radio (ESP32-C5).
// It only selects which bands the radio may use; the rest of the code is shared.
#ifndef DUAL_BAND_WIFI
  #define DUAL_BAND_WIFI 0
#endif

// Channel used when EEPROM holds no valid channel yet (first boot / erased NVS).
// Boards with a 5 GHz radio may set -D DEFAULT_AP_CHANNEL=36 for a 5 GHz setup AP.
#ifndef DEFAULT_AP_CHANNEL
  #define DEFAULT_AP_CHANNEL 1
#endif

// Valid 5 GHz channel numbers (20 MHz centre frequencies).
// DFS channels cannot be used by the ESP32 SoftAP (IDF rejects them with
// "Configure channel to a DFS channel is not allowed in ap mode").
inline bool wifiChannelIsDfs(int channel) {
  return (channel >= 52 && channel <= 64) || (channel >= 100 && channel <= 144);
}
inline bool wifiChannelIs5GHz(int channel) {
  return (channel >= 36 && channel <= 64 && (channel - 36) % 4 == 0) ||
         (channel >= 100 && channel <= 144 && (channel - 100) % 4 == 0) ||
         (channel >= 149 && channel <= 177 && (channel - 149) % 4 == 0);
}

// 2.4 GHz channels 1-13 are always accepted; 5 GHz only where the radio supports it.
inline bool wifiChannelIsValid(int channel) {
  if (channel >= 1 && channel <= 13) return true;
#if DUAL_BAND_WIFI
  return wifiChannelIs5GHz(channel) && !wifiChannelIsDfs(channel);
#else
  return false;
#endif
}

// Shared Functions
extern void monitorHeapStatus();
extern bool isMemoryAvailableForWebResponse();

#endif