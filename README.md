# ESPixelStick Firmware

[![Build Status](https://github.com/forkineye/ESPixelStick/actions/workflows/build.yml/badge.svg)](https://github.com/forkineye/ESPixelStick/actions/workflows/build.yml)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/ShelbyMerrick)

This is the Arduino based ESPixelStick firmware for ESP8266 and ESP32 platforms. The ESPixelStick firmware supports the control of clock-less pixels, DMX512 devices and relays based upon your hardware platform. Pixel limitations are mostly based upon your desired refresh rate, around 800 pixels for a 25ms E1.31 source rate utilizing a single port controller like the ESPixelStick V3. DDP is supported as well along with Alexa and MQTT support for integration into home automation systems. On platforms with SD cards available, sequences from xLights may be uploaded for playback in either standalone or FPP Remote modes.

ESP-01 modules such as those on the ESPixelStick V1 and V2 are no longer supported as there is not enough flash space. If you have one of these controllers, [ESPixelStick v3.2](https://github.com/forkineye/ESPixelStick/releases/tag/v3.2) is the latest supported release. At least 4MB of flash is required for ESP8266 platforms.

## Installation

The recommended installation method is to download the latest [stable release](https://github.com/forkineye/ESPixelStick/releases/latest) and use ESPSFlashTool within the release archive to flash a pre-compiled binary. Beta builds and Release Candidates will be tagged as [Pre-release](https://github.com/forkineye/ESPixelStick/releases) when available and ready for testing.

If you are interested in bleeding edge / un-tested builds, automated CI builds are generated for every code push and are available as Artifact attachments to the [ESPixelStick CI](https://github.com/forkineye/ESPixelStick/actions/workflows/build.yaml) workflow runs. Just click on the latest successful run and look for **Release Archive** towards the bottom. Note to download Artifact attachments though, you will have to be logged into GitHub.

There is also an experimental [web based flash tool](https://espixelstickwebflasher.from-ct.com:5000) available. This tool will always have a few of the most recent CI builds available for flashing to your device.

If you would like to compile the project yourself and modify the source code, go down to [Build Requirements](#build-requirements).

## Hardware

Being open source, you are free to use the ESPixelStick firmware on the device of your choice. The code however is written specifically for the [ESPixelStick](http://forkineye.com/espixelstick). The ESPixelStick V3 utilizes a Wemos D1 Mini module and provides high current connectors, fusing, power filtering, reverse polarity protection, a differential output driver, SD card reader and proper logic level buffering. The ESPixelStick V3 is available for purchase from [Forkineye](https://forkineye.com/product/espixelstick-v3/). The proceeds go towards things like keeping my wife happy so I can work on this project :) At this time, there is not a pre-made ESP32 controller so it is up to the user to roll their own buffer for the WS281x output and add appropriate power connectors. A wide variety of ESP8266 and ESP32 platforms have been added to the supported devices list (37 as of this writting), all of which have artifacts created every build.

## Build Requirements

The recommended way to build ESPixelStick is with PlatformIO. Building from the Arduino IDE is no longer supported.

### Platform IO Instructions

- Download and install [Visual Studio Code](https://code.visualstudio.com/).
- Follow [these instructions](https://platformio.org/install/ide?install=vscode) to install PlatformIO IDE for Visual Studio Code.
- Either [download](https://github.com/forkineye/ESPixelStick/archive/refs/heads/main.zip) and extract or [clone](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository) the ESPixelStick repository.
- Download and install [Node.js](https://nodejs.org/) and [Gulp](http://gulpjs.com/) to build the web pages. Refer to the html [README](html/README.md) for more information.
- Copy `platformio_user.ini.sample` to `platformio_user.ini`.
- Open the project folder in Visual Studio Code.
- Open `platform_user.ini` and define which serial port(s) you are using for programming and monitoring your device(s).
- In the status bar at the bottom of Visual Studio Code, select your target build environment. By default, it will say `Default (ESPixelStick)`. Build environments are defined in `platformio.ini` if you need more information on build targets.
- In the same status bar, click ☑️ to compile or ➡️ to compile and upload.
- To build and upload the filesystem, click on the PlatformIO icon on the left bar, then click on _Project Tasks->Platform->Upload Filesystem Image_. Note that before the filesystem is built, the web pages **must** be processed. Instructions for processing the web pages are in the html [README](html/README.md).

## Project Structure

ESPixelStick is now a PlatformIO project and follows the standard PlatformIO directory structure. All code should be in the releveant `src` and `include` sub-directories. Within each you will find the follwing directories:

- `input` - Show input modules should reside here and inherit from `c_InputCommon` as defined in `InputCommon.hpp`.
- `ouput` - Show output modules should reside here and inherit from `c_OutputCommon` as defined in `OutputCommon.hpp`.
- `service` - Auxiliary services should reside here. These include non-show critical input mechansims like FPP discovery / configuration mechanisms.

You'll want to refer to `src/input/InputMgr.cpp` and `src/output/OutputMgr.cpp` for adding modules to the system.

Other notable directories:

- `dist` - These are the files used for creating the binary distributions. Updated binaries for flashing, ESPSFlashTool and `firmware.json` should all be updated here.
- `html` - All frontend / web related code, refer to the html [README](html/README.md) for details.
- `SupportingDocs` - Datasheets and protocol formats related to this project.
- `tools` = Misc tools related to this project, refer to the tools [README](toosl/README.md) for details.

Checkout the doxygen [source code documentation](https://forkineye.github.io/ESPixelStick/doxy/) which is generated every pipeline run.

## Supported Inputs and Outputs

Details on the supported input protocols and outputs can be found in the [wiki](https://github.com/forkineye/ESPixelStick/wiki/Supported-Inputs-and-Outputs).

## Configuration Backup / Restore

The current device configuration can be saved to a local drive using the Backup button on the Admin page. Pressing the Backup button will result in a local file save operation to the default download directory. The file will be named using the Device name from the device configuration page PLUS the ESP unique ID found on the admin page.
The current configuration can be over written from a file located on the local drive (in any directory you chose) by pressing the Restore button on the admin page. A popup will help you select the file to upload.
NOTE: A restore is an OVER WRITE, not a merge. Any changes made since the backup file was created will be lost.

## SD File Management

The file management page is available on the UI when an SD card is detected by the ESP. You can add / remove files using the file management screen. It is known that the HTTP based file transfer used by the File Management screen can be very slow for large files. Alternativly, you can use your favorite FTP client (tested with FileZilla) to transfer files to the ESP. NOTE: You must configure the FTP client to run in single connection (port) mode. The ESP does not have enough resources to support concurrent command and data connections.

## Resources

- Firmware: [http://github.com/forkineye/ESPixelStick](http://github.com/forkineye/ESPixelStick)
- Hardware: [http://forkineye.com/ESPixelStick](http://forkineye.com/ESPixelStick)

## Credits

- The great people at [diychristmas.org](http://diychristmas.org) and [doityourselfchristmas.com](http://doityourselfchristmas.com) for inspiration and support.
- [Bill Porter](https://github.com/madsci1016) for initial Renard and SoftAP support.
- Bill Porter and [Grayson Lough](https://github.com/GraysonLough) for initial DMX support.
- [Rich Danby](https://github.com/cinoan) for fixes and helping polish the front-end.
- [penfold42](https://github.com/penfold42) for fixes, brightness, gamma support, and zig-zag / grouping.
- [Austin Hodges](https://github.com/ahodges9) for effects support and MQTT cleanup.
- [Matthias C. Hormann](https://github.com/Moonbase59) MQTT & effects cleanup.
- [Martin Mueller](https://github.com/MartinMueller2003) Port to ESP32. Clean up the unify branch. Added Alexa, Play FIle, Relay and native PWM support.
