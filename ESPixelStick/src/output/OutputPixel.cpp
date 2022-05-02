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

#include "../ESPixelStick.h"
#include "OutputPixel.hpp"
#include "OutputGECEFrame.hpp"

#define ADJUST_INTENSITY_AT_ISR

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

    jsonConfig[CN_color_order] = color_order;
    jsonConfig[CN_pixel_count] = pixel_count;
    jsonConfig[CN_group_size] = PixelGroupSize;
    jsonConfig[CN_zig_size] = zig_size;
    jsonConfig[CN_gamma] = gamma;
    jsonConfig[CN_brightness] = brightness; // save as a 0 - 100 percentage
    jsonConfig[CN_interframetime] = InterFrameGapInMicroSec;
    jsonConfig[CN_prependnullcount] = PrependNullPixelCount;
    jsonConfig[CN_appendnullcount] = AppendNullPixelCount;

    c_OutputCommon::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputPixel::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // DEBUG_START;

    c_OutputCommon::GetStatus (jsonStatus);

#ifdef USE_PIXEL_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus.createNestedObject("Pixel Debug");
    debugStatus["NumIntensityBytesPerPixel"]        = NumIntensityBytesPerPixel;
    debugStatus["PixelsToSend"]                     = PixelsToSend;
    debugStatus["FrameStartCounter"]                = FrameStartCounter;
    debugStatus["FrameEndCounter"]                  = FrameEndCounter;
    debugStatus["IntensityBytesSent"]               = IntensityBytesSent;
    debugStatus["IntensityBytesSentLastFrame"]      = IntensityBytesSentLastFrame;
    debugStatus["AbortFrameCounter"]                = AbortFrameCounter;
    debugStatus["GetNextIntensityToSendCounter"]    = GetNextIntensityToSendCounter;
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

    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputPixel::SetOutputBufferSize(size_t NumChannelsAvailable)
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
void c_OutputPixel::SetFramePrependInformation (const uint8_t* data, size_t len)
{
    // DEBUG_START;

    pFramePrependData = (uint8_t*)data;
    FramePrependDataSize = len;

    // DEBUG_END;

} // SetFramePrependInformation

//----------------------------------------------------------------------------
void c_OutputPixel::SetFrameAppendInformation (const uint8_t* data, size_t len)
{
    // DEBUG_START;

    pFrameAppendData = (uint8_t*)data;
    FrameAppendDataSize = len;

    // DEBUG_END;

} // SetFrameAppendInformation

//----------------------------------------------------------------------------
void c_OutputPixel::SetPixelPrependInformation (const uint8_t* data, size_t len)
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

    // DEBUG_V (String ("PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String (" AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String ("           PixelCount: ") + String (pixel_count));

    c_OutputCommon::SetConfig (jsonConfig);

    bool response = validate ();

    AdjustedBrightness = map (brightness, 0, 100, 0, 256);
    GECEBrightness = GECE_SET_BRIGHTNESS(map(brightness, 0, 100, 0, 255));
    // DEBUG_V (String ("brightness: ") + String (brightness));
    // DEBUG_V (String ("AdjustedBrightness: ") + String (AdjustedBrightness));

    updateGammaTable ();
    updateColorOrderOffsets ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    ZigPixelCount  = (2 > zig_size) ? pixel_count + 1 : zig_size + 1;
    ZagPixelCount  = (2 > zig_size) ? pixel_count + 1 : zig_size + 1;
    PixelGroupSize = (2 > PixelGroupSize) ? 1 : PixelGroupSize;

    SetFrameDurration(IntensityBitTimeInUs, BlockSize, BlockDelayUs);

    // DEBUG_V (String ("ZigPixelCount: ") + String (ZigPixelCount));
    // DEBUG_V (String ("ZagPixelCount: ") + String (ZagPixelCount));
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
    color_order.toLowerCase ();

    // DEBUG_V (String ("color_order: ") + color_order);

    if (String (F ("wrgb")) == color_order) { ColorOffsets.offset.r = 3; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 2; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("rgbw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("grbw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("brgw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("rbgw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("gbrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("bgrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 4; }
    else if (String (F ("grb")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("brg")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("rbg")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("gbr")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else if (String (F ("bgr")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; NumIntensityBytesPerPixel = 3; }
    else
    {
        color_order = F ("rgb");
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
void c_OutputPixel::SetFrameDurration (float IntensityBitTimeInUs, uint16_t BlockSize, float BlockDelayUs)
{
    // DEBUG_START;
    if (0 == BlockSize) { BlockSize = 1; }

    float TotalIntensityBytes       = OutputBufferSize * PixelGroupSize;
    float TotalNullBytes            = (PrependNullPixelCount + AppendNullPixelCount) * NumIntensityBytesPerPixel;
    float TotalBytesOfIntensityData = (TotalIntensityBytes + TotalNullBytes + FramePrependDataSize);
    float TotalBits                 = TotalBytesOfIntensityData * 8.0;
    uint16_t NumBlocks              = uint16_t (TotalBytesOfIntensityData / float (BlockSize));
    int TotalBlockDelayUs           = int (float (NumBlocks) * BlockDelayUs);

    FrameMinDurationInMicroSec = (IntensityBitTimeInUs * TotalBits) + InterFrameGapInMicroSec + TotalBlockDelayUs;
    FrameMinDurationInMicroSec = max(uint32_t(25000), FrameMinDurationInMicroSec);

    // DEBUG_V (String ("          OutputBufferSize: ") + String (OutputBufferSize));
    // DEBUG_V (String ("            PixelGroupSize: ") + String (PixelGroupSize));
    // DEBUG_V (String ("       TotalIntensityBytes: ") + String (TotalIntensityBytes));
    // DEBUG_V (String ("     PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String ("      AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String (" NumIntensityBytesPerPixel: ") + String (NumIntensityBytesPerPixel));
    // DEBUG_V (String ("            TotalNullBytes: ") + String (TotalNullBytes));
    // DEBUG_V (String ("      FramePrependDataSize: ") + String (FramePrependDataSize));
    // DEBUG_V (String (" TotalBytesOfIntensityData: ") + String (TotalBytesOfIntensityData));
    // DEBUG_V (String ("                 TotalBits: ") + String (TotalBits));
    // DEBUG_V (String ("                 BlockSize: ") + String (BlockSize));
    // DEBUG_V (String ("                 NumBlocks: ") + String (NumBlocks));
    // DEBUG_V (String ("              BlockDelayUs: ") + String (BlockDelayUs));
    // DEBUG_V (String ("         TotalBlockDelayUs: ") + String (TotalBlockDelayUs));
    // DEBUG_V (String ("      IntensityBitTimeInUs: ") + String (IntensityBitTimeInUs));
    // DEBUG_V (String ("   InterFrameGapInMicroSec: ") + String (InterFrameGapInMicroSec));
    // DEBUG_V (String ("FrameMinDurationInMicroSec: ") + String (FrameMinDurationInMicroSec));

    // DEBUG_END;

} // SetInterframeGap

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputPixel::StartNewFrame ()
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
    ZigPixelCurrentCount            = 1;
    ZagPixelCurrentCount            = 0;
    SentPixelsCount                 = 0;
    PixelIntensityCurrentIndex      = 0;
    PixelGroupSizeCurrentCount      = 0;
    PrependNullPixelCurrentCount    = 0;
    AppendNullPixelCurrentCount     = 0;
    PixelPrependDataCurrentIndex    = 0;
    GECEPixelId                     = 0;
    
    FrameState     = (FramePrependDataSize)  ? FrameState_t::FramePrependData : FrameState_t::FrameSendPixels;
    PixelSendState = (PrependNullPixelCount) ? PixelSendState_t::PixelPrependNulls : PixelSendState_t::PixelSendIntensity;

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
uint32_t IRAM_ATTR c_OutputPixel::ISR_GetNextIntensityToSend ()
{
    uint32_t response = 0x00;

#ifdef USE_PIXEL_DEBUG_COUNTERS
    GetNextIntensityToSendCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

    switch (FrameState)
    {
        case FrameState_t::FramePrependData:
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FramePrependDataCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

            response = pFramePrependData[FramePrependDataCurrentIndex];
            if (++FramePrependDataCurrentIndex < FramePrependDataSize)
            {
                break;
            }

            // FramePrependDataCurrentIndex = 0;
            FrameState = FrameState_t::FrameSendPixels;
            // PixelIntensityCurrentIndex = 0;
            // PixelPrependDataCurrentIndex = 0;
            break;
        } // case FrameState_t::FramePrependData

        case FrameState_t::FrameSendPixels:
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FrameSendPixelsCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

            switch (PixelSendState)
            {
                default:
                {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                    PixelUnkownState++;
                    break;
#endif // def USE_PIXEL_DEBUG_COUNTERS
                }
                
                case PixelSendState_t::PixelPrependNulls:
                {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                    PixelPrependNullsCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
                    if (PixelPrependDataCurrentIndex < PixelPrependDataSize)
                    {
                        response = PixelPrependData[PixelPrependDataCurrentIndex++] * IntensityMultiplier;
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

                    if (++PrependNullPixelCurrentCount < PrependNullPixelCount)
                    {
                        break;
                    }

                    // no more null pixels to send
                    // PrependNullPixelCurrentCount = 0;
                    PixelSendState = PixelSendState_t::PixelSendIntensity;
                    break;
                } // case PixelSendState_t::PixelPrependNulls:

                case PixelSendState_t::PixelSendIntensity:
                {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                    PixelSendIntensityCounter++;
                    IntensityBytesSent++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

                    // pixel prepend goes here
                    if (PixelPrependDataCurrentIndex < PixelPrependDataSize)
                    {
                        response = PixelPrependData[PixelPrependDataCurrentIndex++];
                        break;
                    }
#ifdef SUPPORT_OutputType_GECE
                    if (OutputType == OTYPE_t::OutputType_GECE)
                    {
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
                    }
                    else
#endif // def SUPPORT_OutputType_GECE
                    {
                        response = GetIntensityData();
                    }
                    break;
                } // case PixelSendState_t::PixelSendIntensity:

                case PixelSendState_t::PixelAppendNulls:
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

                    if (++AppendNullPixelCurrentCount < AppendNullPixelCount)
                    {
                        break;
                    }
                    // AppendNullPixelCurrentCount = 0;

                    if (FrameAppendDataSize)
                    {
                        // FrameAppendDataCurrentCount = 0;
                        FrameState = FrameState_t::FrameAppendData;
                        break;
                    }

                    FrameState = FrameState_t::FrameDone;

                    break;
                } // case PixelSendState_t::PixelAppendNulls:

            } // switch SendPixelsState
            break;
        } // case FrameState_t::FrameSendPixels

        case FrameState_t::FrameAppendData:
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FrameAppendDataCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
            response = pFrameAppendData[FrameAppendDataCurrentIndex];
            if (++FrameAppendDataCurrentIndex < FrameAppendDataSize)
            {
                break;
            }
            // FrameAppendDataCurrentIndex = 0;
            FrameState = FrameState_t::FrameDone;
            break;
        } // case FrameState_t::FrameAppendData

        case FrameState_t::FrameDone:
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FrameDoneCounter++;
            response = 0x55;
#endif // def USE_PIXEL_DEBUG_COUNTERS
            break;
        } // case FrameState_t::FrameDone

        default:
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FrameStateUnknownCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS
        }
        } // switch FrameState

        if (InvertData)
        {
            response = ~response;
        }

        return response;

} // NextIntensityToSend

//----------------------------------------------------------------------------
uint32_t IRAM_ATTR c_OutputPixel::GetIntensityData()
{
    uint32_t response = 0;

    do // once
    {
#ifdef ADJUST_INTENSITY_AT_ISR
        response = (NextPixelToSend[ColorOffsets.Array[PixelIntensityCurrentIndex]]);
        response = gamma_table[response];
        response = uint8_t((uint32_t(response) * AdjustedBrightness) >> 8);

        // has the pixel completed?
        ++PixelIntensityCurrentIndex;
        if (PixelIntensityCurrentIndex < NumIntensityBytesPerPixel)
        {
            // response = 0xF0;
            break;
        }
        // response = 0xFF;

        PixelIntensityCurrentIndex = 0;
        PixelPrependDataCurrentIndex = 0;

        // has the group completed?
        if (++PixelGroupSizeCurrentCount < PixelGroupSize)
        {
            // not finished with the group yet
            break;
        }

        // refresh the group count
        PixelGroupSizeCurrentCount = 0;

        ++SentPixelsCount;
        if (SentPixelsCount >= pixel_count)
        {
#ifdef USE_PIXEL_DEBUG_COUNTERS
            FrameEndCounter++;
#endif // def USE_PIXEL_DEBUG_COUNTERS

            // response = 0xaa;
            if (AppendNullPixelCount)
            {
                PixelPrependDataCurrentIndex = 0;
                PixelIntensityCurrentIndex = 0;
                AppendNullPixelCurrentCount = 0;

                PixelSendState = PixelSendState_t::PixelAppendNulls;
            }
            else if (FrameAppendDataSize)
            {
                // FrameAppendDataCurrentIndex = 0;
                FrameState = FrameState_t::FrameAppendData;
            }
            else
            {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                IntensityBytesSentLastFrame = IntensityBytesSent;
#endif // def USE_PIXEL_DEBUG_COUNTERS
                FrameState = FrameState_t::FrameDone;
            }

            break;
        }

        // have we completed the forward traverse
        if (++ZigPixelCurrentCount < ZigPixelCount)
        {
            // response = 0x0F;
            // not finished with the set yet.
            NextPixelToSend += NumIntensityBytesPerPixel;
            break;
        }

        if (0 == ZagPixelCurrentCount)
        {
            // first backward pixel
            NextPixelToSend += NumIntensityBytesPerPixel * (ZagPixelCount);
        }

        // have we completed the backward traverse
        if (++ZagPixelCurrentCount < ZagPixelCount)
        {
            // response = 0xF0;
            // not finished with the set yet.
            NextPixelToSend -= NumIntensityBytesPerPixel;
            break;
        }

        // response = 0xFF;

        // move to next forward pixel
        NextPixelToSend += NumIntensityBytesPerPixel * (ZagPixelCount - 1);

        // refresh the zigZag
        ZigPixelCurrentCount = 1;
        ZagPixelCurrentCount = 0;

        break;

#else // !ADJUST_INTENSITY_AT_ISR Adjustments are made at write time
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

                PixelSendState = PixelSendState_t::PixelAppendNulls;
            }
            else if (FrameAppendDataSize)
            {
                // FrameAppendDataCurrentIndex = 0;
                FrameState = FrameState_t::FrameAppendData;
            }
            else
            {
#ifdef USE_PIXEL_DEBUG_COUNTERS
                IntensityBytesSentLastFrame = IntensityBytesSent;
#endif // def USE_PIXEL_DEBUG_COUNTERS

                FrameState = FrameState_t::FrameDone;
            }

            break;
        }
#endif // ! def ADJUST_INTENSITY_AT_WRITE

    } while (false);
    return response;
}

//----------------------------------------------------------------------------
inline size_t c_OutputPixel::CalculateIntensityOffset(size_t ChannelId)
{
    // DEBUG_START;

    // DEBUG_V(String("              ChannelId: 0x") + String(ChannelId, HEX));
    size_t PixelId = ChannelId / size_t(NumIntensityBytesPerPixel);
    // DEBUG_V(String("                PixelId: 0x") + String(PixelId, HEX));

    // are we doing a zig zag operation?
    if ((zig_size > 1) && (PixelId >= zig_size))
    {
        // DEBUG_V(String("               zig_size: 0x") + String(zig_size, HEX));
        size_t ZigZagGroupId = PixelId / zig_size;
        // DEBUG_V(String("          ZigZagGroupId: 0x") + String(ZigZagGroupId, HEX));

        // is this a backwards group
        if (0 != (ZigZagGroupId & 0x1))
        {
            // DEBUG_V("Backwards Group");
            size_t zigoffset = PixelId % zig_size;
            // DEBUG_V(String("              zigoffset: 0x") + String(zigoffset, HEX));
            size_t BaseGroupPixelId = ZigZagGroupId * zig_size;
            PixelId = BaseGroupPixelId + (zig_size - 1) - zigoffset;
            // DEBUG_V(String("       BaseGroupPixelId: 0x") + String(BaseGroupPixelId, HEX));
            // DEBUG_V(String("                PixelId: 0x") + String(PixelId, HEX));
        }
    }

    size_t ColorOrderIndex = ChannelId % size_t(NumIntensityBytesPerPixel);
    if (size_t(NumIntensityBytesPerPixel) > ChannelId)
    {
        ColorOrderIndex = ChannelId;
    }
    size_t ColorOrderId = ColorOffsets.Array[ColorOrderIndex];
    size_t PixelIntensityBaseId = PixelId * size_t(NumIntensityBytesPerPixel);
    size_t TargetBufferIntensityId = PixelIntensityBaseId + ColorOrderId;

    // DEBUG_V(String("              ChannelId: 0x") + String(ChannelId, HEX));
    // DEBUG_V(String("                PixelId: 0x") + String(PixelId, HEX));
    // DEBUG_V(String("        ColorOrderIndex: 0x") + String(ColorOrderIndex, HEX));
    // DEBUG_V(String("           ColorOrderId: 0x") + String(ColorOrderId, HEX));
    // DEBUG_V(String("   PixelIntensityBaseId: 0x") + String(PixelIntensityBaseId, HEX));
    // DEBUG_V(String("TargetBufferIntensityId: 0x") + String(TargetBufferIntensityId, HEX));

    // DEBUG_END;
    return TargetBufferIntensityId;

} // CalculateIntensityOffset

//----------------------------------------------------------------------------
void c_OutputPixel::WriteChannelData(size_t StartChannelId, size_t ChannelCount, byte *pSourceData)
{
    // DEBUG_START;

    // DEBUG_V(String("         StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("           ChannelCount: 0x") + String(ChannelCount, HEX));

#ifdef ADJUST_INTENSITY_AT_ISR
    memcpy(&pOutputBuffer[StartChannelId], pSourceData, ChannelCount);
#else

    size_t EndChannelId = StartChannelId + ChannelCount;
    size_t SourceDataIndex = 0;
    for (size_t currentChannelId = StartChannelId; currentChannelId < EndChannelId; ++currentChannelId, ++SourceDataIndex)
    {
        size_t CurrentIntensityData = gamma_table[pSourceData[SourceDataIndex]];
        CurrentIntensityData = uint8_t((uint32_t(CurrentIntensityData) * AdjustedBrightness) >> 8);

        pOutputBuffer[CalculateIntensityOffset(currentChannelId)] = CurrentIntensityData;
    }

#endif // def ADJUST_INTENSITY_AT_ISR

    // DEBUG_END;

} // WriteChannelData

//----------------------------------------------------------------------------
void c_OutputPixel::ReadChannelData(size_t StartChannelId, size_t ChannelCount, byte *pTargetData)
{
    // DEBUG_START;

    // DEBUG_V(String("         StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("           ChannelCount: 0x") + String(ChannelCount, HEX));
#ifdef ADJUST_INTENSITY_AT_ISR
    memcpy(pTargetData, &pOutputBuffer[StartChannelId], ChannelCount);
#else  // !ADJUST_INTENSITY_AT_ISR

    size_t EndChannelId = StartChannelId + ChannelCount;
    size_t SourceDataIndex = 0;
    for (size_t currentChannelId = StartChannelId; currentChannelId < EndChannelId; ++currentChannelId, ++SourceDataIndex)
    {
        uint8_t CurrentIntensityData = pOutputBuffer[CalculateIntensityOffset(currentChannelId)];
        // CurrentIntensityData = gamma_table[CurrentIntensityData];
        CurrentIntensityData = uint8_t((uint32_t(CurrentIntensityData << 8) / AdjustedBrightness));
        pTargetData[SourceDataIndex] = CurrentIntensityData;
    }

#endif // !def ADJUST_INTENSITY_AT_ISR

    // DEBUG_END;

} // ReadChannelData
