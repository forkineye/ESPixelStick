#ifndef PWM_H_
#define PWM_H_

#include "gamma.h"

#define NUM_GPIO 17    // 0 .. 16 inclusive

// GPIO 6-11 are for flash chip
#if defined (ESPS_MODE_PIXEL) || ( defined(ESPS_MODE_SERIAL) && (SEROUT_UART == 1))
// { 0,1,  3,4,5,12,13,14,15,16 };  // 2 is WS2811 led data
#define pwm_valid_gpio_mask 0b11111000000111011

#elif defined(ESPS_MODE_SERIAL) && (SEROUT_UART == 0)
// { 0,  2,3,4,5,12,13,14,15,16 };  // 1 is serial TX for DMX data
#define pwm_valid_gpio_mask 0b11111000000111101
#endif

#if defined(ESPS_MODE_PIXEL)
extern PixelDriver     pixels;         // Pixel object
#elif defined(ESPS_MODE_SERIAL)
extern SerialDriver    serial;         // Serial object
#endif

void setupPWM ();
void handlePWM ();

#endif // PWM_H_
