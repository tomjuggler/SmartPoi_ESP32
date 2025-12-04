#include "TimeFunc.h"
#include "Globals.h"
#include <Arduino.h>

/**
 * @brief Changes the pattern periodically based on the set interval.
 * 
 * Uses global timing variables and pattern/image indexes from main.cpp
 * Maintains original functionality with proper PlatformIO organization
 */
//todo: something here is conflicting with ShowLittleFSImage.cpp. Fix! 
void ChangePatternPeriodically()
{
  unsigned long currentMillis3 = millis();
  if (currentMillis3 - previousMillis3 >= interval)
  {
    if(pattern >= 8){
      return; //don't increment for patterns 8+
    }
    imageToUse++;
    previousMillis3 = currentMillis3;
    // if (imageToUse > maxImages)
    // {
    //   imageToUse = minImages;
    // }
    if(imageToUse > maxImages){
        imageToUse = minImages;
        bin.setCharAt(1, images.charAt(minImages));
    } else {
        bin.setCharAt(1, images.charAt(imageToUse));
    }
    // Serial.print("Changed to image: ");
    // Serial.print(imageToUse);
    // Serial.print(" Pattern: ");
    // Serial.println(pattern);
  }
  yield();
}

/**
 * @brief Gradually ramps brightness towards targetBrightness.
 * 
 * Called every 20ms, adjusts newBrightness by BRIGHTNESS_RAMP_STEP (5)
 * towards targetBrightness until they match.
 */
void checkBrightness()
{
  static unsigned long previousBrightnessCheck = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousBrightnessCheck >= BRIGHTNESS_RAMP_INTERVAL)
  {
    previousBrightnessCheck = currentMillis;
    
    if (newBrightness != targetBrightness)
    {
      int difference = targetBrightness - newBrightness;
      int step = BRIGHTNESS_RAMP_STEP;
      
      if (abs(difference) < step)
      {
        // If difference is less than step, set directly to target
        newBrightness = targetBrightness;
      }
      else if (difference > 0)
      {
        newBrightness += step;
      }
      else
      {
        newBrightness -= step;
      }
      
      // Ensure brightness stays within valid range (20-255)
      newBrightness = constrain(newBrightness, 20, 255);
      
      // Apply the new brightness
      FastLED.setBrightness(newBrightness);
      FastLED.show();
    }
  }
}
