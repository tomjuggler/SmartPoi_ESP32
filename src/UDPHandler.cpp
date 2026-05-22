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
    static unsigned long udpPktCount = 0;
    
    #if SERIAL_DEBUG
    static unsigned long lastDebugMillis = 0;
    #endif
    
    len = Udp.read(packetBuffer, sizeof(packetBuffer));  // 250 bytes — match ESP-NOW v1.0 limit
    if (len > 0) {
        packetBuffer[len] = 0;
        lastPacketTime = currentMillis2;

        #if SERIAL_DEBUG
        udpPktCount++;
        // Throttle: print every 100 packets or every 2 seconds
        if (udpPktCount % 100 == 0 || millis() - lastDebugMillis > 2000) {
            lastDebugMillis = millis();
            Serial.printf("[UDP] pkt#%lu  len=%d  data[0..7]:", udpPktCount, len);
            for (int d = 0; d < min(len, 8); d++) {
                Serial.printf(" %02X", packetBuffer[d]);
            }
            Serial.println();
        }
        #endif
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
    if (len != NUM_PX) {
        #if SERIAL_DEBUG
        static unsigned long lastBadLenMillis = 0;
        if (millis() - lastBadLenMillis > 2000) {
            lastBadLenMillis = millis();
            Serial.printf("[ESPNOW] bad len=%d (expected %d)\n", len, NUM_PX);
        }
        #endif
        return;
    }

    #if SERIAL_DEBUG
    static unsigned long espnowPktCount = 0;
    static unsigned long lastDbgMillis = 0;
    espnowPktCount++;
    // Throttle: print every 100 packets or every 2 seconds
    if (espnowPktCount % 100 == 0 || millis() - lastDbgMillis > 2000) {
        lastDbgMillis = millis();
        Serial.printf("[ESPNOW] pkt#%lu  MAC=%02X:%02X:%02X:%02X:%02X:%02X  len=%d  data[0..7]:",
            espnowPktCount,
            recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
            recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
            len);
        for (int d = 0; d < min(len, 8); d++) {
            Serial.printf(" %02X", incomingData[d]);
        }
        Serial.println();
    }
    #endif

    // Decompress 3-3-2 bit-packed format (identical to handleUDP logic)
    for (int i = 0; i < NUM_PX; i++) {
        int X = incomingData[i] - 127;
        leds[i].r = (X & 0xE0);           // Red:   bits 7–5
        leds[i].g = ((X << 3) & 0xE0);    // Green: bits 4–2
        leds[i].b = (X << 6);            // Blue:  bits 1–0
    }
    FastLED.show();
}
