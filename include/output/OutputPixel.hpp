#pragma once
/*
* OutputPixel.h - Pixel driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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

class c_OutputPixel : public c_OutputCommon
{
protected:

    void SetFramePrependInformation (const uint8_t* data, uint32_t len);
    void SetFrameAppendInformation  (const uint8_t* data, uint32_t len);
    void SetPixelPrependInformation (const uint8_t* data, uint32_t len);

    uint16_t  InterFrameGapInMicroSec = 300;

    void SetFrameDurration (float IntensityBitTimeInUs, uint16_t BlockSize = 1, float BlockDelayUs = 0.0);

private:
#define PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL 3

    uint32_t      NumIntensityBytesPerPixel = PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL;

    uint8_t     * NextPixelToSend             = nullptr;
    uint32_t      pixel_count                 = 100;
    uint32_t      SentPixelsCount             = 0;
    uint32_t      PixelIntensityCurrentIndex  = 0;
    uint32_t      PixelIntensityCurrentColor  = 0;

    uint8_t     * pFramePrependData           = nullptr;
    uint32_t      FramePrependDataSize        = 0;
    uint32_t      FramePrependDataCurrentIndex = 0;

    uint8_t     * pFrameAppendData            = nullptr;
    uint32_t      FrameAppendDataSize         = 0;
    uint32_t      FrameAppendDataCurrentIndex = 0;

    uint8_t     * PixelPrependData            = nullptr;
    uint32_t      PixelPrependDataSize        = 0;
    uint32_t      PixelPrependDataCurrentIndex = 0;

    uint32_t      PixelGroupSize              = 1;
    uint32_t      PixelGroups                 = 1;

    float       IntensityBitTimeInUs        = 0.0;
    uint32_t    BlockSize                   = 1;
    float       BlockDelayUs                = 0.0;

    uint32_t    zig_size                    = 1;

    uint32_t    PrependNullPixelCount       = 0;
    uint32_t    PrependNullPixelCurrentCount = 0;

    uint32_t    AppendNullPixelCount        = 0;
    uint32_t    AppendNullPixelCurrentCount = 0;

    bool        InvertData                  = false;
    uint32_t    IntensityMultiplier         = 1;

// #define USE_PIXEL_DEBUG_COUNTERS
#ifdef USE_PIXEL_DEBUG_COUNTERS
    uint32_t   PixelsToSend                        = 0;
    uint32_t   IntensityBytesSent                  = 0;
    uint32_t   IntensityBytesSentLastFrame         = 0;
    uint32_t   FrameStartCounter                   = 0;
    uint32_t   FrameEndCounter                     = 0;
    uint32_t   SentPixels                          = 0;
    uint32_t   AbortFrameCounter                   = 0;
    uint32_t   FramePrependDataCounter             = 0;
    uint32_t   FrameSendPixelsCounter              = 0;
    uint32_t   FrameAppendDataCounter              = 0;
    uint32_t   FrameDoneCounter                    = 0;
    uint32_t   FrameStateUnknownCounter            = 0;
    uint32_t   PixelPrependNullsCounter            = 0;
    uint32_t   PixelSendIntensityCounter           = 0;
    uint32_t   PixelAppendNullsCounter             = 0;
    uint32_t   PixelUnkownState                    = 0;
    uint32_t   GetNextIntensityToSendCounter       = 0;
    uint32_t   GetNextIntensityToSendFailedCounter = 0;
    uint32_t   LastGECEdataSent                    = uint32_t(-1);
    uint32_t   NumGECEdataSent                     = 0;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    // functions used to implement pixel FSM
    uint32_t IRAM_ATTR FramePrependData();
    uint32_t IRAM_ATTR PixelPrependNulls();
    uint32_t IRAM_ATTR PixelSendPrependIntensity();
#ifdef SUPPORT_OutputType_GECE
    uint32_t IRAM_ATTR PixelSendGECEIntensity();
#endif // def SUPPORT_OutputType_GECE
    uint32_t IRAM_ATTR PixelSendIntensity();
    uint32_t IRAM_ATTR PixelAppendNulls();
    uint32_t IRAM_ATTR FrameAppendData();
    uint32_t IRAM_ATTR FrameDone();

    void IRAM_ATTR SetStartingSendPixelState();
    uint32_t (c_OutputPixel::* FrameStateFuncPtr) ();

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

    uint8_t     gamma_table[256]    = { 0 };    ///< Gamma Adjustment table
    float       gamma               = 1.0;      ///< gamma value to use
    uint8_t     brightness          = 100;
    uint32_t    AdjustedBrightness  = 256;
    uint32_t    GECEPixelId         = 0;
    uint32_t    GECEBrightness      = 255;

    // JSON configuration parameters
    String      color_order = "rgb"; ///< Pixel color order

    // Internal variables

    void updateGammaTable(); ///< Generate gamma correction table
    void updateColorOrderOffsets(); ///< Update color order
    bool validate ();        ///< confirm that the current configuration is valid
    inline uint32_t CalculateIntensityOffset(uint32_t ChannelId);
    uint32_t IRAM_ATTR GetIntensityData();

public:
    c_OutputPixel (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                   gpio_num_t outputGpio,
                   uart_port_t uart,
                   c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputPixel ();

    // functions to be provided by the derived class
    virtual  bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual  void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    virtual  void         GetStatus (ArduinoJson::JsonObject& jsonStatus);
             uint32_t     GetNumOutputBufferBytesNeeded () { return (pixel_count * NumIntensityBytesPerPixel); };
             uint32_t     GetNumOutputBufferChannelsServiced () { return (GetNumOutputBufferBytesNeeded() / PixelGroupSize); };
    virtual  void         SetOutputBufferSize (uint32_t NumChannelsAvailable);
             void         SetInvertData (bool _InvertData) { InvertData = _InvertData; }
    virtual  void         WriteChannelData (uint32_t StartChannelId, uint32_t ChannelCount, byte *pSourceData);
    virtual  void         ReadChannelData (uint32_t StartChannelId, uint32_t ChannelCount, byte *pTargetData);
    inline   void         SetIntensityBitTimeInUS (float value) { IntensityBitTimeInUs = value; }
             void         SetIntensityDataWidth(uint32_t value);
             void         StartNewFrame();
    inline   bool IRAM_ATTR ISR_MoreDataToSend () {return (&c_OutputPixel::FrameDone != FrameStateFuncPtr);}
             bool IRAM_ATTR ISR_GetNextIntensityToSend (uint32_t &DataToSend);
    void                  SetPixelCount(uint32_t value) {pixel_count = value;}
    uint32_t              GetPixelCount() {return pixel_count;}

}; // c_OutputPixel
