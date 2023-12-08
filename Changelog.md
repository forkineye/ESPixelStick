
# Changelog

### 4.0-beta5

Numerous fixes have occurred since beta4, most notably web frontend stability (websockets removed) and FPP / xLights compatibility and synchronization.  You will need to be on a current release of xLights in order for input/output uploading to work.  MacOS libraries for the flash tool have been updated as well, but you will still have to execute the flash tool from the command line and jump through Apple's security hoops to allow mklittlefs to run.  Numerous 3rd party ESP32 platforms have been added for those that like to roll their own controllers.

### 4.0-beta4

- Moved FPP Remote to secondary input [Issue #388](https://github.com/forkineye/ESPixelStick/issues/388)
- Added option to change E1.31 port number [Issue #320](https://github.com/forkineye/ESPixelStick/issues/320)
- Display SD card stats on file mangement page [Issue #399](https://github.com/forkineye/ESPixelStick/issues/399)
- Fixed issue not being able to use GPIO0 as a relay pin [Issue #375](https://github.com/forkineye/ESPixelStick/issues/375)
- FPP multisync improvements
- Increased ESP8266 pixel limit to 1200 pixels
- Fixed issue with installing from 32bit Windows systems
- Added ESP32-CAM build enviornment and binaries
- Added RF calibration at boot and maximum TX limits to ESP8266 to help with stability
- Switched LWIP2 build configuration to "High Bandwidth, No Features"
- Switched to "yubox" fork of ESPAsnycWebserver
- Added PWM support for relays
- Added factory reset confirmation dialog

### 4.0-beta3

- Fixed FSEQ playback for sparse files [Issue #367](https://github.com/forkineye/ESPixelStick/issues/367)
- Fixed refresh rate display [Issue #371](https://github.com/forkineye/ESPixelStick/issues/371)
- Added FSEQ validation for channel and sparse range counts
- Stop output on FPP Connect file upload
- Blank output when FPP Remote configuration is changed
- Fixed files not showing for FPP Remote when tab is refreshed
- More serial logging cleanup

### 4.0-beta2

- Fixed issue saving when hostname defined in ESPSFlashTool [Issue #360](https://github.com/forkineye/ESPixelStick/issues/360)
- Websocket stability fixes
- FPP / fseq processing fixes
- Fixed hostname issue for ESP8266
- Fixed cfgver not valid issue
- Added project links to Admin page
- More serial logging cleanup
- Switch back to X.Y versioning

### 4.0-beta1

- Compiled against [Arduino Core 3.0.2](https://github.com/esp8266/Arduino/releases/tag/3.0.2) for ESP8266 platform.
- Compiled against [Arduino Core 2.0.0](https://github.com/espressif/arduino-esp32/releases/tag/2.0.0) for ESP32 platform.
- ***ESP-01 modules are no longer supported***. Version 4.0 requires 4MB of flash and ESP-01 modules only have 1MB of flash. For the ESPixelStick V1 or V2, 3.2 is the latest version you can load.
- Requires local upload to controller. OTA update from 3.x via EFU is not supported due to the new flash layout.  Upgrades to future 4.x versions from 4.0.0 final however will be supported.
- Complete refactor of the code to allow easier implementation of extensions.
- Pixel and Serial code has been unified and all output modes are now selectable at runtime.
- Added support for the ESP32 platform.
- Added Artnet support.
- Added Alexa support.
- Added SD card support.
- Added FPP remote support (requires board with SD card support).
- Added Relay output support (8 channels, DIY hardware solution with GPIO mapping).
- Added PCA9685 Servo output support (16 channels, requires PCA9685 module).
- Added WS2801 support (ESP32 only)
- Added APA-102 support (ESP32 only)

### 3.2

- Compiled against [2.7.4 Arduino Core](https://github.com/esp8266/Arduino/releases/tag/2.7.4)
- Updated to [AdruinoJson 6.17.0](https://github.com/bblanchon/ArduinoJson/releases/tag/v6.17.0)
- Updated to [async-mqtt-client 2019.12.08](https://github.com/marvinroger/async-mqtt-client/tree/7f1ba481a22d56ccf123e4b2f6e555d134c956d0)
- Fix for Client Timeout not Changing [Issue #201](https://github.com/forkineye/ESPixelStick/issues/201)
- Updated the flash tool included in binary releases to fix issues flashing on newer releases of MacOS.

### 3.1

- Compiled against [2.6.3 Arduino Core](https://github.com/esp8266/Arduino/releases/tag/2.6.3)
- ESPSFlashTool support for ESPixelStick v3.0 Hardware
- Fixed DMX issues - [Issue #101](https://github.com/forkineye/ESPixelStick/issues/101)
- Added brightness support.
- Added calculated gamma support.
- Added grouping and zigzag for pixels.
- Added startup and idle effect options.
- Added FPP Discovery support.
- Added xLights ZCPP support.
- Added DDP support.
- Added Home Assistant MQTT Discovery support.
- New effects for standalone and MQTT usage.
- Changed GECE output from bit-banging to a UART based implementation.

### 3.0

- Compiled against [2017.11.20 Arduino core](https://github.com/esp8266/Arduino/tree/117bc875ffdd1f4b11af0dd236e0e12a84748e53)
  - Includes WPA2 KRACK security fix
- Migrated to [ESPAsyncE131](https://github.com/forkineye/ESPAsyncE131) library
- Fixed [Issue #56](https://github.com/forkineye/ESPixelStick/issues/56) - Read too far
- Fixed [Issue #65](https://github.com/forkineye/ESPixelStick/issues/65) - Websocket queuing
- Fixed [Issue #80](https://github.com/forkineye/ESPixelStick/issues/80) - Test mode state reporting

### 3.0-rc3

- Fixed IGMP timer bug in lwip (Fix submitted to ESP8266 Arduino project)

### 3.0-rc2

- Fixed IGMP subscription bug introduced in 3.0-dev1
- Fixed issue receiving E1.31 alternate start codes (fixed in E131 library)
- Added Universe Boundary configuration

### 3.0-rc1

- Fixed validation errors on wifi setup page.
- Fixed AP reboot loop.
- Fixed color order issue in testing interface.

### 3.0-dev1

- Complete re-write of the web frontend. Requires a WebSockets capable browser.
- Migrated to ESPAsyncUDP for E131 parsing (E131 library updated).
- Increased WS2811 reset time from 50us to 300us to support newer WS2811 chips.
- Added hostname configuration.
- Added GBR and BGR color order for pixels.

### 2.0

- Added web based OTA update capability.
- Changed output type (pixel vs serial) to compile time option. Check top of main sketch.
- Migrated to ESPAsyncWebserver for the web server.
- Migrated all web pages to SPIFFS.
- Migrated configuration structure to SPIFFS as a JSON file.
- Added Gulp script to assist with preparing web pages.
- Added mDNS / DNS-SD responder.
- Made WS2811 stream generation asynchronous.
- Removed PPU configuration for pixels.
- Pixel data now will utilize all 512 channels of a universe if required.
- Changed default UART for Renard / DMX to UART1. Can be changed in SerialDriver.h.
- Made Serial stream generation asynchronous (Renard and DMX).
- Added multi-universe support for Renard.
- Added 480kbps for Renard.

### 1.4

- Arduino 1.6.8 compatibility.
- Added SoftAP support (credit: Bill Porter)
- Added Renard Support (credit: Bill Porter)
- Added DMX Support (credit: Bill Porter and Grayson Lough)

### 1.3

- Fix for issues when compiling on Mac and Linux.
- Web page optimizations.
- Configuration structure update to fix alignment errors.

### 1.2

- Added Administration Page.
- Disconnect from WiFi before rebooting.
- Code cleanup / housekeeping.

### 1.1

- Migration to UART code for handling WS281x streams.
- Initial GECE support.
- Support for multiple universes.
- Pixel per Universe configuration to allow subsets of multiple universes to be used.
- Removed external library dependencies.
- Added stream timeout and refresh overflow handling.

### 1.0

- Initial release.
- Single Universe.
- WS281x only.
