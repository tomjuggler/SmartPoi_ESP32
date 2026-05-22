#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "Globals.h"

void handleUDP();

// ESP-NOW receive callback (dual-transport: runs alongside UDP)
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);

#endif
