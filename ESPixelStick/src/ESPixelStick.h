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

#include "backported.h"

#define ARDUINOJSON_USE_LONG_LONG 1

#include <Ticker.h>
#include <ArduinoJson.h>

#include "memdebug.h"
#include "ConstNames.hpp"
#include "GPIO_Defs.hpp"

#define REBOOT_DELAY    100     ///< Delay for rebooting once reboot flag is set
#define LOG_PORT        Serial  ///< Serial port for console logging
#define CLIENT_TIMEOUT  15      ///< In station/client mode try to connection for 15 seconds
#define AP_TIMEOUT      120     ///< In AP mode, wait 120 seconds for a connection or reboot

#define MilliSecondsInASecond       1000
#define MicroSecondsInAmilliSecond  1000
#define MicroSecondsInASecond       (MicroSecondsInAmilliSecond * MilliSecondsInASecond)
#define NanoSecondsInAMicroSecond   1000
#define NanoSecondsInASecond        (MicroSecondsInASecond * NanoSecondsInAMicroSecond)

#define CPU_ClockTimeNS             ((1.0 / float(F_CPU)) * float(NanoSecondsInASecond))

// Macro strings
#define STRINGIFY(X) #X
#define STRING(X) STRINGIFY(X)

/// Core configuration structure
typedef struct {
    // Device
    String      id;
    time_t      BlankDelay = time_t(5);
} config_t;

String  serializeCore          (bool pretty = false);
void    deserializeCoreHandler (DynamicJsonDocument& jsonDoc);
bool    deserializeCore        (JsonObject & json);
bool    dsDevice               (JsonObject & json);
bool    dsNetwork              (JsonObject & json);
extern  bool reboot;
extern  bool IsBooting;
extern  bool ResetWiFi;
static  const String ConfigFileName = "/config.json";
extern  void FeedWDT ();

template <typename J, typename N>
bool setFromJSON (float & OutValue, J& Json, N Name)
{
    bool HasBeenModified = false;

    if (true == Json.containsKey (Name))
    {
        float temp = Json[Name];
        if (fabs (temp - OutValue) > 0.000005F)
        {
            OutValue = temp;
            HasBeenModified = true;
        }
    }

    return HasBeenModified;
};

template <typename T, typename J, typename N>
bool setFromJSON (T& OutValue, J& Json, N Name)
{
    bool HasBeenModified = false;

    if (true == Json.containsKey (Name))
    {
        T temp = Json[Name];
        if (temp != OutValue)
        {
            OutValue = temp;
            HasBeenModified = true;
        }
    }

    return HasBeenModified;
};

// Template function takes array of characters as **reference**
// to allow template to statically assert buffer is large enough
// to always succeed.  Returns esp_err_t to allow use of
// ESP_ERROR_CHECK() macro (as this is used extensively elsewhere).
template <size_t N>
inline esp_err_t saferRgbToHtmlColorString(char (&output)[N], uint8_t r, uint8_t g, uint8_t b) {
    // Including the trailing null, this string requires eight (8) characters.
    //
    // The output is formatted as "#RRGGBB", where RR, GG, and BB are two hex digits
    // for the red, green, and blue components, respectively.
    static_assert(N >= 8);
    static_assert(sizeof(int) <= sizeof(size_t)); // casting non-negative int to size_t is safe
    int wouldHaveWrittenChars = snprintf(output, N, "#%02x%02x%02x", r, g, b);
    if (likely((wouldHaveWrittenChars > 0) && (((size_t)wouldHaveWrittenChars) < N))) {
        return ESP_OK;
    }
    // TODO: assert ((wouldHaveWrittenChars > 0) && (wouldHaveWrittenChars < N))
    return ESP_FAIL;
}
// Template function takes array of characters as **reference**
// to allow template to statically assert buffer is large enough
// to always succeed.  Returns esp_err_t to allow use of
// ESP_ERROR_CHECK() macro (as this is used extensively elsewhere).
template <size_t N>
inline esp_err_t saferSecondsToFormattedMinutesAndSecondsString(char (&output)[N], uint32_t seconds) {

    // Including the trailing null, the string may require up to twelve (12) characters.
    //
    // The output is formatted as "{minutes}:{seconds}".
    // uint32_t seconds is in range [0..4294967295].
    // therefore, minutes is in range [0..71582788] (eight characters).
    // seconds is always exactly two characters.
    static_assert(N >= 12);
    static_assert(sizeof(int) <= sizeof(size_t)); // casting non-negative int to size_t is safe
    uint32_t m = seconds / 60u;
    uint8_t  s = seconds % 60u;
    int wouldHaveWrittenChars = snprintf(output, N, "%u:%02u", m, s);
    if (likely((wouldHaveWrittenChars > 0) && (((size_t)wouldHaveWrittenChars) < N))) {
        return ESP_OK;
    }
    // TODO: assert ((wouldHaveWrittenChars > 0) && (wouldHaveWrittenChars < N))
    return ESP_FAIL;
}

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
