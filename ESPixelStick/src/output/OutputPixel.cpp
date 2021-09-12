/*
* OutputPixel.cpp - Pixel driver code for ESPixelStick UART
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
*/

#include "../ESPixelStick.h"
#include "OutputPixel.hpp"

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
    c_OutputCommon::GetStatus (jsonStatus);

    // jsonStatus["NextPixelToSend"] = uint32_t (NextPixelToSend);
    // jsonStatus["RemainingIntensityCount"] = uint32_t(RemainingIntensityCount);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputPixel::SetOutputBufferSize (uint16_t NumChannelsAvailable)
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

    c_OutputCommon::SetConfig (jsonConfig);

    bool response = validate ();

    AdjustedBrightness = map (brightness, 0, 100, 0, 256);
    // DEBUG_V (String ("brightness: ") + String (brightness));
    // DEBUG_V (String ("AdjustedBrightness: ") + String (AdjustedBrightness));

    updateGammaTable ();
    updateColorOrderOffsets ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    ZigPixelCount = (2 > zig_size) ? pixel_count + 1 : zig_size + 1;
    ZagPixelCount = (2 > zig_size) ? pixel_count + 1 : zig_size + 0;
    PixelGroupSize = (2 > PixelGroupSize) ? 1 : PixelGroupSize;

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

    for (int i = 0; i < sizeof (gamma_table); ++i)
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

    float TotalIntensityBytes = OutputBufferSize * PixelGroupSize;
    float TotalNullBytes = (PrependNullPixelCount + AppendNullPixelCount) * NumIntensityBytesPerPixel;
    float TotalBytesOfIntensityData = (TotalIntensityBytes + TotalNullBytes + FramePrependDataSize);
    float TotalBits = TotalBytesOfIntensityData * 8.0;
    uint16_t NumBlocks = uint16_t (TotalBytesOfIntensityData / float (BlockSize));
    int TotalBlockDelayUs = int (float (NumBlocks) * BlockDelayUs);

    FrameMinDurationInMicroSec = (IntensityBitTimeInUs * TotalBits) + InterFrameGapInMicroSec + TotalBlockDelayUs;

    // DEBUG_V (String ("          OutputBufferSize: ") + String (OutputBufferSize));
    // DEBUG_V (String ("                PixelGroupSize: ") + String (PixelGroupSize));
    // DEBUG_V (String ("       TotalIntensityBytes: ") + String (TotalIntensityBytes));
    // DEBUG_V (String ("          PrependNullPixelCount: ") + String (PrependNullPixelCount));
    // DEBUG_V (String ("           AppendNullPixelCount: ") + String (AppendNullPixelCount));
    // DEBUG_V (String (" NumIntensityBytesPerPixel: ") + String (NumIntensityBytesPerPixel));
    // DEBUG_V (String ("            TotalNullBytes: ") + String (TotalNullBytes));
    // DEBUG_V (String ("              FramePrependDataSize: ") + String (FramePrependDataSize));
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

    NextPixelToSend = GetBufferAddress ();
    FramePrependDataCurrentIndex = 0;
    FrameAppendDataCurrentIndex = 0;
    ZigPixelCurrentCount = 0;
    ZagPixelCurrentCount = 0;
    SentPixelsCount = 0;
    PixelIntensityCurrentIndex = 0;
    PixelGroupSizeCurrentCount = 0;
    PrependNullPixelCurrentCount = 0;
    AppendNullPixelCurrentCount = 0;
    PixelPrependDataCurrentIndex = 0;

    FrameState = (FramePrependDataSize) ? FrameState_t::FramePrependData : FrameState_t::FrameSendPixels;
    PixelSendState = (PrependNullPixelCount) ? PixelSendState_t::PixelPrependNulls : PixelSendState_t::PixelSendIntensity;

    _MoreDataToSend = (0 == pixel_count) ? false : true;

    // NumIntensityBytesPerPixel = 1;

    // DEBUG_END;
} // StartNewFrame

//----------------------------------------------------------------------------
uint8_t IRAM_ATTR c_OutputPixel::GetNextIntensityToSend ()
{
    uint8_t response = 0x00;

    switch (FrameState)
    {
        case FrameState_t::FramePrependData:
        {
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
            switch (PixelSendState)
            {
                case PixelSendState_t::PixelPrependNulls:
                {
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
                    // pixel prepend goes here
                    if (PixelPrependDataCurrentIndex < PixelPrependDataSize)
                    {
                        response = PixelPrependData[PixelPrependDataCurrentIndex++];
                        break;
                    }

                    response = (NextPixelToSend[ColorOffsets.Array[PixelIntensityCurrentIndex]]);
                    response = gamma_table[response];
                    response = uint8_t ((uint32_t (response) * AdjustedBrightness) >> 8);

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
                            _MoreDataToSend = false;
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
                        NextPixelToSend += NumIntensityBytesPerPixel * (ZigPixelCount + 1);
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
                    NextPixelToSend += NumIntensityBytesPerPixel * (ZigPixelCount);

                    // refresh the zigZag
                    ZigPixelCurrentCount = 0;
                    ZagPixelCurrentCount = 0;

                    break;

                } // case PixelSendState_t::PixelSendIntensity:

                case PixelSendState_t::PixelAppendNulls:
                {
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

                    _MoreDataToSend = false;
                    FrameState = FrameState_t::FrameDone;

                    break;
                } // case PixelSendState_t::PixelAppendNulls:

            } // switch SendPixelsState
            break;
        } // case FrameState_t::FrameSendPixels

        case FrameState_t::FrameAppendData:
        {
            response = pFrameAppendData[FrameAppendDataCurrentIndex];
            if (++FrameAppendDataCurrentIndex < FrameAppendDataSize)
            {
                break;
            }
            // FrameAppendDataCurrentIndex = 0;
            FrameState = FrameState_t::FrameDone;
            _MoreDataToSend = false;
            break;
        } // case FrameState_t::FrameAppendData

        case FrameState_t::FrameDone:
        {
            _MoreDataToSend = false;
            break;
        } // case FrameState_t::FrameDone

    } // switch FrameState

    if (InvertData) { response = ~response; }

    return response;
} // NextIntensityToSend
