#ifndef GLOBALS_H
#define GLOBALS_H

// Platform detection
#if defined(ESP32)
  #define PLATFORM_ESP32
#else
  #error "Unsupported platform - Support ESP32 C3 or S3 only!"
#endif

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <FastLED.h>


// #include <WiFi.h>
// #include <DNSServer.h>
// #include <WebServer.h>
// #include <EEPROM.h>
// #include <WiFiMulti.h>
// // extern WebServer poiserver;
// extern WiFiMulti WiFiMulti;
// extern DNSServer dnsServer;

// Configuration Constants
constexpr int NUM_LEDS = NUMLEDS; //from platformio.ini
constexpr int NUM_PX = NUMPX; //from platformio.ini
constexpr int DNS_PORT = 53;
constexpr unsigned int LOCAL_PORT = 2390;
// constexpr int MAX_PX = 12240; //moved to platformio.ini
constexpr int MAX_PX = MAXPX;
constexpr int DEFAULT_BRIGHTNESS = 20;

constexpr int DATA_PIN = DATAPIN;
constexpr int CLOCK_PIN = CLOCKPIN;

// Global Extern Variables
extern CRGB leds[NUM_LEDS];
extern WiFiUDP Udp;
extern int newBrightness;  // Declaration for brightness control variable
extern bool routerOption;  // Declaration for router configuration flag
extern int wifiModeChooser;  // Declaration for WiFi mode selection
extern int patternChooser;
extern int pattern;
extern int apChannel;
extern int imageToUse;
extern int minImages;
extern int maxImages;
extern unsigned long previousMillis3;
extern uint8_t addrNumA;
extern uint8_t addrNumB;
extern uint8_t addrNumC;
extern uint8_t addrNumD;
// extern bool auxillary; //moved to platformio.ini
extern File settings;
extern String Field;
extern size_t maxPX;
extern IPAddress apIP;
// UDP Handler variables
extern unsigned long currentMillis2;
extern unsigned long previousMillis2;
extern bool checkit;
extern int len;
extern uint8_t packetBuffer[255];
extern uint8_t Y;
extern int state;
extern int X;
extern uint8_t R1;
extern uint8_t G1;
extern uint8_t M1;
extern bool channelChange;
extern bool uploadInProgress;  // Flag to disable FastLED operations during upload
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

// Function declarations
extern bool updateCurrentImagesForPattern(int pattern); 

// WiFi Mode Constants
  #define WIFI_STA WIFI_MODE_STA
  #define WIFI_AP WIFI_MODE_AP

// Shared Functions
// void fastLEDInit();
// void fastLEDIndicate();
// void fastLEDIndicateFast();

#endif