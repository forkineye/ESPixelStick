/*
* OutputWS2811I2S.cpp - WS2811 driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2026 Shelby Merrick
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
#if defined (SUPPORT_OutputProtocol_WS2811) && defined (SUPPORT_I2S)

#include "output/OutputWS2811I2S.hpp"
#include "output/OutputMgr.hpp"

//----------------------------------------------------------------------------
static void IRAM_ATTR GetDataSlicesToSendBase (void * arg, c_OutputI2S::I2S_Item_t * DataToSend, uint32_t numSlices)
{
    reinterpret_cast<c_OutputWS2811I2S*> (arg)->ISR_GetNextDataSlicesToSend (DataToSend, numSlices);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
c_OutputWS2811I2S::c_OutputWS2811I2S (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputWS2811 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_0_H: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_0_L: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_1_H: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_1_L: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputWS2811I2S

//----------------------------------------------------------------------------
c_OutputWS2811I2S::~c_OutputWS2811I2S ()
{
    // DEBUG_START;

    I2Sdriver->RemoveSlotDevice(OutputPortDefinition.PortId);

    // DEBUG_END;
} // ~c_OutputWS2811I2S

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::Begin ()
{
    // DEBUG_START;

    I2Sdriver = OutputMgr.GetI2sDriver ();

    c_OutputWS2811::Begin ();

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    ZeroHighBitSliceCount = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_0_HIGH);
    ZeroLowBitSliceCount  = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_0_LOW);
    OneHighBitSliceCount  = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_1_HIGH);
    OneLowBitSliceCount   = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_1_LOW);

    // DEBUG_V (String ("ZeroHighBitSliceCount: ") + String (ZeroHighBitSliceCount));
    // DEBUG_V (String (" ZeroLowBitSliceCount: ") + String (ZeroLowBitSliceCount));
    // DEBUG_V (String (" OneHighBitSliceCount: ") + String (OneHighBitSliceCount));
    // DEBUG_V (String ("  OneLowBitSliceCount: ") + String (OneLowBitSliceCount));

    FrameResetSliceCount        = 0;
    FrameResetCurrentSliceCount = 0;
    HighBitCurrentSliceCount    = 0;
    LowBitCurrentSliceCount     = 0;
    IdleSliceCount              = 0;
    IdleCurrentSliceCount       = 0;

    DataBit = 0; // no data bit assigned yet
    DataBitMask = ~DataBit;

    c_OutputI2S::OutputI2SChannelConfig_t OutputI2SConfig;
    OutputI2SConfig.I2SChannelId              = uint32_t (OutputPortDefinition.PortId);
    OutputI2SConfig.DataPin                   = gpio_num_t (OutputPortDefinition.gpios.data);
    OutputI2SConfig.arg                       = this;
    OutputI2SConfig.GetNextIntensityBitSlices = GetDataSlicesToSendBase;
    OutputI2SConfig.IsActive                  = false; // do not output data

    I2Sdriver->RegisterSlotDevice (OutputI2SConfig, &DataBit, &DataBitMask);

    // DEBUG_V (String ("    DataBit: 0x") + String (DataBit, HEX));
    // DEBUG_V (String ("DataBitMask: 0x") + String (DataBitMask, HEX));

    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::CalculateFrameBitSlices ()
{
    DEBUG_START;

    uint32_t MinFrameLenNS = 25 * NanoSecondsInAMilliSecond;
    uint32_t FrameDurationInNanoSec = ActualFrameDurationMicroSec * NanoSecondsInAMicroSecond;
    // DEBUG_V (String (" ActualFrameDurationMicroSec: ") + String (ActualFrameDurationMicroSec));
    // DEBUG_V (String ("      FrameDurationInNanoSec: ") + String (FrameDurationInNanoSec));
    // DEBUG_V (String ("     InterFrameGapInMicroSec: ") + String (InterFrameGapInMicroSec));

    FrameDurationInNanoSec -= (InterFrameGapInMicroSec * NanoSecondsInAMicroSecond);
    // DEBUG_V (String ("  new FrameDurationInNanoSec: ") + String (FrameDurationInNanoSec));
    // DEBUG_V (String ("               MinFrameLenNS: ") + String (MinFrameLenNS));
    
    uint32_t IdleLenNS = (MinFrameLenNS > FrameDurationInNanoSec) ? MinFrameLenNS - FrameDurationInNanoSec : NanoSecondsInAMicroSecond; 
    // DEBUG_V (String ("                   IdleLenNS: ") + String (IdleLenNS));
    
    IdleSliceCount = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (IdleLenNS);
    // DEBUG_V (String ("              IdleSliceCount: ") + String (IdleSliceCount));

    FrameResetSliceCount = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (InterFrameGapInMicroSec * NanoSecondsInAMicroSecond);
    // DEBUG_V (String ("        FrameResetSliceCount: ") + String (FrameResetSliceCount));

    // DEBUG_END;

} // CalculateFrameBits

//----------------------------------------------------------------------------
bool c_OutputWS2811I2S::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    PauseOutput(true);

    bool response = c_OutputWS2811::SetConfig (jsonConfig);

    // update the output GPIO
    I2Sdriver->SetGpio (OutputPortDefinition.PortId, OutputPortDefinition.gpios.data);

    if(OutputBufferSize)
    {
        // DEBUG_V("start the transmiter");
        CalculateFrameBitSlices ();
        ISR_StartNewDataFrame ();
        PauseOutput (false);
    }
    else
    {
        // DEBUG_V("stop the transmiter");
        PauseOutput (true);
    }

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::SetOutputBufferSize (uint32_t NumChannelsToOutput)
{
    // DEBUG_START;

    // DEBUG_V (String ("NumChannelsToOutput: ") + String (NumChannelsToOutput));
    c_OutputWS2811::SetOutputBufferSize (NumChannelsToOutput);

    if(OutputBufferSize)
    {
        // DEBUG_V("start the transmiter");
        CalculateFrameBitSlices ();
        ISR_StartNewDataFrame ();
        PauseOutput (false);
    }
    else
    {
        // DEBUG_V("stop the transmiter");
        PauseOutput (true);
    }

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputWS2811::GetStatus (jsonStatus);

    #ifdef USE_I2S_DEBUG_COUNTERS
    jsonStatus[F ("FrameDurationInMicroSec")] = FrameDurationInMicroSec;
    #endif // def USE_I2S_DEBUG_COUNTERS

    #ifdef WS2811_I2S_DEBUG_COUNTERS
    JsonObject JsonCounters = jsonStatus["JsonCounters"].to<JsonObject> ();
    JsonWrite (JsonCounters, "GetDataSlices",                  I2SDebugCounters.GetDataSlices);
    JsonWrite (JsonCounters, "FrameStarts",                 I2SDebugCounters.FrameStarts);
    JsonWrite (JsonCounters, "FrameEnds",                   I2SDebugCounters.FrameEnds);
    JsonWrite (JsonCounters, "FrameResetBitSlices",         I2SDebugCounters.FrameResetBitSlices);
    JsonWrite (JsonCounters, "IdleBitSlices",               I2SDebugCounters.IdleBitSlices);
    JsonWrite (JsonCounters, "DataBitSlices",               I2SDebugCounters.DataBitSlices);
    JsonWrite (JsonCounters, "DataBits",                    I2SDebugCounters.DataBits);
    JsonWrite (JsonCounters, "DataBytes",                   I2SDebugCounters.DataBytes);
    JsonWrite (JsonCounters, "BitSliceHigh",                I2SDebugCounters.BitSliceHigh);
    JsonWrite (JsonCounters, "BitSliceLow",                 I2SDebugCounters.BitSliceLow);
    JsonWrite (JsonCounters, "DataBitEnd",                  I2SDebugCounters.DataBitEnd);
    JsonWrite (JsonCounters, "DataByteEnd",                 I2SDebugCounters.DataByteEnd);
    JsonWrite (JsonCounters, "IdleSliceCount",              IdleSliceCount);

    JsonWrite (JsonCounters, "DataBitMask",                 String(DataBitMask,HEX));
    JsonWrite (JsonCounters, "DataBit",                     String(DataBit,HEX));
    JsonWrite (JsonCounters, "ZeroHighBitSliceCount",       ZeroHighBitSliceCount);
    JsonWrite (JsonCounters, "ZeroLowBitSliceCount",        ZeroLowBitSliceCount);
    JsonWrite (JsonCounters, "OneHighBitSliceCount",        OneHighBitSliceCount);
    JsonWrite (JsonCounters, "OneLowBitSliceCount",         OneLowBitSliceCount);
    JsonWrite (JsonCounters, "HighBitCurrentSliceCount",    HighBitCurrentSliceCount);
    JsonWrite (JsonCounters, "LowBitCurrentSliceCount",     LowBitCurrentSliceCount);
    JsonWrite (JsonCounters, "FrameResetSliceCount",        FrameResetSliceCount);
    JsonWrite (JsonCounters, "FrameResetCurrentSliceCount", FrameResetCurrentSliceCount);
    JsonWrite (JsonCounters, "IdleSliceCount",              IdleSliceCount);
    JsonWrite (JsonCounters, "IdleCurrentSliceCount",       IdleCurrentSliceCount);
    JsonWrite (JsonCounters, "DataPattern",                 String(DataPattern, HEX));
    JsonWrite (JsonCounters, "DataPatternMask",             String(DataPatternMask, HEX));

    #endif // def WS2811_I2S_DEBUG_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811I2S::ISR_StartNewDataFrame ()
{
    // DEBUG_START;

    c_OutputWS2811::ISR_StartNewFrame ();

    // DEBUG_V (String ("frame started on ") + String (OutputPortDefinition.gpios.data));
    INC_WS2811_I2S_DEBUG_COUNTERS (FrameStarts);

    IdleCurrentSliceCount = IdleSliceCount;
    FrameResetCurrentSliceCount  = FrameResetSliceCount;

    // set up for the next data byte
    INC_WS2811_I2S_DEBUG_COUNTERS (DataBytes);

    // restore c_OutputPixel::ISR_GetNextIntensityToSend (DataPattern);
    DataPatternMask = 0x80;

    if (DataPattern & DataPatternMask)
    {
        // send a one bit
        HighBitCurrentSliceCount = OneHighBitSliceCount;
        LowBitCurrentSliceCount  = OneLowBitSliceCount;
    }
    else // send a zero bit
    {
        HighBitCurrentSliceCount = ZeroHighBitSliceCount;
        LowBitCurrentSliceCount  = ZeroLowBitSliceCount;
    }

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811I2S::ISR_GetNextDataSlicesToSend (c_OutputI2S::I2S_Item_t * pDataToSend, uint32_t numSlices)
{
    INC_WS2811_I2S_DEBUG_COUNTERS (GetDataSlices);

    for (int CurrentSliceId = 0; CurrentSliceId < numSlices; ++CurrentSliceId, ++pDataToSend)
    {
        if (IdleCurrentSliceCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS (IdleBitSlices);
            *pDataToSend |= uint8_t(DataBit); // output high
            --IdleCurrentSliceCount;
            continue;
        }

        if (FrameResetCurrentSliceCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS (FrameResetBitSlices);
            *pDataToSend &= uint8_t(DataBitMask); // output low
            --FrameResetCurrentSliceCount;
            continue;
        }

        INC_WS2811_I2S_DEBUG_COUNTERS (DataBitSlices);

        if (HighBitCurrentSliceCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS (BitSliceHigh);

            --HighBitCurrentSliceCount;
            *pDataToSend |= uint8_t(DataBit); // output high
            continue;
        }

        if (LowBitCurrentSliceCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS (BitSliceLow);
            *pDataToSend &= uint8_t(DataBitMask); // output low
            --LowBitCurrentSliceCount;
            if (LowBitCurrentSliceCount)
            {
                // more low bits to send
                continue;
            }
        }

        INC_WS2811_I2S_DEBUG_COUNTERS (DataBitEnd);

        // 1 bit of data has completed
        DataPatternMask = DataPatternMask >> 1;

        // do we need to set up the next data byte to send?
        if (0 == DataPatternMask)
        {
            // entire data byte has been sent
            INC_WS2811_I2S_DEBUG_COUNTERS (DataByteEnd);

            // is there another data byte to send?
            if (c_OutputPixel::ISR_MoreDataToSend ())
            {
                INC_WS2811_I2S_DEBUG_COUNTERS (FrameEnds);

                ISR_StartNewDataFrame ();

                // force a huge idle gap
                // IdleCurrentSliceCount = -1;
                continue;
            }

            // there is more data to send
            INC_WS2811_I2S_DEBUG_COUNTERS (DataBytes);

            // set up to output the next data byte
            c_OutputPixel::ISR_GetNextIntensityToSend (DataPattern);
            DataPatternMask = 0x80;
        }

        // more bits to send in the current data byte
        INC_WS2811_I2S_DEBUG_COUNTERS (DataBits);
        if (DataPattern & DataPatternMask)
        {
            // send a one bit
            HighBitCurrentSliceCount = OneHighBitSliceCount;
            LowBitCurrentSliceCount  = OneLowBitSliceCount;
        }
        else
        {
            // send a zero bit
            HighBitCurrentSliceCount = ZeroHighBitSliceCount;
            LowBitCurrentSliceCount  = ZeroLowBitSliceCount;
        }
    };

    return;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::PauseOutput (bool State)
{
    // DEBUG_START;

    // DEBUG_V (String ("PortId: ") + String (OutputPortDefinition.PortId));
    // DEBUG_V (String (" State: ") + String (State));

    c_OutputWS2811::PauseOutput (State);
    I2Sdriver->SetOutputState (OutputPortDefinition.PortId, !State);

    // DEBUG_END;
} // PauseOutput

#endif // defined (SUPPORT_OutputProtocol_WS2811) && defined (ARDUINO_ARCH_ESP32)
