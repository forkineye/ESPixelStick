ESPixelStick Web Source
=======================
These are the web source files which must be placed in ```data/www``` and uploaded to SPIFFS with the [ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin).  All source files (html, css, js) should be minified and gziped before uploading.  A [gulp](http://gulpjs.com) build script is provided in the root of the sketch directory to automate this task, or you can do it manually.

### How to setup and use Gulp
- To install Gulp, you first need to install [Node.js](https://nodejs.org). The latest stable version should be fine.  Make sure the node executable is in your path and that you can execute it from a command prompt.
- After Node.js is installed, open a command prompt and update to the latest version of npm - ```npm install -g npm```
- Install Gulp globally - ```npm install -g gulp-cli```
- To install Gulp and the dependencies for this project, simply run the following in the root of the project - ```npm install```
- Running ```gulp``` will minify, gzip, and move all web assets to ```data/www``` for you.  You can also run ```gulp watch``` and web pages will automatically be processed and moved as they are saved.

3rd Party Software
------------------
The following 3rd party software is included for the web frontend.
- [Bootstrap](http://getbootstrap.com/)
- [jQuery](https://jquery.com/)
- [tinyColorPicker](https://github.com/PitPik/tinyColorPicker)
