#include "UDPHandler.h"
#include "Globals.h"
#include <EEPROM.h>  // Required for EEPROM operations

// UDP Handler Variables
extern unsigned long currentMillis2;
extern unsigned long previousMillis2;
extern int len;
extern uint8_t packetBuffer[250];  // ESP-NOW v1.0 max, supports up to 240px
extern uint8_t Y;
extern int state;
extern int X;
extern uint8_t R1;
extern uint8_t G1;
extern uint8_t M1;
extern bool channelChange;

void handleUDP() {
    static unsigned long lastPacketTime = 0;
    
    
    len = Udp.read(packetBuffer, sizeof(packetBuffer));  // 250 bytes — match ESP-NOW v1.0 limit
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

// ============================================================
// ESP-NOW Receive Callback (dual-transport alongside UDP)
// ============================================================
// Called from WiFi task context — must be fast, no blocking I/O
// Receives 3-3-2 bit-packed pixel data identical to UDP format
// ESP-IDF v5.x signature: first param is esp_now_recv_info_t* (not uint8_t*)
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    // Only process if data size matches expected pixel count
    if (len != NUM_PX) return;

    // Decompress 3-3-2 bit-packed format (identical to handleUDP logic)
    for (int i = 0; i < NUM_PX; i++) {
        int X = incomingData[i] - 127;
        leds[i].r = (X & 0xE0);           // Red:   bits 7–5
        leds[i].g = ((X << 3) & 0xE0);    // Green: bits 4–2
        leds[i].b = (X << 6);            // Blue:  bits 1–0
    }
    FastLED.show();
}
