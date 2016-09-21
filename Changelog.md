
Changelog
=========
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
