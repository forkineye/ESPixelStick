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
  int pwm_val;

  if ( config.pwm_global_enabled ) {
    for (int gpio=0; gpio < NUM_GPIO; gpio++ ) {
      if ( ( valid_gpio_mask & 1<<gpio ) && (config.pwm_gpio_enabled & 1<<gpio) ) {
        uint16_t gpio_dmx = config.pwm_gpio_dmx[gpio];
        if (gpio_dmx < config.channel_count) {
#if defined (ESPS_MODE_PIXEL)
          pwm_val = pixels.getData()[gpio_dmx];
#elif defined(ESPS_MODE_SERIAL)
          if (config.serial_type == SerialType::DMX512)
                pwm_val = serial.getData()[gpio_dmx+1];
          else
                pwm_val = serial.getData()[gpio_dmx+2];
#endif

          if (config.pwm_gpio_digital & 1<<gpio) {
            if ( pwm_val >= 128) {
              pwm_val = 255;
            } else {
              pwm_val = 0;
            }
          } else {
            if (config.pwm_gamma) {
              pwm_val = GAMMA_TABLE[pwm_val];
            }
          }

          if ( pwm_val != last_pwm[gpio]) {
            last_pwm[gpio] = pwm_val;
            if (config.pwm_gpio_invert & 1<<gpio) {
              analogWrite(gpio, 1023-(1023*pwm_val/255) );  // 0..255 => 1023..0
            } else {
              analogWrite(gpio, 1023*pwm_val/255);       // 0..255 => 0..1023
            }
          }
        }
      }
    }
  }
}
#endif

