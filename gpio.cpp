#include "gpio.h"

int gpio;
int state = -1;
int toggleCounter = -1;
String toggleString;
int toggleGpio = -1;
static unsigned long lWaitMillis;

void handleGPIO (AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  String substrings[10];
  int res = splitString('/', request->url(), substrings, sizeof(substrings) / sizeof(substrings[0]));
  gpio = substrings[2].toInt();
  if ((gpio == 0) && (substrings[3].charAt(0) != '0')) {
    gpio = -1;
  }
  switch (gpio) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
      if (substrings[3] == "get") {
        response->printf("gpio%d is %d\r\n", gpio, digitalRead(gpio));

      } else if (substrings[3] == "set") {
        int state = substrings[4].toInt();
        digitalWrite(gpio, state);
        response->printf("gpio%d set to %d\r\n", gpio, state);

      } else if (substrings[3] == "toggle") {
        response->printf("gpio%d toggled to %s\r\n", gpio, substrings[4].c_str());
        toggleGpio = gpio;
        toggleString = substrings[4];
        toggleCounter = 0;
      } else if (substrings[3] == "mode") {
        if (substrings[4].charAt(0) == 'S') {
          pinMode(gpio, SPECIAL);
        }
        else if (substrings[4].charAt(0) == '?') {
          // add query pin mode when i can work out how!
        } else {
          int state = substrings[4].toInt();
          pinMode(gpio, state);
          response->printf("gpio%d mode %d\r\n", gpio, state);
        }
      } else {
      }
      break;
    default:
      response->printf("Invalid gpio %d\r\n");
      break;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}


int splitString(char separator, String input, String results[], int numStrings) {
  int idx;
  int last_idx = 0;
  int retval = 0;
  for (int i = 0; i < numStrings; i++) {
    results[i] = "";     // pre clear this
    idx = input.indexOf(separator, last_idx);
    if ((idx == -1) && (last_idx == -1)) {
      break;
    } else {
      results[i] = input.substring(last_idx, idx);
      retval ++;

      if (idx != -1) {
        last_idx = idx + 1;
      } else {
        last_idx = -1;
      }
    }
  }
  return retval;
}


void ToggleTime() {

  if ( (long)( millis() - lWaitMillis ) >= 0)
  {
    // millis is now later than my 'next' time
    lWaitMillis += toggleMS;  // do it again 1 second later
    
    if (toggleCounter >= 0) {
      if (toggleString.charAt(toggleCounter) == '1') {
        digitalWrite(toggleGpio, 1);
      } else {
        digitalWrite(toggleGpio, 0);
      }

      toggleCounter++;
      if (toggleCounter >= toggleString.length()) {
        toggleCounter = -1;
      }
    }
  }
}

void ToggleSetup() {
  lWaitMillis = millis() + toggleMS;  // initial setup
}


