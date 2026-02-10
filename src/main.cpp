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
// bool auxillary = false; //moved to platformio.ini
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
// bool checkit = false;
bool channelChange = false;
bool uploadInProgress = false; // Flag to disable FastLED operations during upload
bool savingToSpiffs = false;
unsigned long previousFlashy = 0;
const long intervalBetweenFlashy = 5;
bool black = true;
bool upDown = true;
bool lines = true;
#define UPDATES_PER_SECOND 30000
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
#ifndef ESP32
String bin = "a.bin";
#else
String bin = "/a.bin";
#endif
int uploadCounter = 1;
bool wifiEventDetect = false;
// bool start = true;
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

  littleFSLoadSettings();
  checkFilesInSetup();
  fastLEDIndicate();
  Udp.begin(LOCAL_PORT);
  setupElegantOTATask(); // Start the OTA task - also Web Server for built-in controls

  // Send check-in to SmartPoi API
  sendSmartPoiCheckin();
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
        if(LittleFS.exists(testBin)) {
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
    if(LittleFS.exists(testBin)) {
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

  currentMillis = millis();

  // if (currentMillis - previousMillis >= interval)
  // {
  //   previousMillis = currentMillis;
  //   state = 1;
  // }

  if (!uploadInProgress)
  {
    switch (pattern)
    {
    case 0:
      // currentMillis2 = millis();
      packetSize = Udp.parsePacket();
      if (packetSize)
      {
        handleUDP();
      }
      break;
    case 1:
      funColourJam();
      break;
    case 2:
      // Ensure imageToUse is within bounds of currentImages
      if (imageToUse >= currentImages.length()) {
        imageToUse = 0;
      }
      bin.setCharAt(1, currentImages.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 3:
      // Ensure imageToUse is within bounds of currentImages
      if (imageToUse >= currentImages.length()) {
        imageToUse = 0;
      }
      bin.setCharAt(1, currentImages.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 4:
      // Ensure imageToUse is within bounds of currentImages
      if (imageToUse >= currentImages.length()) {
        imageToUse = 0;
      }
      bin.setCharAt(1, currentImages.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 5:
      // Ensure imageToUse is within bounds of currentImages
      if (imageToUse >= currentImages.length()) {
        imageToUse = 0;
      }
      bin.setCharAt(1, currentImages.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 7:
      FastLED.showColor(CRGB::Black);
      break;
    case 8:
      bin.setCharAt(1, currentImages.charAt(0));
      showLittleFSImage();
      break;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
      bin.setCharAt(1, currentImages.charAt(0));
      showLittleFSImage();
      break;
    }
    yield();
  }
  else
  {
    yield();
  }
  yield();
}