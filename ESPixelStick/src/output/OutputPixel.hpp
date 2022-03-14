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
            size_t       GetNumChannelsNeeded () { return (pixel_count * NumIntensityBytesPerPixel); };
    virtual void         SetOutputBufferSize (size_t NumChannelsAvailable);
            bool         MoreDataToSend () { return _MoreDataToSend; }
    IRAM_ATTR void       StartNewFrame ();
    IRAM_ATTR uint8_t    GetNextIntensityToSend ();
            void         SetInvertData (bool _InvertData) { InvertData = _InvertData; }
    virtual void         WriteChannelData (size_t StartChannelId, size_t ChannelCount, byte *pSourceData);
    virtual void         ReadChannelData (size_t StartChannelId, size_t ChannelCount, byte *pTargetData);
protected:

    void SetFramePrependInformation (const uint8_t* data, size_t len);
    void SetFrameAppendInformation  (const uint8_t* data, size_t len);
    void SetPixelPrependInformation (const uint8_t* data, size_t len);

    uint16_t  InterFrameGapInMicroSec = 300;

    void SetFrameDurration (float IntensityBitTimeInUs, uint16_t BlockSize = 1, float BlockDelayUs = 0.0);

private:
#define PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL 3

    size_t      NumIntensityBytesPerPixel = PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL;
    bool        _MoreDataToSend = false;

    uint8_t*    NextPixelToSend = nullptr;
    size_t      pixel_count = 100;
    size_t      SentPixelsCount = 0;
    size_t      PixelIntensityCurrentIndex = 0;

    uint8_t   * pFramePrependData = nullptr;
    size_t      FramePrependDataSize = 0;
    size_t      FramePrependDataCurrentIndex = 0;

    uint8_t   * pFrameAppendData = nullptr;
    size_t      FrameAppendDataSize = 0;
    size_t      FrameAppendDataCurrentIndex = 0;

    uint8_t   * PixelPrependData = nullptr;
    size_t      PixelPrependDataSize = 0;
    size_t      PixelPrependDataCurrentIndex = 0;

    size_t      PixelGroupSize = 1;
    size_t      PixelGroupSizeCurrentCount = 0;

    float       IntensityBitTimeInUs = 0.0;
    size_t      BlockSize = 1;
    float       BlockDelayUs = 0.0;

    size_t      zig_size = 0;
    size_t      ZigPixelCount = 1;
    size_t      ZigPixelCurrentCount = 1;
    size_t      ZagPixelCount = 1;
    size_t      ZagPixelCurrentCount = 1;

    size_t      PrependNullPixelCount = 0;
    size_t      PrependNullPixelCurrentCount = 0;

    size_t      AppendNullPixelCount = 0;
    size_t      AppendNullPixelCurrentCount = 0;

    bool        InvertData = false;

// #define USE_PIXEL_DEBUG_COUNTERS
#ifdef USE_PIXEL_DEBUG_COUNTERS
    size_t     PixelsToSend = 0;
    size_t     IntensityBytesSent = 0;
    size_t     IntensityBytesSentLastFrame = 0;
    size_t     SentPixels = 0;
    uint32_t   AbortFrameCounter = 0;
#endif // def USE_PIXEL_DEBUG_COUNTERS

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
    float       gamma = 1.0;                        ///< gamma value to use
    uint8_t     brightness = 100;
    uint32_t    AdjustedBrightness = 256;           ///< brightness to use

    // JSON configuration parameters
    String      color_order = "rgb"; ///< Pixel color order

    // Internal variables

    void updateGammaTable(); ///< Generate gamma correction table
    void updateColorOrderOffsets(); ///< Update color order
    bool validate ();        ///< confirm that the current configuration is valid
    inline size_t CalculateIntensityOffset(size_t ChannelId);

    enum FrameState_t
    {
        FramePrependData,
        FrameSendPixels,
        FrameAppendData,
        FrameDone
    };
    FrameState_t FrameState = FrameState_t::FrameDone;

    enum PixelSendState_t
    {
        PixelPrependNulls,
        PixelSendIntensity,
        PixelAppendNulls,
    };

    PixelSendState_t PixelSendState = PixelSendState_t::PixelSendIntensity;

}; // c_OutputPixel

