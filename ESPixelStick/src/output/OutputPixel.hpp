#pragma once
/*
* OutputPixel.h - Pixel driver code for ESPixelStick
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

class c_OutputPixel : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputPixel (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputPixel ();

    // functions to be provided by the derived class
    // virtual void         Begin () {};
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    virtual void         GetDriverName (String& sDriverName) = 0;
    virtual c_OutputMgr::e_OutputType GetOutputType () = 0;
    virtual void         GetStatus (ArduinoJson::JsonObject& jsonStatus);
    uint16_t             GetNumChannelsNeeded () { return (pixel_count * numIntensityBytesPerPixel); };
    virtual void         SetOutputBufferSize (uint16_t NumChannelsAvailable);

protected:

    IRAM_ATTR void    StartNewFrame ();
    IRAM_ATTR uint8_t GetNextIntensityToSend ();
    void SetPreambleInformation (uint8_t* PreambleStart, uint8_t PreambleSize);

    bool      MoreDataToSend = false;
    uint16_t  InterFrameGapInMicroSec = 300;

    void SetFrameDurration (float IntensityBitTimeInUs, uint16_t BlockSize = 1, float BlockDelayUs = 0.0);

private:
#define PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL 3

    uint8_t*    pNextIntensityToSend = nullptr;     ///< start of output buffer being sent to the UART
    uint16_t    RemainingPixelCount = 0;            ///< Used by ISR to determine how much more data to send
    uint8_t     numIntensityBytesPerPixel = PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL;

    uint8_t*    pPreamble = nullptr;
    uint8_t     PreambleSize = 0;
    uint8_t     PreambleCurrentCount = 0;

    uint8_t     brightness = 100;                   ///< brightness to use
    uint16_t    zig_size = 1;                       ///< Zigsize count - 0 = no zigzag
    uint16_t    ZigPixelCount = 1;
    uint16_t    CurrentZigPixelCount = 1;
    uint16_t    CurrentZagPixelCount = 1;
    uint16_t    group_size = 1;                     ///< Group size - 1 = no grouping
    uint16_t    GroupPixelCount = 1;
    uint16_t    CurrentGroupPixelCount = 1;
    uint16_t    pixel_count = 100;                  ///< Number of pixels
    uint16_t    PrependNullCount = 0;
    uint16_t    CurrentPrependNullCount = 0;
    uint16_t    AppendNullCount = 0;
    uint16_t    CurrentAppendNullCount = 0;
    uint8_t     CurrentIntensityIndex = 0;

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

    uint8_t     gamma_table[256] = { 0 };           ///< Gamma Adjustment table
    float       gamma = 2.2;                        ///< gamma value to use
    uint32_t    AdjustedBrightness = 256;           ///< brightness to use

    // JSON configuration parameters
    String      color_order; ///< Pixel color order

    // Internal variables

    void updateGammaTable(); ///< Generate gamma correction table
    void updateColorOrderOffsets(); ///< Update color order
    bool validate ();        ///< confirm that the current configuration is valid

}; // c_OutputPixel

