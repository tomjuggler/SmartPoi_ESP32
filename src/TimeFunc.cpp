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
