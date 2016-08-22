#ifndef PAGE_LEDCONTROL_H_
#define PAGE_LEDCONTROL_H_

extern PixelDriver     pixels;         /* Pixel object */

#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
#include "page_config_pixel.h"
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#include "page_config_serial.h"
#else
#error "No valid output mode defined."
#endif

void send_led_val(AsyncWebServerRequest *request) {
    char rgbStr[18] = {0};
    sprintf(rgbStr, "\#%02X%02X%02X", pixels.getValue(0), pixels.getValue(1), pixels.getValue(2));
    String values = "";
    values += "colour|input|" + (String)rgbStr + "\n";
    request->send(200, "text/plain", values);
}

void send_led_val_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "colour")
            {
              for (int i=0; i<config.channel_count; i+=3)
              {
                pixels.setValue(i,p->value().toInt());
              }
            }
        }
        request->send(200);
    } else {
        request->send(400);
    }
}

#endif /* PAGE_LEDCONTROL_H_ */
