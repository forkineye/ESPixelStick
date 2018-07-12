
Changelog
=========
### 3.1-dev (on-going)
 - Added brightness support.
 - Added calculated gamma support.
 - Implemented a new method for GECE support utilzing the UART.
 - Better MQTT support (usage detailed in the README).
 - Removed PWM support. A version supporting PWM is maintaned by [penfold42](https://github.com/penfold42/ESPixelBoard).

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

#### 1.2
- Added Administration Page.
- Disconnect from WiFi before rebooting.
- Code cleanup / housekeeping.

#### 1.1
- Migration to UART code for handling WS281x streams.
- Initial GECE support.
- Support for multiple universes.
- Pixel per Universe configuration to allow subsets of multiple universes to be used.
- Removed external library dependencies.
- Added stream timeout and refresh overflow handling.

#### 1.0
- Initial release.
- Single Universe.
- WS281x only.
