#ifndef TIMEFUNC_H
#define TIMEFUNC_H

#include "Globals.h"
#include <Arduino.h>

// Extern declarations for shared variables
extern unsigned long previousMillis3;
extern String bin;
extern int imageToUse;
extern long interval;
extern int maxImages;
extern int minImages;
extern int pattern;

void ChangePatternPeriodically();

#endif
