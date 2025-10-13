/*
* OutputPixel.cpp - Pixel driver code for ESPixelStick UART
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
*/

#include "ESPixelStick.h"
#include "output/OutputPixel.hpp"
#include "output/OutputGECEFrame.hpp"

//----------------------------------------------------------------------------
c_OutputPixel::c_OutputPixel (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                              gpio_num_t outputGpio,
                              uart_port_t uart,
                              c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    updateGammaTable ();
    updateColorOrderOffsets ();

    FrameStateFuncPtr = &c_OutputPixel::FrameDone;

    strcpy(color_order, String(F("rgb")).c_str());

    // DEBUG_END;
} // c_OutputPixel

//----------------------------------------------------------------------------
c_OutputPixel::~c_OutputPixel ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputPixel

//----------------------------------------------------------------------------
void c_OutputPixel::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // DEBUG_V (String (                "gamma: ") + String (gamma));
    // DEBUG_V (String ("PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String (" AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String ("           PixelCount: ") + String (pixel_count));

    JsonWrite(jsonConfig, CN_color_order,      color_order);
    JsonWrite(jsonConfig, CN_pixel_count,      pixel_count);
    JsonWrite(jsonConfig, CN_group_size,       PixelGroupSize);
    JsonWrite(jsonConfig, CN_zig_size,         zig_size);
    JsonWrite(jsonConfig, CN_gamma,            serialized(String(gamma, 2)));
    JsonWrite(jsonConfig, CN_brightness,       brightness); // save as a 0 - 100 percentage
    JsonWrite(jsonConfig, CN_interframetime,   InterFrameGapInMicroSec);
    JsonWrite(jsonConfig, CN_prependnullcount, PrependNullPixelCount);
    JsonWrite(jsonConfig, CN_appendnullcount,  AppendNullPixelCount);

    c_OutputCommon::GetConfig (jsonConfig);

    // PrettyPrint(jsonConfig, "GetConfig");
    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputPixel::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;

    c_OutputCommon::BaseGetStatus (jsonStatus);

#ifdef USE_PIXEL_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["Pixel Debug"].to<JsonObject>();
    debugStatus["NumIntensityBytesPerPixel"]        = NumIntensityBytesPerPixel;
    debugStatus["PixelsToSend"]                     = PixelsToSend;
    debugStatus["FrameStartCounter"]                = FrameStartCounter;
    debugStatus["FrameEndCounter"]                  = FrameEndCounter;
    debugStatus["IntensityBytesSent"]               = IntensityBytesSent;
    debugStatus["IntensityBytesSentLastFrame"]      = IntensityBytesSentLastFrame;
    debugStatus["AbortFrameCounter"]                = AbortFrameCounter;
    debugStatus["GetNextIntensityToSendCounter"]    = GetNextIntensityToSendCounter;
    debugStatus["GetNextIntensityToSendFailedCounter"] = GetNextIntensityToSendFailedCounter;
    debugStatus["FramePrependDataCounter"]          = FramePrependDataCounter;
    debugStatus["FrameSendPixelsCounter"]           = FrameSendPixelsCounter;
    debugStatus["PixelPrependNullsCounter"]         = PixelPrependNullsCounter;
    debugStatus["PixelSendIntensityCounter"]        = PixelSendIntensityCounter;
    debugStatus["PixelAppendNullsCounter"]          = PixelAppendNullsCounter;
    debugStatus["PixelUnkownState"]                 = PixelUnkownState;
    debugStatus["FrameAppendDataCounter"]           = FrameAppendDataCounter;
    debugStatus["FrameDoneCounter"]                 = FrameDoneCounter;
    debugStatus["FrameStateUnknownCounter"]         = FrameStateUnknownCounter;
    debugStatus["LastGECEdataSent 0x"]              = String(LastGECEdataSent, HEX);
    debugStatus["NumGECEdataSent"]                  = NumGECEdataSent;
    debugStatus["GECEBrightness"]                   = GECEBrightness;
    debugStatus["AdjustedBrightness"]               = AdjustedBrightness;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputPixel::SetOutputBufferSize(uint32_t NumChannelsAvailable)
{
    // DEBUG_START;
    // DEBUG_V (String ("NumChannelsAvailable: ") + String (NumChannelsAvailable));
    // DEBUG_V (String ("   GetBufferUsedSize: ") + String (c_OutputCommon::GetBufferUsedSize ()));
    // DEBUG_V (String ("         pixel_count: ") + String (pixel_count));
    // DEBUG_V (String ("       BufferAddress: ") + String ((uint32_t)(c_OutputCommon::GetBufferAddress ())));

    do // once
    {
        // are we changing size?
        if (NumChannelsAvailable == OutputBufferSize)
        {
            // DEBUG_V ("NO Need to change the buffer");
            break;
        }

        // DEBUG_V ("Need to change the output buffers");

        // Stop current output operation
        c_OutputCommon::SetOutputBufferSize (NumChannelsAvailable);
        SetFrameDurration (IntensityBitTimeInUs, BlockSize, BlockDelayUs);

    } while (false);

    // DEBUG_END;
} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputPixel::SetFramePrependInformation (const uint8_t* data, uint32_t len)
{
    // DEBUG_START;

    pFramePrependData = (uint8_t*)data;
    FramePrependDataSize = len;

    // DEBUG_END;

} // SetFramePrependInformation

//----------------------------------------------------------------------------
void c_OutputPixel::SetFrameAppendInformation (const uint8_t* data, uint32_t len)
{
    // DEBUG_START;

    pFrameAppendData = (uint8_t*)data;
    FrameAppendDataSize = len;

    // DEBUG_END;

} // SetFrameAppendInformation

//----------------------------------------------------------------------------
void c_OutputPixel::SetPixelPrependInformation (const uint8_t* data, uint32_t len)
{
    // DEBUG_START;

    PixelPrependData = (uint8_t*)data;
    PixelPrependDataSize = len;

    // DEBUG_END;

} // SetPixelPrependInformation

//----------------------------------------------------------------------------
bool c_OutputPixel::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // PrettyPrint(jsonConfig, "SetConfig");

    // enums need to be converted to uints for json
    setFromJSON (color_order, jsonConfig, CN_color_order);
    setFromJSON (pixel_count, jsonConfig, CN_pixel_count);
    setFromJSON (PixelGroupSize, jsonConfig, CN_group_size);
    setFromJSON (zig_size, jsonConfig, CN_zig_size);
    setFromJSON (gamma, jsonConfig, CN_gamma);
    setFromJSON (brightness, jsonConfig, CN_brightness);
    setFromJSON (InterFrameGapInMicroSec, jsonConfig, CN_interframetime);
    setFromJSON (PrependNullPixelCount, jsonConfig, CN_prependnullcount);
    setFromJSON (AppendNullPixelCount, jsonConfig, CN_appendnullcount);

    c_OutputCommon::SetConfig (jsonConfig);

    // DEBUG_V (String (                "gamma: ") + String (gamma));
    // DEBUG_V (String ("PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String (" AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String ("           PixelCount: ") + String (pixel_count));

    bool response = validate ();

    AdjustedBrightness = map (brightness, 0, 100, 0, 256);
    GECEBrightness = GECE_SET_BRIGHTNESS(map(brightness, 0, 100, 0, 255));
    // DEBUG_V (String ("brightness: ") + String (brightness));
    // DEBUG_V (String ("AdjustedBrightness: ") + String (AdjustedBrightness));

    updateGammaTable ();
    updateColorOrderOffsets ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    PixelGroupSize = (2 > PixelGroupSize) ? 1 : PixelGroupSize;
    // DEBUG_V (String ("PixelGroupSize: ") + String (PixelGroupSize));

    SetFrameDurration(IntensityBitTimeInUs, BlockSize, BlockDelayUs);

    // DEBUG_V (String ("     zig_size: ") + String (zig_size));

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputPixel::updateGammaTable ()
{
    // DEBUG_START;
    double tempBrightness = double (brightness) / 100.0;
    // DEBUG_V (String ("tempBrightness: ") + String (tempBrightness));

    for (unsigned int i = 0; i < sizeof (gamma_table); ++i)
    {
        // ESP.wdtFeed ();
        gamma_table[i] = (uint8_t)min ((255.0 * pow (i * tempBrightness / 255, gamma) + 0.5), 255.0);
        // DEBUG_V (String ("i: ") + String (i));
        // DEBUG_V (String ("gamma_table[i]: ") + String (gamma_table[i]));
    }

    // DEBUG_END;
} // updateGammaTable

//----------------------------------------------------------------------------
void c_OutputPixel::updateColorOrderOffsets ()
{
    // DEBUG_START;
    // make sure the color order is all lower case
    String _color_order = String(color_order);
    _color_order.toLowerCase ();

    // DEBUG_V (String (" color_order: ") + color_order);
    // DEBUG_V (String ("_color_order: ") + _color_order);

         if (String (F ("rgb"))  == _color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("rbg"))  == _color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("gbr"))  == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("grb"))  == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("brg"))  == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("bgr"))  == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("wrgb")) == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 3; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("wrbg")) == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 3; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("wgbr")) == _color_order) { ColorOffsets.offset.r = 3; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("wgrb")) == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 3; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("wbrg")) == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 3; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("wbgr")) == _color_order) { ColorOffsets.offset.r = 3; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 0; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("rgbw")) == _color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("rbgw")) == _color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("gbrw")) == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("grbw")) == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("brgw")) == _color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("bgrw")) == _color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else
    {
        // DEBUG_V(String(F("Error: Unsupported Color Order: '")) + _color_order + F("'. Using RGB"));
        strcpy(color_order, String(F ("rgb")).c_str());
        ColorOffsets.offset.r = 0;
        ColorOffsets.offset.g = 1;
        ColorOffsets.offset.b = 2;
        ColorOffsets.offset.w = 3;
        NumIntensityBytesPerPixel = 3;
    } // default

    // DEBUG_V (String ("NumIntensityBytesPerPixel: ") + String (NumIntensityBytesPerPixel));

    // DEBUG_END;
} // updateColorOrderOffsets

//----------------------------------------------------------------------------
bool c_OutputPixel::validate ()
{
    // DEBUG_START;
    bool response = true;

    if (zig_size > pixel_count)
    {
        logcon (CN_stars + String (F (" Requested ZigZag size count was too high. Setting to ")) + pixel_count + " " + CN_stars);
        zig_size = pixel_count;
        response = false;
    }
    else if (2 > zig_size)
    {
        zig_size = 1;
    }

    // Default gamma value
    if (gamma <= 0)
    {
        gamma = 2.2;
        response = false;
    }

    // Max brightness value
    if (brightness > 100)
    {
        brightness = 100;
        // DEBUG_V (String ("brightness: ") + String (brightness));
        response = false;
    }

    // DEBUG_END;
    return response;

} // validate

//----------------------------------------------------------------------------
void c_OutputPixel::SetFrameDurration (float _IntensityBitTimeInUs, uint16_t BlockSize, float BlockDelayUs)
{
    // DEBUG_START;
    if (0 == BlockSize) { BlockSize = 1; }

    IntensityBitTimeInUs = _IntensityBitTimeInUs;

    float TotalIntensityBytes       = OutputBufferSize;
    float TotalNullBytes            = (PrependNullPixelCount + AppendNullPixelCount) * NumIntensityBytesPerPixel;
    float TotalBytesOfIntensityData = (TotalIntensityBytes + TotalNullBytes + FramePrependDataSize);
    float TotalBits                 = TotalBytesOfIntensityData * 8.0;
    uint16_t NumBlocks              = uint16_t (TotalBytesOfIntensityData / float (BlockSize));
    int TotalBlockDelayUs           = int (float (NumBlocks) * BlockDelayUs);

    ActualFrameDurationMicroSec = (IntensityBitTimeInUs * TotalBits) + InterFrameGapInMicroSec + TotalBlockDelayUs;
    FrameDurationInMicroSec = max(uint32_t(25000), ActualFrameDurationMicroSec);
    
    // DEBUG_V (String ("           OutputBufferSize: ") + String (OutputBufferSize));
    // DEBUG_V (String ("             PixelGroupSize: ") + String (PixelGroupSize));
    // DEBUG_V (String ("        TotalIntensityBytes: ") + String (TotalIntensityBytes));
    // DEBUG_V (String ("      PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String ("       AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String ("  NumIntensityBytesPerPixel: ") + String (NumIntensityBytesPerPixel));
    // DEBUG_V (String ("             TotalNullBytes: ") + String (TotalNullBytes));
    // DEBUG_V (String ("       FramePrependDataSize: ") + String (FramePrependDataSize));
    // DEBUG_V (String ("  TotalBytesOfIntensityData: ") + String (TotalBytesOfIntensityData));
    // DEBUG_V (String ("                  TotalBits: ") + String (TotalBits));
    // DEBUG_V (String ("                  BlockSize: ") + String (BlockSize));
    // DEBUG_V (String ("                  NumBlocks: ") + String (NumBlocks));
    // DEBUG_V (String ("               BlockDelayUs: ") + String (BlockDelayUs));
    // DEBUG_V (String ("          TotalBlockDelayUs: ") + String (TotalBlockDelayUs));
    // DEBUG_V (String ("       IntensityBitTimeInUs: ") + String (IntensityBitTimeInUs));
    // DEBUG_V (String ("    InterFrameGapInMicroSec: ") + String (InterFrameGapInMicroSec));
    // DEBUG_V (String ("ActualFrameDurationMicroSec: ") + String (ActualFrameDurationMicroSec));
    // DEBUG_V (String (" FrameDurationInMicroSec: ") + String (FrameDurationInMicroSec));

    // DEBUG_END;

} // SetInterframeGap

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputPixel::SetStartingSendPixelState()
{
    if(PixelPrependDataSize)
    {
        FrameStateFuncPtr = &c_OutputPixel::PixelSendPrependIntensity;
    }
    else
    {
#ifdef SUPPORT_OutputType_GECE
        if (OutputType == OTYPE_t::OutputType_GECE)
        {
            FrameStateFuncPtr = &c_OutputPixel::PixelSendGECEIntensity;
        }
        else
        {
            FrameStateFuncPtr = &c_OutputPixel::PixelSendIntensity;
        }
#else
        FrameStateFuncPtr = &c_OutputPixel::PixelSendIntensity;
#endif // def SUPPORT_OutputType_GECE
    }

} // SetStartingSendPixelState

//----------------------------------------------------------------------------
void c_OutputPixel::StartNewFrame ()
{
    // DEBUG_START;

#ifdef USE_PIXEL_DEBUG_COUNTERS
    if (ISR_MoreDataToSend ())
    {
        AbortFrameCounter++;
    }
    FrameStartCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    NextPixelToSend = GetBufferAddress();
    FramePrependDataCurrentIndex    = 0;
    FrameAppendDataCurrentIndex     = 0;
    SentPixelsCount                 = 0;
    PixelIntensityCurrentIndex      = 0;
    PixelIntensityCurrentColor      = 0;
    PrependNullPixelCurrentCount    = 0;
    AppendNullPixelCurrentCount     = 0;
    PixelPrependDataCurrentIndex    = 0;
    GECEPixelId                     = 0;

    if(FramePrependDataSize)
    {
        FrameStateFuncPtr = &c_OutputPixel::FramePrependData;
    }
    else if (PrependNullPixelCount)
    {
        FrameStateFuncPtr = &c_OutputPixel::PixelPrependNulls;
    }
    else
    {
        SetStartingSendPixelState ();
    }

#ifdef USE_PIXEL_DEBUG_COUNTERS
    SentPixels         = SentPixelsCount;
    PixelsToSend       = pixel_count;
    IntensityBytesSent = 0;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    // NumIntensityBytesPerPixel = 1;
    ReportNewFrame();

    // DEBUG_END;
} // StartNewFrame

//----------------------------------------------------------------------------
void c_OutputPixel::SetIntensityDataWidth(uint32_t DataWidth)
{
    uint32_t IntensityMaxValue = (1 << DataWidth);
    IntensityMultiplier = IntensityMaxValue / 256;

} // SetIntensityDataWidth

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::FramePrependData()
{
#ifdef USE_PIXEL_DEBUG_COUNTERS
    FramePrependDataCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    uint32_t response = pFramePrependData[FramePrependDataCurrentIndex];
    if (++FramePrependDataCurrentIndex >= FramePrependDataSize)
    {
        // FramePrependDataCurrentIndex = 0;
        if (PrependNullPixelCount)
        {
            FrameStateFuncPtr = &c_OutputPixel::PixelPrependNulls;
        }
        else
        {
            SetStartingSendPixelState();
        }
        // PixelIntensityCurrentIndex = 0;
        // PixelPrependDataCurrentIndex = 0;
        // PixelIntensityCurrentColor = 0;

    }
    return response;
}

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::PixelPrependNulls()
{
    uint32_t response = 0x00;
    do // once
    {
#ifdef USE_PIXEL_DEBUG_COUNTERS
        PixelPrependNullsCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

        if (PixelPrependDataCurrentIndex < PixelPrependDataSize)
        {
            response = PixelPrependData[PixelPrependDataCurrentIndex++] * IntensityMultiplier;
            break;
        }

        // has the pixel completed?
        if (++PixelIntensityCurrentIndex < NumIntensityBytesPerPixel)
        {
            break;
        }

        // pixel is complete. Move to the next one
        PixelIntensityCurrentIndex = 0;
        PixelPrependDataCurrentIndex = 0;
        PixelIntensityCurrentColor = 0;

        if (++PrependNullPixelCurrentCount < PrependNullPixelCount)
        {
            break;
        }

        // no more null pixels to send
        // PrependNullPixelCurrentCount = 0;
        SetStartingSendPixelState();

    } while (false);

    return response;
} // fPixelPrependNulls

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::PixelSendPrependIntensity()
{
#ifdef USE_PIXEL_DEBUG_COUNTERS
        PixelSendIntensityCounter++;
        IntensityBytesSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    uint32_t response = PixelPrependData[PixelPrependDataCurrentIndex++];

        // pixel prepend goes here
        if (PixelPrependDataCurrentIndex >= PixelPrependDataSize)
        {
            // reset the prepend index for the next pixel
            PixelPrependDataCurrentIndex = 0;

#ifdef SUPPORT_OutputType_GECE
            if (OutputType == OTYPE_t::OutputType_GECE)
            {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                LastGECEdataSent = response;
                NumGECEdataSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
                FrameStateFuncPtr = &c_OutputPixel::PixelSendGECEIntensity;
            }
            else
            {
                FrameStateFuncPtr = &c_OutputPixel::PixelSendIntensity;
            }
#else
            FrameStateFuncPtr = &c_OutputPixel::PixelSendIntensity;
#endif // def SUPPORT_OutputType_GECE
        }

    return response;
}

#ifdef SUPPORT_OutputType_GECE
//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::PixelSendGECEIntensity()
{
    uint32_t response = 0x00;

#ifdef USE_PIXEL_DEBUG_COUNTERS
        PixelSendIntensityCounter++;
        IntensityBytesSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    // build a GECE intensity frame
    response = GECEBrightness;
    response |= GECE_SET_ADDRESS(GECEPixelId++);
    response |= GECE_SET_RED(GetIntensityData());
    response |= GECE_SET_GREEN(GetIntensityData());
    response |= GECE_SET_BLUE(GetIntensityData());
#ifdef USE_PIXEL_DEBUG_COUNTERS
    LastGECEdataSent = response;
    NumGECEdataSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    return response;
}
#endif // def SUPPORT_OutputType_GECE

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::PixelSendIntensity()
{
#ifdef USE_PIXEL_DEBUG_COUNTERS
    PixelSendIntensityCounter++;
    IntensityBytesSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    return GetIntensityData();
} // fPixelSendIntensity

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::PixelAppendNulls()
{
    uint32_t response = 0x00;
    do // once
    {
#ifdef USE_PIXEL_DEBUG_COUNTERS
        PixelAppendNullsCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
// pixel prepend goes here
        if (PixelPrependDataCurrentIndex < PixelPrependDataSize)
        {
            response = PixelPrependData[PixelPrependDataCurrentIndex++];
            break;
        }

        // response = 0x00;

        // has the pixel completed?
        if (++PixelIntensityCurrentIndex < NumIntensityBytesPerPixel)
        {
            break;
        }

        // pixel is complete. Move to the next one
        PixelIntensityCurrentIndex = 0;
        PixelPrependDataCurrentIndex = 0;
        PixelIntensityCurrentColor = 0;

        if (++AppendNullPixelCurrentCount < AppendNullPixelCount)
        {
            break;
        }
        // AppendNullPixelCurrentCount = 0;

        FrameStateFuncPtr = (FrameAppendDataSize) ? &c_OutputPixel::FrameAppendData :&c_OutputPixel::FrameDone;

    } while (false);

    return response;

} // fPixelAppendNulls

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::FrameAppendData()
{
#ifdef USE_PIXEL_DEBUG_COUNTERS
    FrameAppendDataCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    uint32_t response = pFrameAppendData[FrameAppendDataCurrentIndex];

    if (++FrameAppendDataCurrentIndex >= FrameAppendDataSize)
    {
        // FrameAppendDataCurrentIndex = 0;
        FrameStateFuncPtr = &c_OutputPixel::FrameDone;
    }
    return response;
}

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::FrameDone()
{
#ifdef USE_PIXEL_DEBUG_COUNTERS
    FrameDoneCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
    return 0x00;
}

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputPixel::ISR_GetNextIntensityToSend (uint32_t &DataToSend)
{

#ifdef USE_PIXEL_DEBUG_COUNTERS
    GetNextIntensityToSendCounter++;
    if(!ISR_MoreDataToSend())
    {
        GetNextIntensityToSendFailedCounter++;
    }
#endif // def USE_PIXEL_DEBUG_COUNTERS

    DataToSend = (this->*FrameStateFuncPtr)();

    if (InvertData)
    {
        DataToSend = ~DataToSend;
    }

    return ISR_MoreDataToSend();

} // NextIntensityToSend

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::GetIntensityData()
{
    uint32_t response = 0;

    response = pOutputBuffer[PixelIntensityCurrentIndex];

    ++PixelIntensityCurrentIndex;
    if (PixelIntensityCurrentIndex >= OutputBufferSize)
    {
        // response = 0xaa;
        if (AppendNullPixelCount)
        {
            PixelPrependDataCurrentIndex = 0;
            PixelIntensityCurrentIndex = 0;
            AppendNullPixelCurrentCount = 0;
            PixelIntensityCurrentColor = 0;

            FrameStateFuncPtr = &c_OutputPixel::PixelAppendNulls;
        }
        else if (FrameAppendDataSize)
        {
            FrameStateFuncPtr = &c_OutputPixel::FrameAppendData;
        }
        else
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            IntensityBytesSentLastFrame = IntensityBytesSent;
#endif // def USE_PIXEL_DEBUG_COUNTERS

            FrameStateFuncPtr = &c_OutputPixel::FrameDone;
        }
    }
    // are we at the end of a pixel and are we prepending pixel data?
    else if(++PixelIntensityCurrentColor >= NumIntensityBytesPerPixel)
    {
        PixelIntensityCurrentColor = 0;
        SetStartingSendPixelState();
    }

    return response;
}

//----------------------------------------------------------------------------
inline uint32_t c_OutputPixel::CalculateIntensityOffset(uint32_t ChannelId)
{
    // DEBUG_START;

    // DEBUG_V(String("              ChannelId: ") + String(ChannelId));
    uint32_t PixelId = ChannelId / uint32_t(NumIntensityBytesPerPixel);
    // DEBUG_V(String("               PixelId0: ") + String(PixelId));

    // are we doing a zig zag operation?
    if ((zig_size > 1) && (PixelId >= zig_size))
    {
        // DEBUG_V(String("               PixelId1: ") + String(PixelId));
        // DEBUG_V(String("               zig_size: ") + String(zig_size));
        uint32_t ZigZagGroupId = PixelId / zig_size;
        // DEBUG_V(String("          ZigZagGroupId: ") + String(ZigZagGroupId));

        // is this a backwards group
        if (0 != (ZigZagGroupId & 0x1))
        {
            // DEBUG_V("Backwards Group");
            uint32_t zigoffset = PixelId % zig_size;
            // DEBUG_V(String("              zigoffset: ") + String(zigoffset));
            uint32_t BaseGroupPixelId = ZigZagGroupId * zig_size;
            // DEBUG_V(String("      BaseGroupPixelId1: ") + String(BaseGroupPixelId));
            PixelId = BaseGroupPixelId + (zig_size - 1) - zigoffset;
            // DEBUG_V(String("      BaseGroupPixelId2: ") + String(BaseGroupPixelId));
            // DEBUG_V(String("                PixelId: 0x") + String(PixelId, HEX));
        }
        // DEBUG_V(String("               PixelId2: ") + String(PixelId));
    }
    // DEBUG_V(String("          Final PixelId: ") + String(PixelId));

    uint32_t ColorOrderIndex = ChannelId % NumIntensityBytesPerPixel;
    if (uint32_t(NumIntensityBytesPerPixel) > ChannelId)
    {
        ColorOrderIndex = ChannelId;
    }
    uint32_t ColorOrderId = ColorOffsets.Array[ColorOrderIndex];
    uint32_t PixelIntensityBaseId = PixelId * PixelGroupSize * NumIntensityBytesPerPixel;
    uint32_t TargetBufferIntensityId = PixelIntensityBaseId + ColorOrderId;

    // DEBUG_V(String("                ChannelId: 0x") + String(ChannelId, HEX));
    // DEBUG_V(String("                  PixelId: 0x") + String(PixelId, HEX));
    // DEBUG_V(String("          ColorOrderIndex: 0x") + String(ColorOrderIndex, HEX));
    // DEBUG_V(String("             ColorOrderId: 0x") + String(ColorOrderId, HEX));
    // DEBUG_V(String("NumIntensityBytesPerPixel: 0x") + String(NumIntensityBytesPerPixel, HEX));
    // DEBUG_V(String("           PixelGroupSize: 0x") + String(PixelGroupSize, HEX));
    // DEBUG_V(String("     PixelIntensityBaseId: 0x") + String(PixelIntensityBaseId, HEX));
    // DEBUG_V(String("  TargetBufferIntensityId: 0x") + String(TargetBufferIntensityId, HEX));

    // DEBUG_END;
    return TargetBufferIntensityId;

} // CalculateIntensityOffset

//----------------------------------------------------------------------------
void c_OutputPixel::WriteChannelData(uint32_t StartChannelId, uint32_t ChannelCount, byte *pSourceData)
{
    // DEBUG_START;

    if((StartChannelId + ChannelCount) > OutputBufferSize)
    {
        DEBUG_V("ERROR: Writting beyond the end of the output buffer");
        DEBUG_V(String("StartChannelId: 0x") + String(StartChannelId, HEX));
        DEBUG_V(String("  ChannelCount: 0x") + String(ChannelCount));
    }
    // DEBUG_V(String("         StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("           ChannelCount: 0x") + String(ChannelCount, HEX));

    uint32_t EndChannelId = StartChannelId + ChannelCount;
    uint32_t SourceDataIndex = 0;
    for (uint32_t currentChannelId = StartChannelId; currentChannelId < EndChannelId; ++currentChannelId, ++SourceDataIndex)
    {
        uint32_t CurrentIntensityData = gamma_table[pSourceData[SourceDataIndex]];
        CurrentIntensityData = uint8_t((uint32_t(CurrentIntensityData) * AdjustedBrightness) >> 8);
        uint32_t CalculatedChannelId = CalculateIntensityOffset(currentChannelId);
        uint8_t *pBuffer = &pOutputBuffer[CalculatedChannelId];
        for(uint32_t CurrentGroupIndex = 0; CurrentGroupIndex < PixelGroupSize; ++CurrentGroupIndex)
        {
            // DEBUG_V(String("      CurrentGroupIndex: 0x") + String(CurrentGroupIndex, HEX));
            // DEBUG_V(String("    CalculatedChannelId: 0x") + String(CalculatedChannelId, HEX));
            if(uint32_t(pBuffer) >= uint32_t(&pOutputBuffer[OutputBufferSize]))
            {
                // DEBUG_V("This write is beyond the end of the Output buffer for this channel");
                // DEBUG_V(String("      CalculatedChannelId: ") + String(CalculatedChannelId));
                // DEBUG_V(String("NumIntensityBytesPerPixel: ") + String(NumIntensityBytesPerPixel));
                // DEBUG_V(String("           PixelGroupSize: ") + String(PixelGroupSize));
                // DEBUG_V(String("                Last Data: ") + String((CalculatedChannelId + (NumIntensityBytesPerPixel * PixelGroupSize))));
                // DEBUG_V(String("         OutputBufferSize: ") + String(OutputBufferSize));
                break;
            }

            if(uint32_t(pBuffer) >= uint32_t(&(OutputMgr.GetBufferAddress()[OutputMgr.GetBufferSize()])))
            {
                DEBUG_V("This write is beyond the end of the Global Output buffer");
                DEBUG_V(String("      CalculatedChannelId: ") + String(CalculatedChannelId));
                DEBUG_V(String("NumIntensityBytesPerPixel: ") + String(NumIntensityBytesPerPixel));
                DEBUG_V(String("           PixelGroupSize: ") + String(PixelGroupSize));
                DEBUG_V(String("                Last Data: ") + String((CalculatedChannelId + (NumIntensityBytesPerPixel * PixelGroupSize))));
                DEBUG_V(String("         OutputBufferSize: ") + String(OutputBufferSize));
                break;
            }

            *pBuffer = CurrentIntensityData;
            pBuffer += NumIntensityBytesPerPixel;
        }
    }

    // DEBUG_END;

} // WriteChannelData

//----------------------------------------------------------------------------
void c_OutputPixel::ReadChannelData(uint32_t StartChannelId, uint32_t ChannelCount, byte *pTargetData)
{
    // DEBUG_START;

    // DEBUG_V(String("         StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("           ChannelCount: 0x") + String(ChannelCount, HEX));

    uint32_t EndChannelId = StartChannelId + ChannelCount;
    uint32_t SourceDataIndex = 0;
    for (uint32_t currentChannelId = StartChannelId; currentChannelId < EndChannelId; ++currentChannelId, ++SourceDataIndex)
    {
        uint8_t CurrentIntensityData = pOutputBuffer[CalculateIntensityOffset(currentChannelId)];
        // CurrentIntensityData = gamma_table[CurrentIntensityData];
        CurrentIntensityData = uint8_t((uint32_t(CurrentIntensityData << 8) / AdjustedBrightness));
        pTargetData[SourceDataIndex] = CurrentIntensityData;
    }

    // DEBUG_END;

} // ReadChannelData
