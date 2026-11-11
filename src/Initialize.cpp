#include "Initialize.h"
#include "Globals.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiMulti.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <ctype.h>

WiFiMulti WiFiMulti;
DNSServer dnsServer;


void eepromBrightnessChooser(int addr)
{
    int readBRTeprom = EEPROM.read(addr);
    newBrightness = readBRTeprom;
    if (newBrightness > 255 || newBrightness < 20)
    {
        EEPROM.write(15, DEFAULT_BRIGHTNESS);
        newBrightness = DEFAULT_BRIGHTNESS;
    }
    targetBrightness = newBrightness;  // Set target to same value for immediate effect
    FastLED.setBrightness(newBrightness);
    FastLED.showColor(CRGB::Black);
}

void eepromRouterOptionChooser(int addr)
{
    int newRouter = EEPROM.read(addr);
    routerOption = (newRouter == 1);
    EEPROM.write(100, newRouter);
}

void eepromWifiModeChooser(int addr)
{
    if (routerOption)
    {
        wifiModeChooser = EEPROM.read(addr);
        wifiModeChooser++;
        if (wifiModeChooser > 2)
        {
            wifiModeChooser = 1;
        }
        EEPROM.write(5, wifiModeChooser);
    }
    else
    {
        wifiModeChooser = 1;
    }
}

// Restore the selected pattern at boot. Patterns 1-70 are remembered, with
// pattern 6 acting as a boot-rotation mode. Pattern 0 (UDP) and 7 (LEDs off)
// are runtime-only states that look like a fault after a reboot, so they - and
// any invalid value - fall back to pattern 1.
void eepromPatternChooser(int addr)
{
    int stored = EEPROM.read(addr);
    bool pattern6Mode = EEPROM.read(EEPROM_ADDR_PAT6) == 1;

    // Migration: older firmware used EEPROM[10] == 6 without the flag.
    if (stored == 6)
    {
        pattern6Mode = true;
        EEPROM.write(EEPROM_ADDR_PAT6, 1);
    }

    // 0 and 7 are runtime-only; never let a stale flag override them.
    if (stored == 0 || stored == 7 || stored > 70)
    {
        stored = 1;
        pattern6Mode = false;
        EEPROM.write(EEPROM_ADDR_PAT6, 0);
    }

    if (pattern6Mode)
    {
        patternChooser = 6;
        pattern = advanceSequencePattern();
        EEPROM.write(10, 6);
    }
    else
    {
        patternChooser = stored;
        pattern = stored;
        EEPROM.write(10, stored);
    }

    #if SERIAL_DEBUG
    Serial.printf("[PATTERN] restored %d (chooser %d, pat6 %d)\n",
                  pattern, patternChooser, pattern6Mode ? 1 : 0);
    #endif
}

// Pattern 6 rotation: 0 after selection, then 1 -> 2 -> ... -> 5 -> 1 on boot.
int advanceSequencePattern()
{
    int sequence = EEPROM.read(11);
    sequence++;
    if (sequence < 1 || sequence > 5)
    {
        sequence = 1;
    }
    EEPROM.write(11, sequence);
    return sequence;
}

void setPattern6Mode(bool enabled)
{
    EEPROM.write(EEPROM_ADDR_PAT6, enabled ? 1 : 0);
}
// Called after LittleFS and the pattern-file cache are ready. Populates
// currentImages for the restored image pattern and falls back to pattern 1 if
// its files have disappeared (e.g. after a filesystem update). Without this the
// POV task can run with an empty image list and look like the LEDs are off.
void applyStoredPattern()
{
    if (pattern >= 2 && pattern <= 69)
    {
        if (!updateCurrentImagesForPattern(pattern))
        {
            pattern = 1;
            patternChooser = 1;
            EEPROM.write(10, 1);
            EEPROM.write(11, 1);
            EEPROM.commit();
        }
    }
}

void eepromReadChannelAndAddress(int addr1, int addr2, int addr3, int addr4, int addr5)
{
    apChannel = EEPROM.read(addr1);
    if (!wifiChannelIsValid(apChannel))
    {
        apChannel = wifiChannelIsValid(DEFAULT_AP_CHANNEL) ? DEFAULT_AP_CHANNEL : 1;
        EEPROM.write(13, apChannel);
    }
    addrNumD = EEPROM.read(addr2);
    addrNumA = EEPROM.read(addr3);
    addrNumB = EEPROM.read(addr4);
    addrNumC = EEPROM.read(addr5);
}

// No settings.txt: start the setup AP anyway. Besides making the web UI
// reachable, this brings up the network stack that Udp.begin() relies on
// (on newer Arduino cores a socket cannot be opened before Wi-Fi is started).
static void startDefaultWifi()
{
    wifiModeChooser = 1;  // AP mode - there are no router credentials to use
    char emptySsid[1] = { 0 };
    char emptyPwd[1] = { 0 };
    wifiChooser(emptySsid, emptyPwd);
}
void littleFSLoadSettings()
{
  // Check if file exists before opening to avoid framework error logs
  if (LittleFS.exists("/settings.txt")) {
    settings = LittleFS.open("/settings.txt", "r");

    if (settings) {
      // File exists, read settings
      Field = settings.readStringUntil('\n');
      char router_array[Field.length() + 1];
      Field.toCharArray(router_array, Field.length() + 1);

      Field = settings.readStringUntil('\n');
      char pwd_array[Field.length() + 1];
      Field.toCharArray(pwd_array, Field.length() + 1);

      settings.close();
      wifiChooser(router_array, pwd_array);
    } else {
      #if SERIAL_DEBUG
      Serial.println("Failed to open settings.txt, using default WiFi settings");
      #endif
      startDefaultWifi();
    }
  } else {
    // File doesn't exist, use default values
    #if SERIAL_DEBUG
    Serial.println("settings.txt not found, using default WiFi settings");
    #endif
    startDefaultWifi();
  }
}

void checkFilesInSetup()
{
    File root = LittleFS.open("/");
    if (!root)
    {
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        size_t fileSize = file.size();
        if (fileSize > MAX_PX)
        {
            LittleFS.remove(file.name());
        }
        else
        {
            uint8_t buffer[10];
            if (file.read(buffer, sizeof(buffer)) != sizeof(buffer))
            {
                LittleFS.remove(file.name());
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

// Regulatory domain. A 2-letter ISO code can come from EEPROM (set through
// /setting?country=XX) or from -D WIFI_COUNTRY=\"XX\". ESP-IDF's default "01"
// (world) blocks 5 GHz AP channels above 48.
static bool countryCodeLooksValid(const char* cc)
{
    if (!cc || !cc[0] || !cc[1] || cc[2] != '\0') return false;
    if (cc[0] == '0' && cc[1] == '1') return true;  // IDF world domain
    return isalpha((unsigned char)cc[0]) && isalpha((unsigned char)cc[1]);
}

bool wifiSetCountryCode(const char* cc)
{
    if (!countryCodeLooksValid(cc)) return false;
#if DUAL_BAND_WIFI
    char upper[3];
    upper[0] = (char)toupper((unsigned char)cc[0]);
    upper[1] = (char)toupper((unsigned char)cc[1]);
    upper[2] = '\0';
    esp_err_t err = esp_wifi_set_country_code(upper, false);
    #if SERIAL_DEBUG
    if (err != ESP_OK) {
        Serial.printf("WiFi: could not set country %s (%d)\n", upper, err);
    }
    #endif
#if SERIAL_DEBUG
    {
        wifi_country_t info;
        if (esp_wifi_get_country(&info) == ESP_OK) {
            Serial.printf("[WIFI] country=%c%c 2g=%u-%u 5gmask=0x%08x\n", info.cc[0], info.cc[1],
                          info.schan, (unsigned)(info.schan + info.nchan - 1),
                          (unsigned)info.wifi_5g_channel_mask);
        }
    }
#endif
    return err == ESP_OK;
#else
    (void)cc;
    return false;  // country selection only matters on 5 GHz capable boards
#endif
}

static void wifiApplyCountry()
{
    char stored[3] = { 0 };
    stored[0] = (char)EEPROM.read(EEPROM_ADDR_COUNTRY);
    stored[1] = (char)EEPROM.read(EEPROM_ADDR_COUNTRY + 1);
    if (countryCodeLooksValid(stored)) {
        wifiSetCountryCode(stored);
        return;
    }
#ifdef WIFI_COUNTRY
    wifiSetCountryCode(WIFI_COUNTRY);
#endif
}

// True when the channel is usable as an AP channel under the current regulatory
// domain (2.4 GHz range from the country table, 5 GHz bitmask/DFS rules).
bool wifiChannelAllowedByCountry(int channel)
{
    if (!wifiChannelIsValid(channel)) return false;
#if DUAL_BAND_WIFI
    wifi_country_t country;
    if (esp_wifi_get_country(&country) != ESP_OK) return true;
    if (channel <= 14) {
        return channel >= country.schan && channel < (int)country.schan + country.nchan;
    }
    if (wifiChannelIsDfs(channel)) return false;  // never usable for SoftAP
    if (country.wifi_5g_channel_mask != 0) {
        return (CHANNEL_TO_BIT(channel) & country.wifi_5g_channel_mask) != 0;
    }
    return true;
#else
    return true;
#endif
}
void wifiChooser(char router_array[], char pwd_array[])
{
    #if C_THREE
        WiFi.setTxPower(WIFI_POWER_8_5dBm);  // for ESP32 C3 Super Mini - helps WiFi be more reliable! 
    #endif
    if (wifiModeChooser == 1)
    {
        if (auxillary)
        {
            WiFi.mode(WIFI_STA);
            wifiApplyCountry();
#if DUAL_BAND_WIFI
            // Auxiliary joins the main poi's AP, which may live on either band.
            WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
#endif
            WiFi.begin(apName, apPass);
            WiFi.config(apIPauxillary, ipGatewayauxillary, ipSubnet, ipGatewayauxillary);
        }
        else
        {
            WiFi.mode(WIFI_AP_STA);  // AP+STA: AP serves web UI, idle STA enables ESP-NOW send
            wifiApplyCountry();
            // The stored channel may not be usable in this regulatory domain.
            if (!wifiChannelAllowedByCountry(apChannel))
            {
                apChannel = wifiChannelAllowedByCountry(DEFAULT_AP_CHANNEL) ? DEFAULT_AP_CHANNEL : 1;
                EEPROM.write(EEPROM_ADDR_CHANNEL, apChannel);
                EEPROM.commit();
            }
#if DUAL_BAND_WIFI
            // A 5 GHz channel can only be used together with 5 GHz band support.
            WiFi.setBandMode(wifiChannelIs5GHz(apChannel) ? WIFI_BAND_MODE_5G_ONLY
                                                          : WIFI_BAND_MODE_AUTO);
#endif
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
            bool apStarted = WiFi.softAP(apName, apPass, apChannel);
#if DUAL_BAND_WIFI
            if (!apStarted)
            {
                // The requested 5 GHz channel is not usable here (regulatory/DFS):
                // fall back to the safe 2.4 GHz default instead of starting no AP.
                WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
                apChannel = 1;
                apStarted = WiFi.softAP(apName, apPass, apChannel);
            }
#endif
            #if SERIAL_DEBUG
            if (!apStarted) Serial.println("WiFi: AP failed to start");
            #endif
            dnsServer.start(DNS_PORT, "*", apIP);
            #if SERIAL_DEBUG
            Serial.printf("AP '%s' on channel %d (%s)\n", apName, apChannel,
                          wifiChannelIs5GHz(apChannel) ? "5 GHz" : "2.4 GHz");
            #endif
        }
    }
    else
    {
        WiFi.mode(WIFI_STA);
        wifiApplyCountry();
#if DUAL_BAND_WIFI
        // Router mode: scan and connect on both 2.4 GHz and 5 GHz.
        WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
#endif
        WiFiMulti.addAP(router_array, pwd_array);
        #if SERIAL_DEBUG
        Serial.print("Connecting to: ");
        Serial.println(router_array);
        Serial.print("Attempt: ");
        #endif
        byte wifiConnectAttemptCount = 0;
        while (WiFiMulti.run() != WL_CONNECTED)
        {
            #if SERIAL_DEBUG
            Serial.print(int(wifiConnectAttemptCount));
            Serial.print(", ");
            #endif
            wifiConnectAttemptCount++;
            // Dual-band scans take noticeably longer than 2.4 GHz only scans.
            if (wifiConnectAttemptCount > (DUAL_BAND_WIFI ? 28 : 18))
            {
                ESP.restart();
            }
            delay(500);
        }
        #if SERIAL_DEBUG
        Serial.printf("Connected on channel %d (%s)\n", WiFi.channel(),
                      WiFi.channel() > 14 ? "5 GHz" : "2.4 GHz");
        #endif
    }
}

void handleDNSServer(){
    dnsServer.processNextRequest();
}

void fastLEDInit()
{

////////////////////////////////////////////////Fast LED Setup: ////////////////////////////////////////////////////////////////////////////////////////////////
// APA102 or WS2812 now defined in platformio.ini. #define LED_APA102=false to change to WS2812

#if LED_APA102
    // For APA102/SK9822 LEDs
    FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
#else
    // For WS2812/NeoPixel LEDs
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
#endif

    FastLED.setBrightness(newBrightness); // should be low figure here, for startup battery saving...
    FastLED.showColor(CRGB::Black);

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    ///////////////////////////////////////////////////////////////////////end FastLED setup///////////////////////////////////////////////////////////////////
}

void fastLEDIndicate()
{
    if (wifiModeChooser == 1)
    {
        CRGB color = auxillary ? CRGB::Red : CRGB::Blue;
        for (int i = 0; i < NUM_LEDS; i++)
        {
            leds[i] = color;
            FastLED.show();
            leds[i] = CRGB::Black;
            FastLED.delay(10);
        }
    }
    else
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            leds[i] = CRGB::Green;
            FastLED.show();
            leds[i] = CRGB::Black;
            FastLED.delay(10);
        }
    }
    FastLED.showColor(CRGB::Black);
}

void fastLEDIndicateFast()
{
    if (wifiModeChooser == 1)
    {
        CRGB color = auxillary ? CRGB::Magenta : CRGB::Cyan;
        for (int i = 0; i < NUM_LEDS; i += 2)
        {
            leds[i] = color;
            FastLED.show();
            leds[i] = CRGB::Black;
            FastLED.delay(15);
        }
    }
    else
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            leds[i] = CRGB::Green;
            FastLED.show();
            leds[i] = CRGB::Black;
            FastLED.delay(15);
        }
    }
    FastLED.showColor(CRGB::Black);
}
