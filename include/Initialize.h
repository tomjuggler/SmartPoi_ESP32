#ifndef INITIALIZE_H
#define INITIALIZE_H

#include "Globals.h"
void eepromBrightnessChooser(int addr);
void eepromRouterOptionChooser(int addr);
void eepromWifiModeChooser(int addr);
void eepromPatternChooser(int addr);
void setPattern6Mode(bool enabled);
int advanceSequencePattern();
void applyStoredPattern();
void eepromReadChannelAndAddress(int addr1, int addr2, int addr3, int addr4, int addr5);
bool wifiSetCountryCode(const char* cc);
bool wifiChannelAllowedByCountry(int channel);
void littleFSLoadSettings();
void checkFilesInSetup();
void wifiChooser(char router_array[], char pwd_array[]);
void handleDNSServer();
void fastLEDInit();
void fastLEDIndicate();
void fastLEDIndicateFast();
void sendSmartPoiCheckin();
#endif
