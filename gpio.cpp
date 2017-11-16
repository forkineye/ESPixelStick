/*
Generic GPIO interface via the web interface

IMPLEMENTED URLs:
/uptime            // show uptime, heap, signal strength

/gpio/<n>/get      // read gpio <n>
/gpio/<n>/set/0    // set to low
/gpio/<n>/set/1    // set to high
/gpio/<n>/mode/0   // set to input
/gpio/<n>/mode/1   // set to output
/gpio/<n>/toggle/101  // toggle the output 1,0,1 with 200ms delay

NOT IMPLEMENTED:
/acl/<row>/set/aaa.bbb.ccc.ddd.eee/mask  // add aaa.bbb.ccc.ddd to the allowed IP list in slot <row>
/acl/<row>/get    // show allowed IP list in slot <row>

*/

#include "gpio.h"

int gpio;
int state = -1;
int toggleCounter = -1;
String toggleString;
int toggleGpio = -1;

static unsigned long lWaitMillis;
long unsigned int this_mill;
long unsigned int last_mill;
unsigned long long  extended_mill;
unsigned long long mill_rollover_count;

extern AsyncWebServer  web; // Web Server


void handleGPIO (AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  String substrings[10];
  int res = splitString('/', request->url(), substrings, sizeof(substrings) / sizeof(substrings[0]));
  gpio = substrings[2].toInt();
  // distinguish between valid gpio 0 and invalid data (which also return 0 *sigh*)
  if ((gpio == 0) && (substrings[2].charAt(0) != '0')) {
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
      response->printf("Invalid gpio %d\r\n",gpio);
      break;
  }
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

  this_mill = millis();
  if (last_mill > this_mill) {  // rollover
      mill_rollover_count ++;
  }
  extended_mill = (mill_rollover_count << (8*sizeof(this_mill))) + this_mill;
  last_mill = this_mill;

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
    // gpio url handler
    web.on("/gpio", HTTP_GET, handleGPIO).setFilter(ON_STA_FILTER);

    // uptime Handler
    web.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/plain");
        long int secs = (extended_mill/1000);
        long int mins = (secs/60);
        long int hours = (mins/60);
        long int days = (hours/24);

        response->printf ("Uptime: %d days, %02d:%02d:%02d.%03d\r\nFreeHeap: %x\r\nSignal: %d\r\n", 
                days, hours%24, mins%60, secs%60, (int)extended_mill%1000, ESP.getFreeHeap(), WiFi.RSSI() );
        request->send(response);
    });
}

