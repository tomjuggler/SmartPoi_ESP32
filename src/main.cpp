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
// #include "WebServerSetup.h"
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
bool checkit = false;
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

  eepromBrightnessChooser(15);
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
  // webServerSetupLogic(apName, apPass);
  setupElegantOTATask(); // Start the OTA task - todo: is this going to conflict with my own webserver above???

  // Send check-in to SmartPoi API
  sendSmartPoiCheckin();
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
  // handleAllServers();
  // handleDNSServer();
  ChangePatternPeriodically();

  currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    state = 1;
  }

  if (state == 1 && !uploadInProgress)
  {
    switch (pattern)
    {
    case 0:
      currentMillis2 = millis();
      packetSize = Udp.parsePacket();
      if (packetSize)
      {
        handleUDP();
      }
    case 1:
      funColourJam();
      break;
    case 2:
      minImages = 0;
      maxImages = 4;
      bin.setCharAt(1, images.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 3:
      minImages = 5;
      maxImages = 10;
      bin.setCharAt(1, images.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 4:
      minImages = 11;
      maxImages = 20;
      bin.setCharAt(1, images.charAt(imageToUse));
      showLittleFSImage();
      break;
    case 5:
      minImages = 0;
      maxImages = 62;
      bin.setCharAt(1, images.charAt(imageToUse));
      showLittleFSImage();
      break;
    // case 6 handled on startup - switching case. 
    // todo: update logic so this actually works..
    // todo: should this work for individual images? 
    // case 6:
    //   // todo: on/off switch change setting here!
    //   yield();
    //   break;
    case 7:
      Serial.print(">");
      FastLED.delay(100);
      yield();
      break;
    case 8:
      minImages = 0;
      maxImages = 0;
      bin.setCharAt(1, images.charAt(0));
      showLittleFSImage();
      break;
    case 9:
      minImages = 1;
      maxImages = 1;
      bin.setCharAt(1, images.charAt(1));
      showLittleFSImage();
      break;
    case 10:
      minImages = 2;
      maxImages = 2;
      bin.setCharAt(1, images.charAt(2));
      showLittleFSImage();
      break;
    case 11:
      minImages = 3;
      maxImages = 3;
      bin.setCharAt(1, images.charAt(3));
      showLittleFSImage();
      break;
    case 12:
      minImages = 4;
      maxImages = 4;
      bin.setCharAt(1, images.charAt(4));
      showLittleFSImage();
      break;
    case 13:
      minImages = 5;
      maxImages = 5;
      bin.setCharAt(1, images.charAt(5));
      showLittleFSImage();
      break;
    case 14:
      minImages = 6;
      maxImages = 6;
      bin.setCharAt(1, images.charAt(6));
      showLittleFSImage();
      break;
    case 15:
      minImages = 7;
      maxImages = 7;
      bin.setCharAt(1, images.charAt(7));
      showLittleFSImage();
      break;
    case 16:
      minImages = 8;
      maxImages = 8;
      bin.setCharAt(1, images.charAt(8));
      showLittleFSImage();
      break;
    case 17:
      minImages = 9;
      maxImages = 9;
      bin.setCharAt(1, images.charAt(9));
      showLittleFSImage();
      break;
    case 18:
      minImages = 10;
      maxImages = 10;
      bin.setCharAt(1, images.charAt(10));
      showLittleFSImage();
      break;
    case 19:
      minImages = 11;
      maxImages = 11;
      bin.setCharAt(1, images.charAt(11));
      showLittleFSImage();
      break;
    case 20:
      minImages = 12;
      maxImages = 12;
      bin.setCharAt(1, images.charAt(12));
      showLittleFSImage();
      break;
    case 21:
      minImages = 13;
      maxImages = 13;
      bin.setCharAt(1, images.charAt(13));
      showLittleFSImage();
      break;
    case 22:
      minImages = 14;
      maxImages = 14;
      bin.setCharAt(1, images.charAt(14));
      showLittleFSImage();
      break;
    case 23:
      minImages = 15;
      maxImages = 15;
      bin.setCharAt(1, images.charAt(15));
      showLittleFSImage();
      break;
    case 24:
      minImages = 16;
      maxImages = 16;
      bin.setCharAt(1, images.charAt(16));
      showLittleFSImage();
      break;
    case 25:
      minImages = 17;
      maxImages = 17;
      bin.setCharAt(1, images.charAt(17));
      showLittleFSImage();
      break;
    case 26:
      minImages = 18;
      maxImages = 18;
      bin.setCharAt(1, images.charAt(18));
      showLittleFSImage();
      break;
    case 27:
      minImages = 19;
      maxImages = 19;
      bin.setCharAt(1, images.charAt(19));
      showLittleFSImage();
      break;
    case 28:
      minImages = 20;
      maxImages = 20;
      bin.setCharAt(1, images.charAt(20));
      showLittleFSImage();
      break;
    case 29:
      minImages = 21;
      maxImages = 21;
      bin.setCharAt(1, images.charAt(21));
      showLittleFSImage();
      break;
    case 30:
      minImages = 22;
      maxImages = 22;
      bin.setCharAt(1, images.charAt(22));
      showLittleFSImage();
      break;
    case 31:
      minImages = 23;
      maxImages = 23;
      bin.setCharAt(1, images.charAt(23));
      showLittleFSImage();
      break;
    case 32:
      minImages = 24;
      maxImages = 24;
      bin.setCharAt(1, images.charAt(24));
      showLittleFSImage();
      break;
    case 33:
      minImages = 25;
      maxImages = 25;
      bin.setCharAt(1, images.charAt(25));
      showLittleFSImage();
      break;
    case 34:
      minImages = 26;
      maxImages = 26;
      bin.setCharAt(1, images.charAt(26));
      showLittleFSImage();
      break;
    case 35:
      minImages = 27;
      maxImages = 27;
      bin.setCharAt(1, images.charAt(27));
      showLittleFSImage();
      break;
    case 36:
      minImages = 28;
      maxImages = 28;
      bin.setCharAt(1, images.charAt(28));
      showLittleFSImage();
      break;
    case 37:
      minImages = 29;
      maxImages = 29;
      bin.setCharAt(1, images.charAt(29));
      showLittleFSImage();
      break;
    case 38:
      minImages = 30;
      maxImages = 30;
      bin.setCharAt(1, images.charAt(30));
      showLittleFSImage();
      break;
    case 39:
      minImages = 31;
      maxImages = 31;
      bin.setCharAt(1, images.charAt(31));
      showLittleFSImage();
      break;
    case 40:
      minImages = 32;
      maxImages = 32;
      bin.setCharAt(1, images.charAt(32));
      showLittleFSImage();
      break;
    case 41:
      minImages = 33;
      maxImages = 33;
      bin.setCharAt(1, images.charAt(33));
      showLittleFSImage();
      break;
    case 42:
      minImages = 34;
      maxImages = 34;
      bin.setCharAt(1, images.charAt(34));
      showLittleFSImage();
      break;
    case 43:
      minImages = 35;
      maxImages = 35;
      bin.setCharAt(1, images.charAt(35));
      showLittleFSImage();
      break;
    case 44:
      minImages = 36;
      maxImages = 36;
      bin.setCharAt(1, images.charAt(36));
      showLittleFSImage();
      break;
    case 45:
      minImages = 37;
      maxImages = 37;
      bin.setCharAt(1, images.charAt(37));
      showLittleFSImage();
      break;
    case 46:
      minImages = 38;
      maxImages = 38;
      bin.setCharAt(1, images.charAt(38));
      showLittleFSImage();
      break;
    case 47:
      minImages = 39;
      maxImages = 39;
      bin.setCharAt(1, images.charAt(39));
      showLittleFSImage();
      break;
    case 48:
      minImages = 40;
      maxImages = 40;
      bin.setCharAt(1, images.charAt(40));
      showLittleFSImage();
      break;
    case 49:
      minImages = 41;
      maxImages = 41;
      bin.setCharAt(1, images.charAt(41));
      showLittleFSImage();
      break;
    case 50:
      minImages = 42;
      maxImages = 42;
      bin.setCharAt(1, images.charAt(42));
      showLittleFSImage();
      break;
    case 51:
      minImages = 43;
      maxImages = 43;
      bin.setCharAt(1, images.charAt(43));
      showLittleFSImage();
      break;
    case 52:
      minImages = 44;
      maxImages = 44;
      bin.setCharAt(1, images.charAt(44));
      showLittleFSImage();
      break;
    case 53:
      minImages = 45;
      maxImages = 45;
      bin.setCharAt(1, images.charAt(45));
      showLittleFSImage();
      break;
    case 54:
      minImages = 46;
      maxImages = 46;
      bin.setCharAt(1, images.charAt(46));
      showLittleFSImage();
      break;
    case 55:
      minImages = 47;
      maxImages = 47;
      bin.setCharAt(1, images.charAt(47));
      showLittleFSImage();
      break;
    case 56:
      minImages = 48;
      maxImages = 48;
      bin.setCharAt(1, images.charAt(48));
      showLittleFSImage();
      break;
    case 57:
      minImages = 49;
      maxImages = 49;
      bin.setCharAt(1, images.charAt(49));
      showLittleFSImage();
      break;
    case 58:
      minImages = 50;
      maxImages = 50;
      bin.setCharAt(1, images.charAt(50));
      showLittleFSImage();
      break;
    case 59:
      minImages = 51;
      maxImages = 51;
      bin.setCharAt(1, images.charAt(51));
      showLittleFSImage();
      break;
    case 60:
      minImages = 52;
      maxImages = 52;
      bin.setCharAt(1, images.charAt(52));
      showLittleFSImage();
      break;
    case 61:
      minImages = 53;
      maxImages = 53;
      bin.setCharAt(1, images.charAt(53));
      showLittleFSImage();
      break;
    case 62:
      minImages = 54;
      maxImages = 54;
      bin.setCharAt(1, images.charAt(54));
      showLittleFSImage();
      break;
    case 63:
      minImages = 55;
      maxImages = 55;
      bin.setCharAt(1, images.charAt(55));
      showLittleFSImage();
      break;
    case 64:
      minImages = 56;
      maxImages = 56;
      bin.setCharAt(1, images.charAt(56));
      showLittleFSImage();
      break;
    case 65:
      minImages = 57;
      maxImages = 57;
      bin.setCharAt(1, images.charAt(57));
      showLittleFSImage();
      break;
    case 66:
      minImages = 58;
      maxImages = 58;
      bin.setCharAt(1, images.charAt(58));
      showLittleFSImage();
      break;
    case 67:
      minImages = 59;
      maxImages = 59;
      bin.setCharAt(1, images.charAt(59));
      showLittleFSImage();
      break;
    case 68:
      minImages = 60;
      maxImages = 60;
      bin.setCharAt(1, images.charAt(60));
      showLittleFSImage();
      break;
    case 69:
      minImages = 61;
      maxImages = 61;
      bin.setCharAt(1, images.charAt(61));
      showLittleFSImage();
      break;
    default:
      yield();
    }
    yield();
  }
  else
  {
    yield();
  }
  yield();
}

// Add all remaining function implementations here...
