# ESPixelStick Web Source

These are the web source files which must be placed in ```data/www``` and uploaded to LittleFS with the [ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin).  All source files (html, css, js) should be minified and gziped before uploading.  A [gulp](http://gulpjs.com) build script is provided in the root of the sketch directory to automate this task, or you can do it manually.

## How to setup and use Gulp

- To install Gulp, you first need to install [Node.js](https://nodejs.org). The latest stable version should be fine.  Make sure the node executable is in your path and that you can execute it from a command prompt.
- After Node.js is installed, open a command prompt and update to the latest version of npm - ```npm install -g npm``` and npx - ```npm install -g npx```
- Install Gulp globally
- Open a command window in administrator mode and change to your development directory
```npm install -g gulp-cli```
- To install Gulp and the dependencies for this project, simply run the following in the root of the project
- Open a command window in administrator mode and change to your development directory
```npm install --global```
```npm install --global gulp```
```npm install --save-dev --global gulp```
- Running ```gulp``` in a command window will minify, gzip, and move all web assets to ```data/www``` for you.
- You can also run ```gulp watch``` and web pages will automatically be processed and moved as they are saved.



## Development

When developing, check the comments at the top and bottom of ```index.html``` to use local sources. You'll need to use a local web server due to the ajax page loading. Python includes a simple one, just run ```python -m http.server``` (python 3) in this directory and connect to ```http://localhost:8000/?target=x.x.x.x```  where ```x.x.x.x``` is a device with the ESPixelStick firmware to use for the websocket connection.  If for some reason your system blocks the default port of 8000, you can pass a port is an option to the embedded python http server.

## 3rd Party Software

The following 3rd party software is included for the web frontend.

- [Bootstrap](http://getbootstrap.com/)
- [jQuery](https://jquery.com/)
- [tinyColorPicker](https://github.com/PitPik/tinyColorPicker)
