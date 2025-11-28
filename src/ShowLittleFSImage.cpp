#include "ShowLittleFSImage.h"
#include "Globals.h"

int cnti = 0;

void showLittleFSImage() {
    // Open the image file from LittleFS
    a = LittleFS.open(bin, "r"); // Open every time?

    if (!a)
    {
        if (pattern >= 8) {
            pattern = 1;
            return;
        }
        // If no file is found, go to the next image
        imageToUse++;
        if(imageToUse > maxImages){
            bin.setCharAt(1, currentImages.charAt(minImages));
        } else {
            bin.setCharAt(1, currentImages.charAt(imageToUse));
        }
        
        // FastLED.showColor(CRGB::Blue); // Display error color (optional)
        // Serial.println("Code Blue - no file found!");
        // return;
    }
    else
    {
        size_t size = a.size(); // Get file size

        // Check if the image size is larger than the max allowed
        if (size > MAX_PX)
        {
            a.close();
            // Display error if the image is too large
            FastLED.showColor(CRGB::Blue); // Show blue color as error indicator
            imageToUse++;
            if(imageToUse > maxImages){
                bin.setCharAt(1, currentImages.charAt(minImages));
            } else {
                bin.setCharAt(1, currentImages.charAt(imageToUse));
            }
            return;
        }
        else
        {
            // Calculate the number of pixels across based on the file size
            pxAcross = int(size / pxDown); // Should be an integer

            // Read image data into message1Data buffer
            a.read(message1Data, size);

            cnti++;
            if (cnti >= pxDown)
            {
                cnti = 0;
            }

            // Close the file after reading
            a.close();
        }
    }

    // Initialize counter for reading pixel data
    int counter = 0;

    // Loop through the pixels to display the image
    for (int j = 0; j < pxAcross; j++)
    {
        for (int i = 0; i < pxDown; i++)
        {
            byte X = message1Data[counter++]; // Get pixel data

            // Decompress and assign color values
            leds[i].r = (X & 0xE0);
            leds[i].g = ((X << 3) & 0xE0);
            leds[i].b = (X << 6);

        }
        
        // Display the current row of pixels on the LED strip
        FastLED.show();
        yield();
    }
}


/////////////////////////////////////////////////////////////////////////OLD CODE////////////////
//     // For patterns 2-5 (range patterns): check if any file exists in the range
//     if (pattern >= 2 && pattern <= 5) {
//         bool anyFileExists = false;
        
//         // Check all files in the current range
//         for (int i = minImages; i <= maxImages; i++) {
//             String testBin = bin;
//             testBin.setCharAt(1, images.charAt(i));
                      
//             if (LittleFS.exists(testBin)) {
//                 anyFileExists = true;
//                 break;
//             }
//         }
        
//         // If no files exist in the entire range, switch to pattern 1
//         if (!anyFileExists) {
//             pattern = 1;
//             return;
//         }
//     }
    
//     // For patterns 8+ (single image patterns): check if the specific file exists
//     if (pattern >= 8) {
//         if (!LittleFS.exists(bin)) {
//             pattern = 1;
//             return;
//         }
//     }
    
//     // Try to find a valid file within the current pattern range
//     int originalImageToUse = imageToUse;
//     bool fileFound = false;
    
//     do {
//         a = LittleFS.open(bin, "r");
//         if (a) {
//             fileFound = true;
//             break;
//         }
        
//         // File doesn't exist, try next image
//         imageToUse++;
//         if (imageToUse > maxImages) imageToUse = minImages;
        
//         // Update bin path for next attempt
//         bin.setCharAt(1, images.charAt(imageToUse));
        
//     } while (imageToUse != originalImageToUse && !fileFound);
    
//     if (!fileFound) {
//         // This should not happen for patterns 2-5 since we already checked the range
//         // But for safety, switch to pattern 1
//         pattern = 1;
//         return;
//     }
//         size_t size = a.size();
        
//         if (size > MAX_PX) {
//             FastLED.showColor(CRGB::Blue);
//             imageToUse++;
//             if (imageToUse > maxImages) imageToUse = minImages;
//         } else {
//             pxAcross = int(size / pxDown);
//             a.read(message1Data, size);
            
//             cnti = (cnti >= pxDown) ? 0 : cnti + 1;
//             a.close();
//         }

//     int counter = 0;
//     for (int j = 0; j < pxAcross; j++) {
//         for (int i = 0; i < pxDown; i++) {
//             byte X = message1Data[counter++];
//             leds[i].r = (X & 0xE0);
//             leds[i].g = ((X << 3) & 0xE0);
//             leds[i].b = (X << 6);
//         }
//         FastLED.show();
//         yield();
//     }
// }