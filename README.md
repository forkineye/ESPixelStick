ESPixelStick Firmware
=====================
[![Join the chat at https://gitter.im/forkineye/ESPixelStick](https://badges.gitter.im/forkineye/ESPixelStick.svg)](https://gitter.im/forkineye/ESPixelStick)

This is the Arduino firmware for the ESP8266 based ESPixelStick.  The ESPixelStick is a small wireless E1.31 sACN pixel controller designed to control a single strand of pixels.  Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate.  

Since this project began, the firmware has moved beyond just pixel support for those with other ESP8266 based devices.  The ESPixelStick firmware now supports outputting E1.31 streams to serial links as well.  Note this is not supported on the ESPixelStick hardware, but intended for other ESP8266 devices such as Bill's RenardESP.

Requirements
------------
Along with the Arduino IDE, you'll need the following software to build this project:
- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266
- [Arduino ESP8266 Filesystem Uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [gulp](http://gulpjs.com/) - Build system to process web sources.  Optional, but recommended.  Refer to the html [README](html/README.md) for more information.

The following libraries are required:
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library

Important Notes on Compiling and Flashing
-----------------------------------------
- Device mode is now a compile time option to set your device type and is configured in the top of the main sketch file.  Current options are ```ESPS_MODE_PIXEL``` and ```ESPS_MODE_SERIAL```.  The default is ```ESPS_MODE_PIXEL``` for the ESPixelStick hardware.
- Web pages **must** be processed, placed into ```data/www```, and uploaded with the upload plugin. Gulp will process the pages and put them in ```data/www``` for you. Refer to the html [README](html/README.md) for more information.
- In order to use the upload plugin, the ESP8266 **must** be placed into programming mode and the Arduino serial monitor **must** be closed.
- ESP-01 modules **must** be configured for 1M flash and 128k SPIFFS within the Arduino IDE for OTA updates to work.
- For best performance, set the CPU frequency to 160MHz (Tools->CPU Frequency).  You may experience lag and other issues if running at 80MHz.

Supported Outputs
-----------------
The ESPixelStick firmware can generate the following outputs from incoming E1.31 streams, however your hardware must support the physical interface.
#### Pixel Protocols
- WS2811 / WS2812 / WS2812b
- GE Color Effects

#### Serial Protocols
- DMX512
- Renard

Resources
---------
- Firmware: http://github.com/forkineye/ESPixelStick
- Hardware: http://forkineye.com/ESPixelStick

Credits
-------
- The people at http://diychristmas.org and http://doityourselfchristmas.com for inspiration.
- John Lassen for his web framework from which the web porition was derived - http://www.john-lassen.de/index.php/projects/esp-8266-arduino-ide-webconfig.
- Bill Porter for Renard and SoftAP aupport.
- Bill Porter and Grayson Lough for DMX support.
