#include <Arduino.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "pwm.h"

extern  config_t    config;         // Current configuration

// PWM globals

uint8_t last_pwm[NUM_GPIO];   // 0-255, 0=dark


#if defined(ESPS_SUPPORT_PWM)
void setupPWM () {
  if ( config.pwm_global_enabled ) {
    if ( (config.pwm_freq >= 100) && (config.pwm_freq <= 1000) ) {
      analogWriteFreq(config.pwm_freq);
    }
    for (int gpio=0; gpio < NUM_GPIO; gpio++ ) {
      if ( ( valid_gpio_mask & 1<<gpio ) && (config.pwm_gpio_enabled & 1<<gpio) ) {
        pinMode(gpio, OUTPUT);
        if (config.pwm_gpio_invert & 1<<gpio) {
          analogWrite(gpio, 1023);
        } else {
          analogWrite(gpio, 0);          
        }
      }
    }
  }
}

void handlePWM() {
  if ( config.pwm_global_enabled ) {
    for (int gpio=0; gpio < NUM_GPIO; gpio++ ) {
      if ( ( valid_gpio_mask & 1<<gpio ) && (config.pwm_gpio_enabled & 1<<gpio) ) {
        uint16_t gpio_dmx = config.pwm_gpio_dmx[gpio];
        if (gpio_dmx < config.channel_count) {
#if defined (ESPS_MODE_PIXEL)
          int pwm_val = (config.pwm_gamma) ? GAMMA_TABLE[pixels.getData()[gpio_dmx]] : pixels.getData()[gpio_dmx];
#elif defined(ESPS_MODE_SERIAL)
          int pwm_val = (config.pwm_gamma) ? GAMMA_TABLE[serial.getData()[gpio_dmx]] : serial.getData()[gpio_dmx];
#endif
          if ( pwm_val != last_pwm[gpio]) {
            last_pwm[gpio] = pwm_val;
            if (config.pwm_gpio_invert & 1<<gpio) {
              analogWrite(gpio, 1023-4*pwm_val);  // 0..255 => 1023..0
            } else {
              analogWrite(gpio, 4*pwm_val);       // 0..255 => 0..1023
            }
          }
        }
      }
    }
  }
}
#endif

