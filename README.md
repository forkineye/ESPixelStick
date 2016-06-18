ESPixelStick Firmware
=====================
This is the Arduino firmware for the ESP8266 based ESPixelStick.  The ESPixelStick is a small wireless E1.31 sACN pixel controller designed to control a single strand of pixels.  Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate.  

Since this project began, the firmware has moved beyond just pixel support for those with other ESP8266 based devices.  The ESPixelStick firmware now supports outputting E1.31 streams to serial links as well.  Note this is not supported on the ESPixelStick hardware, but intended for other ESP8266 devices such as Bill's RenardESP.

### Supported Pixels
- WS2811 / WS2812 / WS2812b
- GE Color Effects

### Requirements
- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266
- [Arduino ESP8266 Filesystem Uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library
- [gulp](http://gulpjs.com/) - Build system to process web sources.  Optional, but recommended.  Refer to the html [README](html/README.md) for more information.

### Notes
- For best performance, set the CPU frequency to 160MHz (Tools->CPU Frequency)
- Web pages ***must*** be processed, placed into ```data/www```, and uploaded with the upload tool. Gulp will process the pages and put them in ```data/www``` for you. 

### Known Issues
- Gamma value is ingored.  ```pow()``` is currently broken in the ESP8266 Arduino environment, so gamma tables cannot be generated.
- Some fields do not validate input.  Need to add validation routines to the web configuration inputs fields.

### To-do
- Add mDNS and DNS-SD support.

### Resources
- Firmware: http://github.com/forkineye/ESPixelStick
- Hardware: http://forkineye.com/ESPixelStick

### Credits
- The people at http://diychristmas.org and http://doityourselfchristmas.com for inspiration.
- John Lassen for his web framework from which the web porition was derived - http://www.john-lassen.de/index.php/projects/esp-8266-arduino-ide-webconfig.
- Bill Porter for Renard and SoftAP aupport.
- Bill Porter and Grayson Lough for DMX support.