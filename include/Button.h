#ifndef BUTTON_H
#define BUTTON_H

// Optional hardware button.
// Enable with -D HAVE_BUTTON=true and choose a pin with -D BUTTON_PIN=<n>.
// The button must be wired between the pin and GND (internal pull-up is used).
#ifndef HAVE_BUTTON
#define HAVE_BUTTON 0
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN 2
#endif

#if HAVE_BUTTON
void buttonInit();
#else
inline void buttonInit() {}
#endif

#endif // BUTTON_H