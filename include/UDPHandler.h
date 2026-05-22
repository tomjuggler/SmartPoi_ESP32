#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "Globals.h"

void handleUDP();

// ESP-NOW receive callback (dual-transport: runs alongside UDP)
// ESP-IDF v5.x signature: esp_now_recv_info_t* for source address
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len);

#endif
