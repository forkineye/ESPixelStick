#pragma once
/*
* OutputWS2811.h - WS2811 driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "OutputCommon.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputWS2811 : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2811 ();

    // functions to be provided by the derived class
    virtual void         Begin () {};
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    virtual void         Render () {};
            void         GetDriverName (String & sDriverName) { sDriverName = String (F ("WS2811")); }
    c_OutputMgr::e_OutputType GetOutputType () {return c_OutputMgr::e_OutputType::OutputType_WS2811;} ///< Have the instance report its type.
    virtual void         GetStatus (ArduinoJson::JsonObject& jsonStatus);
    uint16_t             GetNumChannelsNeeded () { return (pixel_count * numIntensityBytesPerPixel); };
    virtual void         SetOutputBufferSize (uint16_t NumChannelsAvailable);
    virtual void         PauseOutput () {};

#define WS2811_MICRO_SEC_PER_INTENSITY  10L     // ((1/800000) * 8 bits) = 10us
#define WS2811_MIN_IDLE_TIME            300L    ///< 300us idle time

protected:
    uint8_t   * pNextIntensityToSend = nullptr;     ///< start of output buffer being sent to the UART
    uint16_t    RemainingPixelCount = 0;            ///< Used by ISR to determine how much more data to send
    uint8_t     numIntensityBytesPerPixel = 3;      ///< number of bytes per pixel
    uint8_t     gamma_table[256] = { 0 };           ///< Gamma Adjustment table
    float       gamma = 2.2;                        ///< gamma value to use
    uint8_t     AdjustedBrightness = 255;           ///< brightness to use
    uint8_t     brightness = 100;                   ///< brightness to use
    uint16_t    zig_size = 0;                       ///< Zigsize count - 0 = no zigzag
    uint16_t    ZigPixelCount = 0;
    uint16_t    CurrentZigPixelCount = 0;
    uint16_t    CurrentZagPixelCount = 0;
    uint16_t    group_size = 0;                     ///< Group size - 1 = no grouping
    uint16_t    GroupPixelCount = 0;
    uint16_t    CurrentGroupPixelCount = 0;
    uint16_t    pixel_count = 100;                  ///< Number of pixels

    typedef union ColorOffsets_s
    {
        struct offsets
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t w;
        } offset;
        uint8_t Array[4];
    } ColorOffsets_t;
    ColorOffsets_t  ColorOffsets;

    // JSON configuration parameters
    String      color_order; ///< Pixel color order

    // Internal variables
    uint16_t    InterFrameGapInMicroSec = 0;

    void updateGammaTable(); ///< Generate gamma correction table
    void updateColorOrderOffsets(); ///< Update color order
    bool validate ();        ///< confirm that the current configuration is valid

}; // c_OutputWS2811

