#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Initialize.h"
#include "Globals.h"
#include "UDPHandler.h"
#include "ColourPalette.h"
#include "ShowLittleFSImage.h"
#include "TimeFunc.h"
#include "tasks.h"
#include <esp_task_wdt.h>

// Global Variable Definitions
CRGB leds[NUM_LEDS];
WiFiUDP Udp;
AsyncWebServer server(80); // Create the web server instance

// Additional global variables
File fsUploadFile;
File f;
File a;
File settings;
// UDP Handler variables
unsigned long currentMillis2 = 0;
int state = 0;
uint8_t packetBuffer[255] = {0};
int len = 0;
uint8_t Y = 0;
int X = 0;
uint8_t R1 = 0;
uint8_t G1 = 0;
uint8_t M1 = 0;
int newBrightness = DEFAULT_BRIGHTNESS;
int targetBrightness = DEFAULT_BRIGHTNESS;
uint8_t message1Data[MAX_PX];
int pxDown = NUM_PX;
int pxAcross = pxDown;
IPAddress apIP(192, 168, 1, 1);
IPAddress apIPauxillary(192, 168, 1, 78);
int status = WiFi.status();
const char *apName = "Smart_Poi9";
const char *apPass = "SmartOne";
int apChannel = 1;
IPAddress ipSubnet(255, 255, 255, 0);
IPAddress ipGateway(192, 168, 8, 1);
IPAddress ipGatewayauxillary(192, 168, 1, 1);
IPAddress ip(192, 168, 8, 77);
uint8_t addrNumA = 192;
uint8_t addrNumB = 168;
uint8_t addrNumC = 8;
uint8_t addrNumD = 78;
String responseHTML;
String content;
int statusCode;
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long previousMillis3 = 0;
long interval = 5000;
bool channelChange = false;
bool uploadInProgress = false; // Flag to disable FastLED operations during upload
bool savingToSpiffs = false;
unsigned long previousFlashy = 0;
const long intervalBetweenFlashy = 5;
bool black = true;
bool upDown = true;
bool lines = true;
int motionSpeed = 1;
int maxStartIndex = 70;
int minStartIndex = 0;
volatile int setting = 2;
int patternChooser = 2;
int pattern = 2;
int wifiModeChooser = 1;
int imageChooser = 1;
bool preloaded = false;
int byteCounter = 0;
IPAddress tmpGateway(192, 168, 8, 1);
IPAddress tmpIP(192, 168, 8, 77);
String Field;
int imageToUse = 0;
int maxImages = 52;
int minImages = 0;
String images = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
String images2 = "abcde";
String images3 = "fghij";
String images4 = "klmnopqrst";
String images5 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
String currentImages = images;
String bin = "/a.bin";

// FreeRTOS Mutex Semaphores (for thread safety)
SemaphoreHandle_t bufferMutex = NULL;  // Protects message1Data array in RAM
SemaphoreHandle_t diskMutex = NULL;    // Protects SPI Flash bus

// FreeRTOS Task Handles
TaskHandle_t povTaskHandle = NULL;
TaskHandle_t fileTaskHandle = NULL;
TaskHandle_t webTaskHandle = NULL;

// Cache for pattern file existence to avoid repeated LittleFS.exists() calls
// that generate error messages for non-existent files
bool patternFileExistsCache[62]; // 26 lowercase + 26 uppercase + 10 digits = 62
bool cacheInitialized = false;

void initializePatternFileCache() {
  if (cacheInitialized) return;
  
  Serial.println("Initializing pattern file cache...");
  
  // Check each possible pattern file once and cache the result
  for (int i = 0; i < 62; i++) {
    String testBin = bin;
    testBin.setCharAt(1, images.charAt(i));
    patternFileExistsCache[i] = LittleFS.exists(testBin);
    
    // Small delay to prevent overwhelming the filesystem
    delay(1);
  }
  
  cacheInitialized = true;
  Serial.println("Pattern file cache initialized");
}

// Refresh a specific cache entry when a file changes
void refreshPatternFileCacheEntry(char patternChar) {
  int index = images.indexOf(patternChar);
  if (index == -1) return; // Character not in images string
  
  String testBin = bin;
  testBin.setCharAt(1, patternChar);
  patternFileExistsCache[index] = LittleFS.exists(testBin);
}

// Refresh entire cache (use sparingly)
void refreshPatternFileCache() {
  Serial.println("Refreshing pattern file cache...");
  for (int i = 0; i < 62; i++) {
    String testBin = bin;
    testBin.setCharAt(1, images.charAt(i));
    patternFileExistsCache[i] = LittleFS.exists(testBin);
    delay(1);
  }
  Serial.println("Pattern file cache refreshed");
}

bool checkPatternFileExists(char patternChar) {
  int index = images.indexOf(patternChar);
  if (index == -1) return false; // Character not in images string
  if (!cacheInitialized) {
  }
  
  return patternFileExistsCache[index];
}
// POV Display Task - High priority task for microsecond-accurate LED timing
void povDisplayTask(void *pvParameters) {
  Serial.println("POV display task started");
  
  // Task watchdog reset
  esp_task_wdt_reset();
  
  // Stack monitoring for debugging
  UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("POV task stack high water mark: %u\n", stackHighWaterMark);
  // Column delay for POV timing (adjust based on required refresh rate)
  const int columnDelay = 1000; // microseconds - adjust as needed
  
  while (1) {
    // Reset task watchdog
    esp_task_wdt_reset();
    
    // Only run POV display if not in upload mode and pattern requires image display
    if (!uploadInProgress && pattern >= 2 && pattern <= 69) {
      // Extract character from bin string (format: "/x.bin" where x is the character)
      char patternChar = bin.charAt(1);
      
      // Check if we need to display an image
      if (pattern >= 2 && pattern <= 5) {
        // For patterns 2-5, use currentImages sequence
        if (imageToUse >= currentImages.length()) {
          imageToUse = 0;
        }
        patternChar = currentImages.charAt(imageToUse);
      } else if (pattern >= 8 && pattern <= 69) {
        // For patterns 8-69, use single character pattern
        patternChar = currentImages.charAt(0);
      }
      
      // Update bin string with current character
      bin.setCharAt(1, patternChar);
      
      // Shadow buffer for local copy to minimize mutex hold time
      uint8_t shadowBuffer[MAX_PX];
      int localPxAcross = pxAcross;
      int localPxDown = pxDown;
      // Get buffer mutex to safely copy data
      if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Copy data to shadow buffer
        memcpy(shadowBuffer, message1Data, MAX_PX);
        xSemaphoreGive(bufferMutex);
        
        // Initialize counter for reading pixel data
        int counter = 0;
        
        // Loop through the pixels to display the image with microsecond timing
        for (int j = 0; j < localPxAcross; j++) {
          // Decompress and display column
          for (int i = 0; i < localPxDown; i++) {
            byte X = shadowBuffer[counter++]; // Get pixel data from shadow buffer
            
            // Decompress 3-3-2 bit-packed format
            leds[i].r = (X & 0xE0);          // Red: bits 7-5
            leds[i].g = ((X << 3) & 0xE0);   // Green: bits 4-2
            leds[i].b = (X << 6);            // Blue: bits 1-0
          }
          
          // Display the current column of pixels on the LED strip
          FastLED.show();
          
          // Microsecond delay for POV timing
          ets_delay_us(columnDelay);
          
          // Yield briefly to prevent watchdog timeout
          if (j % 10 == 0) {
            esp_task_wdt_reset();
          }
        }
      }
    }
    
    // Small delay to yield CPU to other tasks
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
// File Reader Task - Background task for LittleFS operations
void fileReaderTask(void *pvParameters) {
  Serial.println("File reader task started");
  
  // Stack monitoring for debugging
  UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("File task stack high water mark: %u\n", stackHighWaterMark);
  
  // Track last loaded image to avoid unnecessary reloads
  char lastLoadedImage = '\0';
  bool needsReload = true;
  while (1) {
    // Only process if not in upload mode
    if (!uploadInProgress) {
      // Determine which image to load based on current pattern
      char targetImage = '\0';
      
      if (pattern >= 2 && pattern <= 5) {
        // For patterns 2-5, use currentImages sequence
        if (imageToUse >= currentImages.length()) {
          imageToUse = 0;
        }
        targetImage = currentImages.charAt(imageToUse);
      } else if (pattern >= 8 && pattern <= 69) {
        // For patterns 8-69, use single character pattern
        targetImage = currentImages.charAt(0);
      }
      
      // Check if we need to load a new image
      if (targetImage != '\0' && (needsReload || targetImage != lastLoadedImage)) {
        // Update bin string
        bin.setCharAt(1, targetImage);
        
        // Check if file exists
        if (checkPatternFileExists(targetImage)) {
          // Get disk mutex to protect SPI Flash bus
          if (xSemaphoreTake(diskMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Open file for reading
            a = LittleFS.open(bin, "r");
            
            if (a && a.available()) {
              size_t size = a.size();
              
              // Check if the image size is valid
              if (size > 0 && size <= MAX_PX) {
                // Calculate pixels across
                int newPxAcross = int(size / pxDown);
                
                // Get buffer mutex to protect RAM buffer
                if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                  // Read image data into buffer
                  a.read(message1Data, size);
                  pxAcross = newPxAcross;
                  xSemaphoreGive(bufferMutex);
                  
                  // Update tracking variables
                  lastLoadedImage = targetImage;
                  needsReload = false;
                  
                  Serial.printf("Loaded image %c (%d bytes)\n", targetImage, size);
                }
              }
              
              // Close the file
              a.close();
            }
            
            xSemaphoreGive(diskMutex);
          }
        } else {
          // File doesn't exist, mark for reload
          needsReload = true;
          Serial.printf("Image %c not found\n", targetImage);
        }
      }
    }
    
    // Delay to yield CPU to other tasks
    vTaskDelay(pdMS_TO_TICKS(100)); // Check for new files every 100ms
  }
}

int uploadCounter = 1;
bool routerOption = false;
volatile unsigned long currentMillis = millis();
volatile int packetSize;

void setup()
{
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(DATA_PIN, LOW);
  fastLEDInit();
  fastLEDIndicateFast();
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Started");

  EEPROM.begin(512);

  // eepromBrightnessChooser(15); //setting brightness to 20 for startup now. 
  eepromRouterOptionChooser(100);
  eepromWifiModeChooser(5);
  eepromPatternChooser(10);
  eepromReadChannelAndAddress(13, 14, 16, 17, 18);
  EEPROM.commit();

  bool result = LittleFS.begin(true);
  
  // Initialize pattern file cache to avoid repeated LittleFS.exists() calls
  initializePatternFileCache();
  
  littleFSLoadSettings();
  fastLEDIndicate();
  Udp.begin(LOCAL_PORT);
  setupElegantOTATask(); // Start the OTA task - also Web Server for built-in controls

  // Send check-in to SmartPoi API
  sendSmartPoiCheckin();
  
  // Initialize FreeRTOS mutex semaphores for thread safety
  bufferMutex = xSemaphoreCreateMutex();
  if (bufferMutex == NULL) {
    Serial.println("ERROR: Failed to create bufferMutex!");
  } else {
    Serial.println("bufferMutex created successfully");
  }
  
  diskMutex = xSemaphoreCreateMutex();
  if (diskMutex == NULL) {
    Serial.println("ERROR: Failed to create diskMutex!");
  } else {
    Serial.println("diskMutex created successfully");
  }
  
  // Create FreeRTOS tasks
  xTaskCreate(povDisplayTask, "POV_TASK", POV_TASK_STACK_SIZE, NULL, POV_TASK_PRIO, &povTaskHandle);
  if (povTaskHandle == NULL) {
    Serial.println("ERROR: Failed to create POV display task!");
  } else {
    Serial.println("POV display task created successfully");
  }
  
  xTaskCreate(fileReaderTask, "FILE_TASK", FILE_TASK_STACK_SIZE, NULL, FILE_TASK_PRIO, &fileTaskHandle);
  if (fileTaskHandle == NULL) {
    Serial.println("ERROR: Failed to create file reader task!");
  } else {
    Serial.println("File reader task created successfully");
  }
  
  // Note: Web server task is created in setupElegantOTATask() in tasks.cpp
  // using xTaskCreatePinnedToCore with WEB_TASK_PRIO
}

bool updateCurrentImagesForPattern(int pattern) {
  String tempImages;

  switch(pattern) {
    case 2:
      tempImages = images2;
      break;
    case 3:
      tempImages = images3;
      break;
    case 4:
      tempImages = images4;
      break;
    case 5:
      tempImages = images5;
      break;
    default:
      // For patterns 8+, use single character from images
      if(pattern >= 8 && pattern <= 69) {
        tempImages = String(images.charAt(pattern - 8));
        // Check if the file actually exists
        String testBin = bin;
        testBin.setCharAt(1, tempImages.charAt(0));
        if(checkPatternFileExists(tempImages.charAt(0))) {
          // For single character patterns, set min/max to 0
          currentImages = tempImages;
          minImages = 0;
          maxImages = 0;
          return true;
        } else {
          // File doesn't exist
          return false;
        }
      } else {
        tempImages = currentImages; // Keep current
        return true;
      }
  }

  // Check if any files exist for this pattern and build filtered list
  bool anyFileExists = false;
  String testBin = bin;
  String filteredImages = "";

  for(int i = 0; i < tempImages.length(); i++) {
    testBin.setCharAt(1, tempImages.charAt(i));
    if(checkPatternFileExists(tempImages.charAt(i))) {
      anyFileExists = true;
      filteredImages += tempImages.charAt(i);
    }
  }

  if(anyFileExists) {
    currentImages = filteredImages;
    minImages = 0;
    maxImages = currentImages.length() - 1;
    // Reset imageToUse if it's out of bounds
    if (imageToUse >= currentImages.length()) {
      imageToUse = 0;
    }
    return true;
  } else {
    // No files available, return false to indicate pattern should be switched
    return false;
  }
}

void sendSmartPoiCheckin()
{
  // Don't send check-in in AP mode (wifiModeChooser == 1)
  if (wifiModeChooser == 1)
  {
    Serial.println("AP mode detected, skipping SmartPoi check-in");
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Begin the HTTP request
    http.begin("https://smartpoifirmware.circusscientist.com/api/smartpoi-checkin");

    // Set timeout to 5000ms to avoid blocking too long
    http.setTimeout(5000);

    // Send GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.println("SmartPoi API Response: " + response);

      // Try to parse JSON response
      try
      {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error)
        {
          const char *status = doc["status"];
          const char *message = doc["message"];
          const char *timestamp = doc["timestamp"];
          const char *ip = doc["ip"];

          Serial.printf("Check-in successful - Status: %s, IP: %s\n", status, ip);
        }
        else
        {
          Serial.println("Failed to parse JSON response");
        }
      }
      catch (...)
      {
        Serial.println("Error parsing JSON response");
      }
    }
    else
    {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi not connected, skipping SmartPoi check-in");
  }
}

void loop()
{
  ChangePatternPeriodically();
  checkBrightness();
  handleDNSServer();
  // monitorHeapStatus(); // Monitor heap usage every loop iteration
  
  currentMillis = millis();


  if (!uploadInProgress)
  {
    switch (pattern)
    {
    case 0:
      packetSize = Udp.parsePacket();
      if (packetSize)
      {
        handleUDP();
      }
      break;
    case 1:
      funColourJam();
      break;
    case 7:
      FastLED.showColor(CRGB::Black);
      break;
    }
    yield();
  }
  else
  {
    yield();
  }
  yield();
  
  // Yield CPU to other tasks with proper vTaskDelay
  vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay to yield CPU
}