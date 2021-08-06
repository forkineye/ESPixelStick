# ESPixelStick Firmware

[![Join the chat at https://gitter.im/forkineye/ESPixelStick](https://badges.gitter.im/forkineye/ESPixelStick.svg)](https://gitter.im/forkineye/ESPixelStick)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/ShelbyMerrick)
[![Build Status](https://github.com/forkineye/ESPixelStick/actions/workflows/build.yaml/badge.svg)](https://github.com/forkineye/ESPixelStick/actions/workflows/build.yaml)

## ***Code in this branch is currently being refactored. It is an Alpha state. All features have been developed and feedback is needed on usability***

This is the Arduino based ESPixelStick firmware for ESP8266 and ESP32 platforms. The ESPixelStick firmware supports the control of clockless pixels, DMX512-A devices and relays based upon your hardware platform. Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate utilizing a single port controller like the ESPixelStick V3.  DDP is supported as well along with Alexa and MQTT support for integration into home automation systems.  On platforms with SD cards available, sequences from xLights may be uploaded for playback in either standalone or FPP Remote modes.

## Hardware

Being open source, you are free to use the ESPixelStick firmware on the device of your choice.  The code however is written specifically for the [ESPixelStick](http://forkineye.com/espixelstick). The ESPixelStick V3 utilizes a Wemos D1 Mini module and provides high current connectors, fusing, power filtering, reverse polarity protection, a differential output driver, SD card reader and proper logic level buffering.  The ESPixelStick V3 is available for purchase from [Forkineye](https://forkineye.com/product/espixelstick-v3/) and if you're in the US, it is available via [Amazon](http://amzn.to/2uqBFuX) as well.  The proceeds go towards things like keeping my wife happy so I can work on this project :)  The ESP32 version of the firmware is targeted for the Lolin D32 Pro. At this time, there is not a pre-made ESP32 controller so it is up to the user to roll their own buffer for the WS281x output and add appropiate power connectors. It does however have a SD card reader.

## Build Requirements

The easiest way to get up and going is to download the [latest stable release](https://github.com/forkineye/ESPixelStick/releases/latest) and use ESPSFlashTool within the release archive to flash a pre-compiled binary.  If you're intent on compiling yourself or wish to modify the source code, you'll need the following software to build this project along with the Arduino IDE.

- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266
- [Arduino ESP8266 Filesystem Uploader](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases/latest) - Arduino plugin for uploading files to ESP8266 platforms
- [Adruino for ESP32](https://github.com/espressif/arduino-esp32) - Arduino core for ESP32
- [Arduino ESP32 Filesystem Uploader](https://github.com/lorol/arduino-esp32fs-plugin/releases/latest) - Arduino plugin for uploading files to ESP32 platforms
- [gulp](http://gulpjs.com/) - Build system required to process web sources.  Refer to the html [README](html/README.md) for more information.

The following libraries are required. Extract the folder in each of these zip files and place it in the "library" folder under your arduino environment.

Required for all platforms:

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncE131](https://github.com/forkineye/ESPAsyncE131) - Asynchronous E1.31 (sACN) library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library
- [async-mqtt-client](https://github.com/marvinroger/async-mqtt-client) - Asynchronous MQTT Client
- [Int64String](https://github.com/djGrrr/Int64String) - Converts 64 bit integers into a string
- [EspAlexa](https://github.com/MartinMueller2003/Espalexa) - Alexa Direct control Library
- [Adafruit-PWM-Servo-Driver-Library](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library) - Servo Motor I2C control
- [Artnet](https://github.com/natcl/Artnet) - Artnet access library
- [ArduinoStreamUtils](https://github.com/bblanchon/ArduinoStreamUtils) - Streaming library

Required for ESP8266:

- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncUDP](https://github.com/me-no-dev/ESPAsyncUDP) - Asynchronous UDP Library

Required for ESP32:

- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) - Asynchronous TCP Library
- [LittleFS](https://github.com/lorol/LITTLEFS) - LittleFS for ESP32

## Important Notes on Compiling and Flashing

- ESP-01 modules such as those on the ESPixelStick V1 and V2 are no longer supported as there is not enough flash space. If you have one of these controllers, [ESPixelStick v3.2](https://github.com/forkineye/ESPixelStick/releases/tag/v3.2) is the latest supported release.  At least 4MB of flash is required for ESP8266 platforms.
- Web pages **must** be processed, placed into ```data/www```, and uploaded with the upload plugin. Gulp will process the pages and put them in ```data/www``` for you. Refer to the html [README](html/README.md) for more information.
- In order to use the upload plugin, the ESP **must** be placed into programming mode and the Arduino serial monitor **must** be closed.
- ESP8266 modules **must** be configured for 4MB flash and 2MB File System within the Arduino IDE for OTA updates to work.
- For best performance on ESP8266 modules, set the CPU frequency to 160MHz (Tools->CPU Frequency).  You may experience lag and other issues if running at 80MHz.
- For best performance on ESP32 modules, set the PSRAM option to ENABLED (Tools->CPU Frequency).  You may experience lag and other issues if running at 80MHz.
- Depending on your setings of ```Tools->Erase Flash```, you may have to re-upload the filesystem after uploading the sketch.
- If not performing a full erase during sketch upload, it is reccomended that you do a factory reset via the browser admin page after performing a new sketch upload.

### How to build for the ESP32

- install the Arduino IDE and tell it your default workbook folder is ..\Documents\Arduino.
- In ...\Documents\Arduino create two sub directories
  - ...\Documents\Arduino\Libraries
  - ...\Documents\Arduino\Tools
- Download / Clone the libraries identified in this readme (above) into sub directories in the ...\Arduino\libraries directory
- Download the littleFS utility into the Tools directory
- Clone ESPixelStick-Unify into ...\Documents\Arduino\ESPixelStick
- Create the ...\Documents\Arduino\ESPixelStick\src\secrets.h file and enter your WiFi ID and WiFi Password as described in WiFiMgr.cpp
- Connect your LoLin D32 PRO to the computer using a USB cable (not the Serial Port the gets opened)
- In the ESPixelStick directory double click on the ESPixelStick.ino file
- In the Arduino IDE click: File->Preferences
  - Make sure the Sketchbook location is set to point at your Arduino directory (note ESPixelStick dir)
- Set the following into the "Additional Boards Manager URLs" field: ```https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json,https://arduino.esp8266.com/stable/package_esp8266com_index.json```
- Exit the preferences menu
- Click on Tools->Board->ESP32->LOLIN D32 PRO
- Next click on Tools->PSRAM->enabled
- Set the port on which the LoLin is connected: Tools->port->YourPortNumberGoesHere
- Click Sketch->Upload - This will build and upload to the LoLin device

## Supported Inputs

The configuration has been modified to support multiple (two) input types concurrently. For example you can select E1.31 and Effects at the same time. E1.31 will have higher priority than Effects. Valid input types are:

- Alexa
- DDP
- E1.31
- Effect Engine
- FPP Remote / FSEQ file auto Play / Play Lists
- MQTT

### Alexa Support

Alexa is supported in direct mode. No additional hubs or applications are needed. When Alexa is selected as an input mode, the ESP will be discoverable by the Alexa app as a device that supports 24 bit color. The entire output buffer will be treated (from Alexa's point of view) as a single light bulb. Connect by giving your device a name in the device config screen and then telling alexe "alexa discover new devices"
When adding the ESPixelstick using the alexa app: Select devices, add device, device type = light, manufacturer is "other", discover devices.

### DDP

No additional configuration is needed for DDP Support. Simply select it as an input mode and it will listen for DDP messages being sent to the ESP

### E1.31

E1.31 requires additional configuration:

- Starting Universe
- Channels / universe
- Offset into the first universe to first channel (Typically zero)

### Effect Engine

Effect engine provides a list of effects and colors for the effects. Effects will repeat until the engine is turned off.

### FPP Remote / Play FSEQ

FPP / FSEQ support allows the ESP to play files stored on a local SD card. The configuration allows the SPI pins used for the SD Card to be set.

- When the "FSEQ File to Play" configuration parameter is set to "Play Remote Sequence", the ESP will respond to FPP Sync and play commands.
- When the "FSEQ File to Play" configuration parameter is set to one of the files stored on the ESP, the ESP will play that file on an endless loop.
- When the "FSEQ File To Play" configuration parameter is set to a file with a .pl extension, the file will be parsed and used to control a set of actions performed by the FPP Play List Player.

#### Play List

The ESPixelstick can follow a set of instructions found in a play list file. A playlist file can have any name and up to 20 actions to take. Play List actions are one of the following:

- Play a file (must be an fseq file)
  - Requires the Filename and the number of times to play the file (max 255)
- Play an effect (Effect list can be found in the Effect configuration dropdown)
  - Requires an effect configuration that includes how long to play the effect (max 1000 seconds)
- Pause
  - Requires a time value (up to 1000 seconds)
- The play list file is case sensitive.
- The effect configuration sections must be present for the effects to function properly.

Here is an example of a Play List File:

```json
[
    {
        "type": "file",
        "name": "MySequence.fseq",
        "playcount": 2
    },
    {
        "type": "pause",
        "duration": 5
    },
    {
        "type": "effect",
        "duration": 10,
        "config": {
            "currenteffect": "Chase",
            "EffectSpeed": 6,
            "EffectReverse": false,
            "EffectMirror": false,
            "EffectAllLeds": false,
            "EffectBrightness": 1,
            "EffectBlankTime": 0,
            "EffectWhiteChannel": false,
            "EffectColor": "#0000ff"
        }
    },
    {
        "type": "effect",
        "duration": 5,
        "config": {
            "currenteffect": "Rainbow",
            "EffectSpeed": 6,
            "EffectReverse": false,
            "EffectMirror": false,
            "EffectAllLeds": false,
            "EffectBrightness": 1,
            "EffectBlankTime": 0,
            "EffectWhiteChannel": false,
            "EffectColor": "#b700ff"
        }
    },
    {
        "type": "effect",
        "duration": 10,
        "config": {
            "currenteffect": "Blink",
            "EffectSpeed": 6,
            "EffectReverse": false,
            "EffectMirror": false,
            "EffectAllLeds": false,
            "EffectBrightness": 1,
            "EffectBlankTime": 0,
            "EffectWhiteChannel": false,
            "EffectColor": "#FF0055"
        }
    },
    {
        "type": "file",
        "name": "MyOtherSequence.fseq",
        "playcount": 1
    },
    {
        "type": "pause",
        "duration": 5
    }
]
```

### MQTT Support

MQTT can be configured via the web interface.  When enabled, a payload of "ON" will tell the ESPixelStick to override any incoming E1.31 data with MQTT data.  When a payload of "OFF" is received, E1.31 processing will resume.  The configured topic is used for state, and the command topic will be the state topic appended with ```/set```.

For example, if you enter ```porch/esps``` as the topic, the state can be queried from ```porch/esps``` and commands can be sent to ```porch/esps/set```

If using [Home Assistant](https://home-assistant.io/), it is recommended to enable Home Assistant Discovery in the MQTT configuration.  Your ESPixelStick along with all effects will be automatically imported as an entity within Home Assistant utilzing "Device ID" as the friendly name.  For manual configuration, you can use the following as an example.  When disabling Home Assistant Discovery, ESPixelStick will attempt to remove its configuration entry from your MQTTT broker.

```yaml
light:
  - platform: mqtt
    schema: json
    name: "Front Porch ESPixelStick"
    state_topic: "porch/esps"
    command_topic: "porch/esps/set"
    brightness: true
    rgb: true
    effect: true
    effect_list:
      - Solid
      - Blink
      - Flash
      - Rainbow
      - Chase
      - Fire flicker
      - Lightning
      - Breathe
      - playFseq
```

Here is an example of playing an fseq file via mqtt

```yaml
light:
  - platform: mqtt
    schema: json
    name: "Porch ESPixelStick"
    state_topic: "porch/esps"
    command_topic: "porch/esps/set"
    effect: playFseq
    filename: NameOfFileToPlay
    count: 5
```

Here's an example using the mosquitto_pub command line tool:

```bash
mosquitto_pub -t porch/esps/set -m '{"state":"ON","color":{"r":255,"g":128,"b":64},"brightness":255,"effect":"solid","reverse":false,"mirror":false}'
```

An example of playing a file three times:

```bash
mosquitto_pub -t porch/esps/set -m '{"state":"ON","brightness":255,"effect":"playFseq","filename":"ESPTuneToTest.fseq","count":3}'
```

### Additional Input Features

The input channels will respond to FPP and xLights discovery requests. xLights also has the ability to auto configure the ESP.

## Supported Outputs

The ESPixelStick firmware can generate the following outputs from incoming data streams, however your hardware must support the physical interface.

ESP8266-12F platforms:

- Max 2400 Channels (800 RGB pixels)
- Single Serial / Pixel output
    - GPIO_NUM_2, UART_NUM_1,
- 8 Relay Outputs
- 16 PWM Outputs (I2C bus)
    - GPIO_NUM_21,  SDA
    - GPIO_NUM_22,  SCL
- SD Card GPIO
    - GPIO_NUM_23,  MOSI
    - GPIO_NUM_19,  MISO
    - GPIO_NUM_18,  SCK
    - GPIO_NUM_15,  CS

ESP32 platforms:

- Max 9000 Channels (3000 RGB pixels)
- Two Serial / Pixel outputs
    - GPIO_NUM_2,  UART_NUM_1,
    - GPIO_NUM_13, UART_NUM_2
- Eight RMT based pixel outputs
    - GPIO_NUM_12,  RMT (0)
    - GPIO_NUM_14,  RMT (1)
    - GPIO_NUM_32,  RMT (2)
    - GPIO_NUM_33,  RMT (3)
    - GPIO_NUM_25,  RMT (4)
    - GPIO_NUM_26,  RMT (5)
    - GPIO_NUM_27,  RMT (6)
    - GPIO_NUM_34,  RMT (7)
- 8 Relay outputs
- 16 PWM Outputs (I2C bus)
    - GPIO_NUM_21,  SDA
    - GPIO_NUM_22,  SCL
    - SD Card GPIO
    - GPIO_NUM_23,  MOSI
    - GPIO_NUM_19,  MISO
    - GPIO_NUM_18,  SCK
    - GPIO_NUM_4,   CS

Each Serial / Pixel output can be configured to support any of the output protocols.
RMT Ports only support WS281x pixels

### Pixel Protocols

- WS2811 / WS2812 / WS2812b (WS281x)
- GE Color Effects

### Serial Protocols

- DMX-512A
- Renard
- Generic Serial

### Relay Outputs

We support an output configuration that drives up to eight (8) relay outputs.

- Each relay output can be configured to be active high or active low.
- The channel intensity trip point (on/off) is configurable per output.
- The GPIO for each output is configurable

### PWM Outputs

We support up to 16 PWM outputs via the I2C bus. Each output can be configured to map the 8 bit channel intensity values to 4096 PWM timings.
PWM is supported via the PCM9685 16 channel PWM output module.
This CAN be used to drive DC SSRs to support upto 5 dumb RGB light strings.

## Configuration Backup / Restore

The current device configuration can be saved to a local drive using the Backup button on the Admin page. Pressing the Backup button will result in a local file save operation to the default download directory. The file will be named using the Device name from the device configuration page PLUS the ESP unique ID found on the admin page.
The current configuration can be over written from a file located on the local drive (in any directory you chose) by pressing the Restore button on the admin page. A popup will help you select the file to upload.
NOTE: A restore is an OVER WRITE, not a merge. Any changes made since the backup file was created will be lost.

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
- [Martin Mueller](https://github.com/MartinMueller2003) Port to ESP32. Clean up the unify branch. Added Alexa, Play FIle, Relay and native PWM supprt.
