#include "ShowLittleFSImage.h"
#include "Globals.h"
#include "tasks.h"

int cnti = 0;
char currentLoadedImageChar = '\0'; // Tracks which image is currently loaded in buffer
bool fileNeedsReload = true;        // Flag to indicate file needs to be reloaded

// Function to load image data into buffer
bool loadImageData(char patternChar) {
    // Check if file exists before attempting to open it
    if (!checkPatternFileExists(patternChar)) {
        return false;
    }
    
    // File exists, open it for reading
    a = LittleFS.open(bin, "r");
    
    if (!a) {
        return false;
    }
    
    size_t size = a.size(); // Get file size
    
    // Check if the image size is larger than the max allowed
    if (size > MAX_PX) {
        a.close();
        return false;
    }
    
    // Calculate the number of pixels across based on the file size
    pxAcross = int(size / pxDown); // Should be an integer
    
    // Read image data into message1Data buffer
    a.read(message1Data, size);
    
    cnti++;
    if (cnti >= pxDown) {
        cnti = 0;
    }
    
    // Close the file after reading
    a.close();
    
    currentLoadedImageChar = patternChar;
    fileNeedsReload = false;
    return true;
}

// Function to mark file for reload (called when files are uploaded/deleted)
extern "C" void markFileForReload(char patternChar) {
    // If the currently loaded image matches the changed file, mark for reload
    if (currentLoadedImageChar == patternChar) {
        fileNeedsReload = true;
        currentLoadedImageChar = '\0'; // Reset to force reload check
    }
}

void showLittleFSImage() {
    // Extract character from bin string (format: "/x.bin" where x is the character)
    char patternChar = bin.charAt(1);
    
    // Check if we need to reload the file
    if (fileNeedsReload || currentLoadedImageChar != patternChar) {
        if (!loadImageData(patternChar)) {
            // File doesn't exist or failed to load, handle gracefully
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
            return;
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
