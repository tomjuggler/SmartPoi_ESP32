#include "UDPHandler.h"
#include "Globals.h"
#include <EEPROM.h>  // Required for EEPROM operations

// UDP Handler Variables
extern unsigned long currentMillis2;
extern unsigned long previousMillis2;
constexpr int interval = 1000;  // 1 second default interval
extern int len;
extern uint8_t packetBuffer[255];
extern uint8_t Y;
extern int state;
extern int X;
extern uint8_t R1;
extern uint8_t G1;
extern uint8_t M1;
extern bool channelChange;

void handleUDP() {
    static unsigned long lastPacketTime = 0;
    
    
    len = Udp.read(packetBuffer, 255);
    if (len > 0) {
        packetBuffer[len] = 0;
        lastPacketTime = currentMillis2;
    } 

    for (int i = 0; i < NUM_PX; i++) {
        X = packetBuffer[i] - 127;
        R1 = (X & 0xE0);
        leds[i].r = R1;
        G1 = ((X << 3) & 0xE0);
        leds[i].g = G1;
        M1 = (X << 6);
        leds[i].b = M1;
    }
    FastLED.show();
    yield();
}
