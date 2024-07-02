ESPixelStick Binaries
=====================
This is a precompiled firmware release for the ESPixelStick hardware.  It contains everything you need to flash or update your [ESPixelStick](ESPixelStick.html).  For a list of changes, please refer to the [Changelog](Changelog.html) included with this release.  To use the web based updates, you must be on ESPixelStick Firmware 4.0.0 or greater.  Note that upgrades from beta and release candidate versions may not work.

How to use
----------
**ESPSFlashTool.jar** - This is a java based flash tool for flashing new modules or modules that can't be updated via the web interface.  It will flash the ESP module with the ESPixelStick firmware to a default state and overwrite any previous settings.  To launch the tool, you will need [Java 8](https://www.java.com) or greater. On Linux and macOS platforms, you will need launch the tool from a terminal within the extracted directory (```java -jar ESPSFlashTool.jar```).  Additionally for macOS, you will have to jump through Apple's typical security hoops to allow non-Apple signed software to run.  Once launched, simply fill in the boxes, select your harwdare and serial port, and click "Upload".

**espixelstick\*.efu** - These are web based firmware updates. Simply upload these via the web interface to flash your ESPixelStick.  Your configuration will be saved and applied to the new firmware.  Not that web upgrades between major releases, betas, and release candidates are not supported.

If you are interested in using the experimental web based flash tool, connect to:
HTTPS://espixelstickwebflasher.from-ct.com:5000
This tool will always have a few of the most recent CI builds available for flashing to your device. This tool has only been tested with the Chrome browser. You do not need to install anything on your PC. Just connect the ESP using a USB cable and follow the instructions.