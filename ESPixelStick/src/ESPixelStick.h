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

extern void RequestReboot(uint32_t LoopDelay);
extern bool RebootInProgress();

/// Core configuration structure
typedef struct {
    // Device
    String      id;
    time_t      BlankDelay = time_t(5);
} config_t;

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

extern bool setFromJSON (float    & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (double   & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (String   & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (time_t   & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (uint8_t  & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (bool     & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (uint16_t & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (uint32_t & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (int32_t  & OutValue, JsonObject & Json, const char * Name);
extern bool setFromJSON (gpio_num_t  & OutValue, JsonObject & Json, const char * Name);

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
