ESPixelStick Firmware
=====================
This is the Arduino firmware for the ESP8266 based ESPixelStick.  The ESPixelStick is a small wireless E1.31 sACN pixel controller designed to control a single strand of pixels.  Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate.

### Supported Pixels
- WS2811 / WS2812 / WS2812b
- GE Color Effects

### Requirements
- Adruino for ESP8266 - https://github.com/esp8266/Arduino

### Known Issues
- Gamma value is ingored.  ```pow()``` is currently broken in the ESP8266 Arduino environment, so gamma tables cannot be generated.
- Some fields do not validate input.  Need to add validation routines to the web configuration inputs fields.
- Multicast mode can cause resets.  Multicast listening configuration currently causes sporadic resets with the web server is running.

### To-do
- Add softAP configuration mode.
- Add mDNS and DNS-SD support.
- Migrate to FastLED if / when ported to ESP8266.

### Resources
- Firmware: http://github.com/forkineye/ESPixelStick
- Hardware: http://forkineye.com/ESPixelStick

### Credits
- The people at http://diychristmas.org and http://doityourselfchristmas.com for inspiration.
- John Lassen for his awesome web framework from which the web porition is built on - http://www.john-lassen.de/index.php/projects/esp-8266-arduino-ide-webconfig.