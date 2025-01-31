#pragma once
/*
* ESPixelStick.h
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2016, 2022 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
#	include <ESP8266WiFi.h>
#	include <ESPAsyncTCP.h>
#	include <ESPAsyncUDP.h>
#elif defined(ARDUINO_ARCH_ESP32)
#   include <AsyncTCP.h>
#   include <AsyncUDP.h>
#   include <WiFi.h>
#else
#	error "Unsupported CPU type"
#endif

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 15

#include <Ticker.h>
#include <ArduinoJson.h>

#include "memdebug.h"
#include "ConstNames.hpp"
#include "GPIO_Defs.hpp"
#include "FastTimer.hpp"

#define REBOOT_DELAY    100     ///< Delay for rebooting once reboot flag is set
#define LOG_PORT        Serial  ///< Serial port for console logging
#define CLIENT_TIMEOUT  15      ///< In station/client mode try to connection for 15 seconds
#define AP_TIMEOUT      120     ///< In AP mode, wait 120 seconds for a connection or reboot

#define MilliSecondsInASecond       1000
#define MicroSecondsInAmilliSecond  1000
#define MicroSecondsInASecond       (MicroSecondsInAmilliSecond * MilliSecondsInASecond)
#define NanoSecondsInAMicroSecond   1000
#define NanoSecondsInASecond        (MicroSecondsInASecond * NanoSecondsInAMicroSecond)
#define NanoSecondsInAMilliSecond   (NanoSecondsInAMicroSecond * MicroSecondsInAmilliSecond)

#define CPU_ClockTimeNS             ((1.0 / float(F_CPU)) * float(NanoSecondsInASecond))

// Macro strings
#define STRINGIFY(X) #X
#define STRING(X) STRINGIFY(X)

extern void RequestReboot(uint32_t LoopDelay, bool SkipDisable = false);
extern bool RebootInProgress();

/// Core configuration structure
struct config_t
{
    // Device
    String      id;
    uint32_t    BlankDelay = uint32_t(5);
};

String  serializeCore          (bool pretty = false);
void    deserializeCoreHandler (JsonDocument& jsonDoc);
bool    deserializeCore        (JsonObject & json);
bool    dsDevice               (JsonObject & json);
bool    dsNetwork              (JsonObject & json);

extern  bool IsBooting;
extern  bool ResetWiFi;
extern  const String ConfigFileName;
extern  void FeedWDT ();
extern  uint32_t DiscardedRxData;

extern void PrettyPrint (JsonObject& jsonStuff, String Name);
extern void PrettyPrint (JsonArray& jsonStuff, String Name);
extern void PrettyPrint(JsonDocument &jsonStuff, String Name);

template <typename T, typename N>
bool setFromJSON (T& OutValue, JsonObject & Json, N Name)
{
    bool HasBeenModified = false;

    if (Json[(char*)Name].template is<T>())
    {
        T temp = Json[(char*)Name];
        if (temp != OutValue)
        {
            OutValue = temp;
            HasBeenModified = true;
        }
    }
    else
    {
        DEBUG_V(String("Could not find field '") + Name + "' in the json record");
        PrettyPrint (Json, Name);
    }

    return HasBeenModified;
};

template <typename T, typename N>
bool setFromJSON (T& OutValue, JsonVariant & Json, N Name)
{
    bool HasBeenModified = false;

    if (Json[(char*)Name].template is<T>())
    {
        T temp = Json[(char*)Name];
        if (temp != OutValue)
        {
            OutValue = temp;
            HasBeenModified = true;
        }
    }
    else
    {
        DEBUG_V(String("Could not find field '") + Name + "' in the json record");
        PrettyPrint (Json, Name);
    }

    return HasBeenModified;
};

#if defined(ARDUINO_ARCH_ESP8266)
#   define JsonWrite(j, n, v)  (j)[String(n)] = (v)
void inline ResetGpio(const gpio_num_t pinId)
{
    if(gpio_num_t(33) > pinId)
    {
        pinMode(pinId, INPUT);
    }
}
#else // defined(ARDUINO_ARCH_ESP32)
#   define JsonWrite(j, n, v)  (j)[(char*)(n)] = (v)
void inline ResetGpio(const gpio_num_t pinId)
{
    if(GPIO_IS_VALID_OUTPUT_GPIO(pinId))
    {
        gpio_reset_pin(pinId);
        pinMode(pinId, INPUT);
    }
}
#endif

extern bool ConsoleUartIsActive;
#define logcon(msg) \
{ \
    String DN; \
    GetDriverName (DN); \
    extern void _logcon (String & DriverName, String Message); \
    _logcon (DN, msg); \
}

extern config_t config;
extern bool ConfigSaveNeeded;

extern const uint8_t CurrentConfigVersion;
#define LOAD_CONFIG_DELAY 4
// #define DEBUG_GPIO gpio_num_t::GPIO_NUM_25
// #define DEBUG_GPIO1 gpio_num_t::GPIO_NUM_14
