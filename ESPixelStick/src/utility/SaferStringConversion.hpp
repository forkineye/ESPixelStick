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

#include "../ESPixelStick.h"
#include "./backported.h"
#include <time.h>
 
// The following template functions' first parameter is defined
// as a **reference** to an array of characters.  The size of
// the array is the template parameter.  This allows the function
// to statically assert that the passed-in buffer is large enough
// to always succeed.  Else, a compilation will occur.
//
// Allowing the compiler to deduce the template parameters has
// multiple benefits:
// * Users can ignore that the function is a template
// * Code remains easy to read
// * Compiler doesn't deduce wrong size
// * Even if a user manually enters a larger template size,
//   the compiler will report an error, such as:
//   error: invalid initialization of reference of type 'char (&)[15]'
//          from expression of type 'char [12]'



// Safer RGB to HTML string (e.g., "#FF8833") conversion function
//
// Example use:
//     char foo[8];
//     ESP_ERROR_CHECK(saferRgbToHtmlColorString(foo, led.r, led.g, led.b));
//
template <size_t N>
inline esp_err_t saferRgbToHtmlColorString(char (&output)[N], uint8_t r, uint8_t g, uint8_t b) {
    esp_err_t result = ESP_FAIL;

    // Including the trailing null, this string requires eight (8) characters.
    //
    // The output is formatted as "#RRGGBB", where RR, GG, and BB are two hex digits
    // for the red, green, and blue components, respectively.
    static_assert(N >= 8);
    static_assert(sizeof(int) <= sizeof(size_t)); // casting non-negative int to size_t is safe
    int wouldHaveWrittenChars = snprintf(output, N, "#%02x%02x%02x", r, g, b);
    if (likely((wouldHaveWrittenChars > 0) && (((size_t)wouldHaveWrittenChars) < N))) {
        result = ESP_OK;
    } else {
        // TODO: assert((wouldHaveWrittenChars > 0) && (wouldHaveWrittenChars < N));
    }
    return result;
}
// Safer seconds to "Minutes:Seconds" string conversion function
//
// Example use:
//     char foo[12];
//     ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(foo, seconds));
//
template <size_t N>
inline esp_err_t saferSecondsToFormattedMinutesAndSecondsString(char (&output)[N], uint32_t seconds) {
    esp_err_t result = ESP_FAIL;

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
        result = ESP_OK;
    } else {
        // TODO: assert((wouldHaveWrittenChars > 0) && (wouldHaveWrittenChars < N));
    }
    return result;
}
// Safer `tm` to `yyyy-MM-dd hh:mm:ss` format (RFC1123, using space instead of `T`)
// Example use:
//     char foo[8];
//     ESP_ERROR_CHECK(saferRgbToHtmlColorString(foo, led.r, led.g, led.b));
//
template <size_t N>
inline esp_err_t saferTmToSortableString(char (&output)[N], const struct tm &tm) {
    esp_err_t result = ESP_FAIL;

    // The output is formatted as "yyyy-MM-dd hh:mm:ss", where:
    //     yyyy is four-digit year
    //     MM   is two-digit month (e.g. 01 for January)
    //     dd   is two-digit day in that month
    //     hh   is two-digit hour (24-hour format)
    //     mm   is two-digit minutes
    //     ss   is two-digit seconds

    // Including the trailing null, this string requires (20) characters,
    // (at least through 9999-12-31 11:59:59 ... which should be sufficient)
    //    "9999-12-31 24:59:59"
    //     ....-....1....-....2
    //     1234 1234 1234 1234
    static_assert(N >= 20);
    output[0] = (char)0;

    // esp01 build emits warning if do not explicitly prevent overflow
    // (e.g., negative values, large values),  even though using snprintf()
    // and returning error code on failures.
    // THIS IS ***NOT*** FULLY VALIDATING THE VALUES!
    // For example, this might result in invalid date: "2000-02-31 23:59:60"
    if (       unlikely((tm.tm_year < -1900) || (tm.tm_year > 8100))) {
    } else if (unlikely((tm.tm_mon  <     0) || (tm.tm_mon  >   12))) {
    } else if (unlikely((tm.tm_mday <     1) || (tm.tm_mday >   31))) {
    } else if (unlikely((tm.tm_hour <     0) || (tm.tm_hour >   24))) { // not a mistake ... leap seconds!
    } else if (unlikely((tm.tm_min  <     0) || (tm.tm_min  >   59))) {
    } else if (unlikely((tm.tm_sec  <     0) || (tm.tm_sec  >   69))) { // not a mistake ... leap seconds!
    } else {

#if defined(ARDUINO_ARCH_ESP8266)
// Older compiler used for ESP8266 has false warning below ...
// Keep this diagnostic enabled for other platforms (e.g., ESP32)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
        int wouldHaveWrittenChars =
            snprintf(
                output, N,
                "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
                1900 + tm.tm_year,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec
                );
#if defined(ARDUINO_ARCH_ESP8266)
#   pragma GCC diagnostic pop
#endif

        if (likely((wouldHaveWrittenChars > 0) && (((size_t)wouldHaveWrittenChars) < N))) {
            result = ESP_OK;
        } else {
            // TODO: assert((wouldHaveWrittenChars > 0) && (wouldHaveWrittenChars < N));
        }
    }

    return result;
}
