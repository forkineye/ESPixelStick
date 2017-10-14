#ifndef GPIO_H_
#define GPIO_H_
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "ESPixelStick.h"

void handleGPIO (AsyncWebServerRequest *request); 
int splitString(char separator, String input, String results[], int numStrings);

#endif /* GPIO_H_ */
