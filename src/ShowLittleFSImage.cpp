#include "ShowLittleFSImage.h"
#include "Globals.h"

int cnti = 0;

void showLittleFSImage() {
    // Try to find a valid file within the current pattern range
    int originalImageToUse = imageToUse;
    bool fileFound = false;
    
    do {
        a = LittleFS.open(bin, "r");
        if (a) {
            fileFound = true;
            break;
        }
        
        // File doesn't exist, try next image
        imageToUse++;
        if (imageToUse > maxImages) imageToUse = minImages;
        
        // Update bin path for next attempt
        #ifndef ESP32
            bin.setCharAt(0, images.charAt(imageToUse));
        #else
            bin.setCharAt(1, images.charAt(imageToUse));
        #endif
        
    } while (imageToUse != originalImageToUse && !fileFound);
    
    if (!fileFound) {
        // No files found in this pattern range, skip display
        return;
    }
        size_t size = a.size();
        
        if (size > MAX_PX) {
            FastLED.showColor(CRGB::Blue);
            imageToUse++;
            if (imageToUse > maxImages) imageToUse = minImages;
        } else {
            pxAcross = int(size / pxDown);
            a.read(message1Data, size);
            
            cnti = (cnti >= pxDown) ? 0 : cnti + 1;
            a.close();
        }

    int counter = 0;
    for (int j = 0; j < pxAcross; j++) {
        for (int i = 0; i < pxDown; i++) {
            byte X = message1Data[counter++];
            leds[i].r = (X & 0xE0);
            leds[i].g = ((X << 3) & 0xE0);
            leds[i].b = (X << 6);
        }
        FastLED.show();
        yield();
    }
}