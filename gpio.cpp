#include "gpio.h"
void handleGPIO (AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  //LOG_PORT.println("yay got gpio");
  //request->send(200, "text/json", "Yall got ma gpio!!");
  String substrings[10];
  int res = splitString('/', request->url(), substrings, sizeof(substrings) / sizeof(substrings[0]));
 // LOG_PORT.printf("Ret val is %d\n", res);
  int gpio = substrings[2].toInt();
  if ((gpio == 0) && (substrings[3].charAt(0) != '0')) {
    gpio = -1;
  }

  LOG_PORT.printf( "got gpio : %d\n", gpio);
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
      //message += " is valid\n";
      if (substrings[3] == "get") {
       // LOG_PORT.println("yay got get");
        response->printf("gpio%d is %d\r\n", gpio, digitalRead(gpio));


      } else if (substrings[3] == "set") {
        int state = substrings[4].toInt();
        digitalWrite(gpio, state);
        response->printf("gpio%d set to %d\r\n", gpio, state);
/*
      } else if (substrings[3] == "toggle") {
        for (int i = 0; i < substrings[4].length(); i++) {
          int state = substrings[4].charAt(i) - 0x30;
          digitalWrite(gpio, state);
          delay(200);
          response->printf("gpio%d set to %d\r\n", gpio, state);

        }
*/
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
        // message += "ERROR!\n";

      }
      break;
    default:
       response->printf("Invalid gpio %d\r\n");
      break;
  }
  request->send(response);
}
int splitString(char separator, String input, String results[], int numStrings) {
  int idx;
  int last_idx = 0;
  int retval = 0;
  //    message += "numStrings: ";
  //    message += numStrings;
  //    message += "\n";
  //LOG_PORT.println(input);
  for (int i = 0; i < numStrings; i++) {
    results[i] = "";     // pre clear this
    idx = input.indexOf(separator, last_idx);
    // message += "i: " ; message += i;
    // message += " idx: " ; message += idx;
    // message += " last_idx: " ; message += last_idx;
    if ((idx == -1) && (last_idx == -1)) {
      break;
    } else {
      results[i] = input.substring(last_idx, idx);
      retval ++;
      // message += " results: "; message += results[i];
      // message += "\n";
      if (idx != -1) {
        last_idx = idx + 1;
      } else {
        last_idx = -1;
      }
    }
  }
  return retval;
}
