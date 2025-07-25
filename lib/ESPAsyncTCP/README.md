![https://avatars.githubusercontent.com/u/195753706?s=96&v=4](https://avatars.githubusercontent.com/u/195753706?s=96&v=4)

# ESPAsyncTCP 

Project moved to [ESP32Async](https://github.com/ESP32Async) organization at [https://github.com/ESP32Async/ESPAsyncTCP](https://github.com/ESP32Async/ESPAsyncTCP)

Discord Server: [https://discord.gg/X7zpGdyUcY](https://discord.gg/X7zpGdyUcY)

Please see the new links:

- `ESP32Async/ESPAsyncWebServer @ 3.6.0` (ESP32, ESP8266, RP2040)
- `ESP32Async/AsyncTCP @ 3.3.2` (ESP32)
- `ESP32Async/ESPAsyncTCP @ 2.0.0` (ESP8266)
- `https://github.com/ESP32Async/AsyncTCPSock/archive/refs/tags/v1.0.3-dev.zip` (AsyncTCP alternative for ESP32)
- `khoih-prog/AsyncTCP_RP2040W @ 1.2.0` (RP2040)

### Async TCP Library for ESP8266 Arduino

For ESP32 look [https://github.com/ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP)

This is a fully asynchronous TCP library, aimed at enabling trouble-free, multi-connection network environment for Espressif's ESP8266 MCUs.

This library is the base for [https://github.com/ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)

## AsyncClient and AsyncServer
The base classes on which everything else is built. They expose all possible scenarios, but are really raw and require more skills to use.

## AsyncPrinter
This class can be used to send data like any other ```Print``` interface (```Serial``` for example).
The object then can be used outside of the Async callbacks (the loop) and receive asynchronously data using ```onData```. The object can be checked if the underlying ```AsyncClient```is connected, or hook to the ```onDisconnect``` callback.

## AsyncTCPbuffer
This class is really similar to the ```AsyncPrinter```, but it differs in the fact that it can buffer some of the incoming data.

## SyncClient
It is exactly what it sounds like. This is a standard, blocking TCP Client, similar to the one included in ```ESP8266WiFi```

## Libraries and projects that use AsyncTCP
- [ESP Async Web Server](https://github.com/ESP32Async/ESPAsyncWebServer)
- [Async MQTT client](https://github.com/marvinroger/async-mqtt-client)
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets)
- [ESP8266 Smart Home](https://github.com/baruch/esp8266_smart_home)
- [KBox Firmware](https://github.com/sarfata/kbox-firmware)
