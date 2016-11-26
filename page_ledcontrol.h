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
    sprintf(rgbStr, "%02X%02X%02X", pixels.getValue(0), pixels.getValue(1), pixels.getValue(2));
    String values = "";
    values += "colour|input|" + (String)rgbStr + "\n";
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", values);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void send_led_val_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "colour") {
//Serial.print("got colour str");
//Serial.println(p->value());
long RGBint = (int) strtol( p->value().c_str(), NULL, 16);
//Serial.print("got colour RGBint");
//Serial.println(RGBint);

              for (int i=0; i<config.channel_count; i+=3) {
                pixels.setValue(i+0,RGBint>>16 & 0xff);
                pixels.setValue(i+1,RGBint>>8  & 0xff);
                pixels.setValue(i+2,RGBint     & 0xff);
              }
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);
    } else {
        request->send(400);
    }
}

#endif /* PAGE_LEDCONTROL_H_ */
