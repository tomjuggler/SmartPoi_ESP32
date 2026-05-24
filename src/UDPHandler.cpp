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
extern String bin;

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
    // Check for sync packet first (always process, regardless of pattern)
    // Sync packets: 6 bytes, byte[0] == 0x01 (distinct from pixel data which is NUM_PX bytes)
    if (len == 6 && incomingData[0] == 0x01) {
        // Only auxiliary applies sync; main ignores its own broadcasts
        if (auxillary) {
            applySync(incomingData);
        }
        return;
    }

    // Only process pixel data if in pattern 0 (UDP/ESP-NOW mode)
    if (pattern != 0) return;

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

// ============================================================
// ESP-NOW Pattern-State Sync Broadcast (Main → Auxiliary)
// ============================================================
// Called periodically from loop() on main device only.
// Sends current pattern, imageToUse, image character, and interval.
// Only broadcasts in AP mode (wifiModeChooser == 1).
void broadcastSync() {
    // Only main broadcasts; auxiliary is silent
    if (auxillary) return;
    
    // Only in AP mode (wifiModeChooser == 1)
    if (wifiModeChooser != 1) return;
    
    uint8_t syncPacket[6];
    syncPacket[0] = 0x01;                     // sync packet type identifier
    syncPacket[1] = (uint8_t)pattern;          // current pattern number
    syncPacket[2] = (uint8_t)imageToUse;       // current image index
    syncPacket[3] = (uint8_t)bin.charAt(1);    // current image character
    syncPacket[4] = (uint8_t)(interval & 0xFF); // interval low byte
    syncPacket[5] = (uint8_t)((interval >> 8) & 0xFF); // interval high byte
    
    esp_err_t result = esp_now_send(broadcastAddress, syncPacket, 6);
    #if SERIAL_DEBUG
    if (result != ESP_OK) {
        static unsigned long lastSyncErrMillis = 0;
        if (millis() - lastSyncErrMillis > 3000) {
            lastSyncErrMillis = millis();
            Serial.printf("[SYNC] broadcast failed: %d\n", result);
        }
    }
    #endif
}

// ============================================================
// Apply Received Sync Data (Auxiliary only)
// ============================================================
// Snaps the auxiliary device to match main's pattern state.
// Called from onDataReceived() when a sync packet is received.
void applySync(const uint8_t *data) {
    // Only auxiliary applies; main ignores
    if (!auxillary) return;
    
    int newPattern = data[1];
    int newImageToUse = data[2];
    char newImageChar = (char)data[3];
    long newInterval = data[4] | ((long)data[5] << 8);
    
    // Apply pattern change
    if (newPattern != pattern) {
        pattern = newPattern;
        patternChooser = newPattern;
        updateCurrentImagesForPattern(pattern);
        #if SERIAL_DEBUG
        Serial.printf("[SYNC] pattern -> %d\n", newPattern);
        #endif
    }
    
    // Apply image change
    if (newImageToUse != imageToUse || bin.charAt(1) != newImageChar) {
        imageToUse = newImageToUse;
        bin.setCharAt(1, newImageChar);
        // Signal file reader task to reload the new image
        extern char currentLoadedImageChar;
        extern bool fileNeedsReload;
        currentLoadedImageChar = '\0';
        fileNeedsReload = true;
        #if SERIAL_DEBUG
        Serial.printf("[SYNC] image -> %d ('%c')\n", newImageToUse, newImageChar);
        #endif
    }
    
    // Apply interval change
    if (newInterval != interval) {
        interval = newInterval;
        #if SERIAL_DEBUG
        Serial.printf("[SYNC] interval -> %ld ms\n", newInterval);
        #endif
    }
    
    // Reset phase timer so auxiliary doesn't advance immediately on its own
    previousMillis3 = millis();
}
