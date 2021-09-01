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
    c_OutputCommon (OutputChannelId, outputGpio, uart, outputType),
    color_order ("rgb"),
    pixel_count (100),
    zig_size (0),
    group_size (1),
    gamma (1.0),
    brightness (100),
    pNextIntensityToSend (nullptr),
    RemainingPixelCount (0),
    numIntensityBytesPerPixel (3)
{
    // DEBUG_START;
    ColorOffsets.offset.r = 0;
    ColorOffsets.offset.g = 1;
    ColorOffsets.offset.b = 2;
    ColorOffsets.offset.w = 3;

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
    jsonConfig[CN_group_size] = group_size;
    jsonConfig[CN_zig_size] = zig_size;
    jsonConfig[CN_gamma] = gamma;
    jsonConfig[CN_brightness] = brightness; // save as a 0 - 100 percentage
    jsonConfig[CN_interframetime] = InterFrameGapInMicroSec;
    jsonConfig[CN_prependnullcount] = PrependNullCount;
    jsonConfig[CN_appendnullcount] = AppendNullCount;

    c_OutputCommon::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputPixel::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputCommon::GetStatus (jsonStatus);

    // jsonStatus["pNextIntensityToSend"] = uint32_t(pNextIntensityToSend);
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
void c_OutputPixel::SetPreambleInformation (uint8_t* PreambleStart, uint8_t NewPreambleSize)
{
    // DEBUG_START;

    pPreamble = PreambleStart;
    PreambleSize = NewPreambleSize;

    // DEBUG_END;

} // SetPreambleInformation

//----------------------------------------------------------------------------
bool c_OutputPixel::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    setFromJSON (color_order, jsonConfig, CN_color_order);
    setFromJSON (pixel_count, jsonConfig, CN_pixel_count);
    setFromJSON (group_size, jsonConfig, CN_group_size);
    setFromJSON (zig_size, jsonConfig, CN_zig_size);
    setFromJSON (gamma, jsonConfig, CN_gamma);
    setFromJSON (brightness, jsonConfig, CN_brightness);
    setFromJSON (InterFrameGapInMicroSec, jsonConfig, CN_interframetime);
    setFromJSON (PrependNullCount, jsonConfig, CN_prependnullcount);
    setFromJSON (AppendNullCount, jsonConfig, CN_appendnullcount);

    // DEBUG_V (String ("PrependNullCount: ") + String (PrependNullCount));
    // DEBUG_V (String (" AppendNullCount: ") + String (AppendNullCount));
 
    c_OutputCommon::SetConfig (jsonConfig);

    bool response = validate ();

    AdjustedBrightness = map (brightness, 0, 100, 0, 256);
    // DEBUG_V (String ("brightness: ") + String (brightness));
    // DEBUG_V (String ("AdjustedBrightness: ") + String (AdjustedBrightness));

    updateGammaTable ();
    updateColorOrderOffsets ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    ZigPixelCount = (2 > zig_size) ? pixel_count : zig_size;
    GroupPixelCount = (2 > group_size) ? 1 : group_size;

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

         if (String (F ("wrgb")) == color_order) { ColorOffsets.offset.r = 3; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 2; numIntensityBytesPerPixel = 4; }
    else if (String (F ("rgbw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("grbw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("brgw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("rbgw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("gbrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("bgrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("grb"))  == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("brg"))  == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("rbg"))  == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("gbr"))  == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("bgr"))  == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else
    {
        color_order = F ("rgb");
        ColorOffsets.offset.r = 0;
        ColorOffsets.offset.g = 1;
        ColorOffsets.offset.b = 2;
        ColorOffsets.offset.w = 3;
        numIntensityBytesPerPixel = 3;
    } // default

    // DEBUG_V (String ("numIntensityBytesPerPixel: ") + String (numIntensityBytesPerPixel));

    // DEBUG_END;
} // updateColorOrderOffsets

//----------------------------------------------------------------------------
/*
*   Validate that the current values meet our needs
*
*   needs
*       data set in the class elements
*   returns
*       true - no issues found
*       false - had an issue and had to fix things
*/
bool c_OutputPixel::validate ()
{
    // DEBUG_START;
    bool response = true;

    if (zig_size > pixel_count)
    {
        log (CN_stars + String (F (" Requested ZigZag size count was too high. Setting to ")) + pixel_count + " " + CN_stars);
        zig_size = pixel_count;
        response = false;
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

    float TotalIntensityBytes = OutputBufferSize * group_size;
    float TotalNullBytes = (PrependNullCount + AppendNullCount) * numIntensityBytesPerPixel;
    float TotalBytesOfIntensityData = (TotalIntensityBytes + TotalNullBytes + PreambleSize);
    float TotalBits = TotalBytesOfIntensityData * 8.0;
    uint16_t NumBlocks = uint16_t (TotalBytesOfIntensityData / float (BlockSize));
    int TotalBlockDelayUs = int (float (NumBlocks) * BlockDelayUs);

    FrameMinDurationInMicroSec = (IntensityBitTimeInUs * TotalBits) + InterFrameGapInMicroSec + TotalBlockDelayUs;

    // DEBUG_V (String ("          OutputBufferSize: ") + String (OutputBufferSize));
    // DEBUG_V (String ("                group_size: ") + String (group_size));
    // DEBUG_V (String ("       TotalIntensityBytes: ") + String (TotalIntensityBytes));
    // DEBUG_V (String ("          PrependNullCount: ") + String (PrependNullCount));
    // DEBUG_V (String ("           AppendNullCount: ") + String (AppendNullCount));
    // DEBUG_V (String (" numIntensityBytesPerPixel: ") + String (numIntensityBytesPerPixel));
    // DEBUG_V (String ("            TotalNullBytes: ") + String (TotalNullBytes));
    // DEBUG_V (String ("              PreambleSize: ") + String (PreambleSize));
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

    CurrentZigPixelCount    = ZigPixelCount - 1;
    CurrentZagPixelCount    = ZigPixelCount;
    CurrentGroupPixelCount  = GroupPixelCount;
    pNextIntensityToSend    = GetBufferAddress ();
    RemainingPixelCount     = pixel_count;
    CurrentIntensityIndex   = 0;
    CurrentPrependNullCount = PrependNullCount * numIntensityBytesPerPixel;
    CurrentAppendNullCount  = AppendNullCount  * numIntensityBytesPerPixel;
    PreambleCurrentCount    = 0;

    MoreDataToSend = (0 == pixel_count) ? false : true;

    // DEBUG_END;
} // StartNewFrame

//----------------------------------------------------------------------------
uint8_t IRAM_ATTR c_OutputPixel::GetNextIntensityToSend ()
{
    uint8_t response = (pNextIntensityToSend[ColorOffsets.Array[CurrentIntensityIndex]]);
    response = gamma_table[response];
    response = uint8_t( (uint32_t(response) * AdjustedBrightness) >> 8);

    do // once
    {
        if (PreambleSize != PreambleCurrentCount)
        {
            response = pPreamble[PreambleCurrentCount++];
            break;
        }

        // Are we prepending NULL data?
        if (CurrentPrependNullCount)
        {
            --CurrentPrependNullCount;
            response = 0x00;
            break;
        }

        // have we sent all of the frame data?
        if (0 == RemainingPixelCount)
        {
            response = 0x00;

            // Are we sending NULL data?
            if (CurrentAppendNullCount)
            {
                --CurrentAppendNullCount;
                if (0 == CurrentAppendNullCount)
                {
                    MoreDataToSend = false;
                }
            }
            break;
        }

        // has the current pixel completed?
        ++CurrentIntensityIndex;
        if (CurrentIntensityIndex < numIntensityBytesPerPixel)
        {
            // still working on the current pixel
            break;
        }
        CurrentIntensityIndex = 0;

        // has the group completed?
        --CurrentGroupPixelCount;
        if (0 != CurrentGroupPixelCount)
        {
            // not finished with the group yet
            continue;
        }

        // refresh the group count
        CurrentGroupPixelCount = GroupPixelCount;

        --RemainingPixelCount;
        if (0 == RemainingPixelCount)
        {
            // FrameDoneCounter++;
            // Do we need to append NULL data?
            if (0 == CurrentAppendNullCount)
            {
                MoreDataToSend = false;
            }

            break;
        }

        // have we completed the forward traverse
        if (CurrentZigPixelCount)
        {
            --CurrentZigPixelCount;
            // not finished with the set yet.
            pNextIntensityToSend += numIntensityBytesPerPixel;
            continue;
        }

        if (CurrentZagPixelCount == ZigPixelCount)
        {
            // first backward pixel
            pNextIntensityToSend += numIntensityBytesPerPixel * (ZigPixelCount + 1);
        }

        // have we completed the backward traverse
        if (CurrentZagPixelCount)
        {
            --CurrentZagPixelCount;
            // not finished with the set yet.
            pNextIntensityToSend -= numIntensityBytesPerPixel;
            continue;
        }

        // move to next forward pixel
        pNextIntensityToSend += numIntensityBytesPerPixel * (ZigPixelCount);

        // refresh the zigZag
        CurrentZigPixelCount = ZigPixelCount - 1;
        CurrentZagPixelCount = ZigPixelCount;

    } while (false);

    return response;
} // NextIntensityToSend
