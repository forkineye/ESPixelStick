ESPixelStick Binaries
=====================
This is a precompiled firmware release for the ESPixelStick hardware.  It contains everything you need to flash or update your [ESPixelStick](ESPixelStick.html).  For a list of changes, please refer to the [Changelog](Changelog.html) included with this release.  To use the web based updates, you must be on ESPixelStick firmware 2.0 or greater.

How to use
----------
**ESPSFlashTool.jar** - This is a java based flash tool for flashing new modules or modules that can't be updated via the web interface.  It will flash the ESP module with the ESPixelStick firmware to a default state and overwrite any previous settings.  Simply fill in the boxes, select your mode and serial port, put your ESPixelStick in programming mode and hit "Upload".

**spiffs/config.json** - This is the default configuration file in JSON format that will be applied when you use ESPSFlashTool to flash your ESP modules. Everything in this file is configurable via the web interface. Normally, you shouldn't have to edit anything in this file unless you need to configure static networking.  It's recommended to leave as-is.

**espixelstick\*.efu** - These are web based firmware updates. Simply upload these via the web interface to flash your ESPixelStick.  Your configuration will be saved and applied to the new firmware.
