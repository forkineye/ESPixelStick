ESPixelStick Firmware
=====================
[![Join the chat at https://gitter.im/forkineye/ESPixelStick](https://badges.gitter.im/forkineye/ESPixelStick.svg)](https://gitter.im/forkineye/ESPixelStick)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](paypal.me/ShelbyMerrick)

This is the Arduino firmware for the ESP8266 based ESPixelStick.  The ESPixelStick is a small wireless E1.31 sACN pixel controller designed to control a single strand of pixels.  Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate.  MQTT support is provided as well for integration into home automation systems where an E1.31 source may not be present.

Since this project began, the firmware has moved beyond just pixel support for those with other ESP8266 based devices.  The ESPixelStick firmware now supports outputting E1.31 streams to serial links as well.  Note this is not supported on the ESPixelStick hardware, but intended for other ESP8266 devices such as Bill's RenardESP.

Hardware
--------
Being open source, you are free to use the ESPixelStick firmware on the device of your choice.  The code however is written specifically for the [ESPixelStick](http://forkineye.com/espixelstick). The ESPixelStick V2 utilizes an ESP-01 module and provides high current connectors, fusing, power filtering, a programming interface and proper logic level buffering.  If you're in the US and would like to purchase an ESPixelStick, they are available via [Amazon](http://amzn.to/2uqBFuX).  The proceeds go towards things like keeping my wife happy so I can work on this project :)

Requirements
------------
Along with the Arduino IDE, you'll need the following software to build this project:
- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266 - **2.40-rc2 or greater required**
- [Arduino ESP8266 Filesystem Uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [gulp](http://gulpjs.com/) - Build system required to process web sources.  Refer to the html [README](html/README.md) for more information.

The following libraries are required:
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncE131](https://github.com/forkineye/ESPAsyncE131) - Asynchronous E1.31 (sACN) library
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncUDP](https://github.com/me-no-dev/ESPAsyncUDP) - Asynchronous UDP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library
- [async-mqtt-client](https://github.com/marvinroger/async-mqtt-client) - Asynchronous MQTT Client

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
Support upto 8 universes each of 512channel of output - making it 1360pixels(170pixels in each universe ) at 24Hz frame rate.
These numbers can be increased by editing the file ESPixelStick.h but it will decrease the frame rate even further.
#### Serial Protocols
- DMX512
- Renard
The renard has a limit of 2048 Channel limit for serial outputs.
MQTT Support
------------
**NOTE:** Effects are planned, this is just the initial static support.  Brightness is not implemented yet either.

MQTT can be configured via the web interface.  When enabled, a payload of "ON" will tell the ESPixelStick to override any incoming E1.31 data with MQTT data.  When a payload of "OFF" is received, E1.31 processing will resume.  All Topics are under the configured top level Topic.

For example, if you enter ```porch/esps``` as the topic, the following topics will be generated:
```
porch/esps/light/status
porch/esps/light/switch
porch/esps/brightness/status
porch/esps/brightness/set
porch/esps/rgb/status
porch/esps/rgb/set
```

And here's a corresponding configuration for [Home Assistant](https://home-assistant.io/):
```
light:
  - platform: mqtt
    name: "Front Porch ESPixelStick"
    state_topic: "porch/esps/light/status"
    command_topic: "porch/esps/light/switch"
    brightness_state_topic: "porch/esps/brightness/status"
    brightness_command_topic: "porch/esps/brightness/set"
    rgb_state_topic: "porch/esps/rgb/status"
    rgb_command_topic: "porch/esps/rgb/set"
    brightness_scale: 100
    optimistic: false
```

Resources
---------
- Firmware: http://github.com/forkineye/ESPixelStick
- Hardware: http://forkineye.com/ESPixelStick

Credits
-------
- The great people at http://diychristmas.org and http://doityourselfchristmas.com for inspiration and support.
- Bill Porter for initial Renard and SoftAP aupport.
- Bill Porter and Grayson Lough for initial DMX support.
- Rich Danby(https://github.com/cinoan) for fixes and helping polish the front-end.
- penfold42(https://github.com/penfold42) for fixes and PWM support.
