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

