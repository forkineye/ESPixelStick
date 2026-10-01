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

    OutputWS2811I2S_FSM_State = OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameStart;

    // DEBUG_END;

} // c_OutputWS2811I2S

//----------------------------------------------------------------------------
c_OutputWS2811I2S::~c_OutputWS2811I2S ()
{
    // DEBUG_START;
    if(HasBeenInitialized)
    {
        I2Sdriver->RemoveSlotDevice(OutputPortDefinition.PortId);
    }

    // DEBUG_END;
} // ~c_OutputWS2811I2S

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::Begin ()
{
    // DEBUG_START;

    I2Sdriver = OutputMgr.GetI2sDriver ();

    c_OutputWS2811::Begin ();

    // make sure we are in the paused state
    PauseOutput(true);

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    ZeroHighBitSliceCount = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_0_HIGH);
    ZeroLowBitSliceCount  = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_0_LOW);
    // OneHighBitSliceCount  = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_1_HIGH);
    // OneLowBitSliceCount   = I2Sdriver->GetNumTimeSlicesForTargetTimeNS (WS2811_PIXEL_NS_BIT_1_LOW);
    OneHighBitSliceCount  = ZeroLowBitSliceCount;
    OneLowBitSliceCount   = ZeroHighBitSliceCount;

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
    // DEBUG_START;

    uint32_t MinFrameLenNS = 25 * NanoSecondsInAMilliSecond;
    uint32_t FrameDurationInNS = ActualFrameDurationMicroSec * NanoSecondsInAMicroSecond;
    // DEBUG_V (String (" ActualFrameDurationMicroSec: ") + String (ActualFrameDurationMicroSec));
    // DEBUG_V (String ("           FrameDurationInNS: ") + String (FrameDurationInNS));
    // DEBUG_V (String ("     InterFrameGapInMicroSec: ") + String (InterFrameGapInMicroSec));

    uint32_t InterFrameGapInNS = InterFrameGapInMicroSec * NanoSecondsInAMicroSecond;
    // DEBUG_V (String ("           InterFrameGapInNS: ") + String (InterFrameGapInNS));

    FrameDurationInNS += InterFrameGapInNS;
    // DEBUG_V (String ("       new FrameDurationInNS: ") + String (FrameDurationInNS));
    // DEBUG_V (String ("               MinFrameLenNS: ") + String (MinFrameLenNS));
    
    uint32_t IdleLenNS = (MinFrameLenNS > FrameDurationInNS) ? MinFrameLenNS - FrameDurationInNS : 2 *NanoSecondsInAMicroSecond; 
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
        PauseOutput(true);
        CalculateFrameBitSlices ();
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
    JsonWrite (JsonCounters, "UnKnownFrameState",           I2SDebugCounters.UnKnownFrameState);

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
    INC_WS2811_I2S_DEBUG_COUNTER (FrameStarts);

    IdleCurrentSliceCount = IdleSliceCount;
    FrameResetCurrentSliceCount  = FrameResetSliceCount;

    // set up for the next data byte
    INC_WS2811_I2S_DEBUG_COUNTER (DataBytes);

    c_OutputPixel::ISR_GetNextIntensityToSend (DataPattern);
    DataPatternMask = 0x100; // one bit past the valid data.

    DataPattern = 0; // todo Remove This test code

    ISR_SetUpNextDataBitToSend ();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811I2S::ISR_GetNextDataSlicesToSend (c_OutputI2S::I2S_Item_t * pDataToSend, uint32_t numSlices)
{
    INC_WS2811_I2S_DEBUG_COUNTER (GetDataSlices);

    // Place to build the next data slices to send
    c_OutputI2S::I2S_Item_t CurrentData;

    while(numSlices > 0)
    {
        // byte order is 2 3 0 1
        (CurrentData) = *((c_OutputI2S::I2S_Item_t*)(((uint32_t)(pDataToSend)) ^ 0x2));

        switch (OutputWS2811I2S_FSM_State)
        {
            case OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameStart:
            {
                INC_WS2811_I2S_DEBUG_COUNTER (IdleBitSlices);

                // this must be first. The call to start frame could change the state.
                OutputWS2811I2S_FSM_State = c_OutputWS2811I2S::OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_Ifg;

                ISR_StartNewDataFrame ();

                // output a high bit
                CurrentData |= DataBit;

                break;
            }

            case OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_Ifg:
            {
                INC_WS2811_I2S_DEBUG_COUNTER (IdleBitSlices);

                if(--IdleCurrentSliceCount == 0)
                {
                    OutputWS2811I2S_FSM_State = c_OutputWS2811I2S::OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameReset;
                }
                
                // send a high bit
                CurrentData |= DataBit;

                break;
            }

            case OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameReset:
            {
                INC_WS2811_I2S_DEBUG_COUNTER (FrameResetBitSlices);

                if(--FrameResetCurrentSliceCount == 0)
                {
                    OutputWS2811I2S_FSM_State = c_OutputWS2811I2S::OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_DataHigh;
                }
                
                // send a low bit
                CurrentData &= DataBitMask;

                break;
            }

            case OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_DataHigh:
            {
                INC_WS2811_I2S_DEBUG_COUNTER (BitSliceHigh);
                if(--HighBitCurrentSliceCount == 0)
                {
                    OutputWS2811I2S_FSM_State = c_OutputWS2811I2S::OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_DataLow;
                }

                // send a high bit
                CurrentData |= DataBit;

                break;
            }

            case OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_DataLow:
            {
                INC_WS2811_I2S_DEBUG_COUNTER (BitSliceLow);
                if(--LowBitCurrentSliceCount == 0)
                {
                    OutputWS2811I2S_FSM_State = OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_DataHigh;
                    ISR_SetUpNextDataBitToSend();
                }

                CurrentData &= DataBitMask;

                break;
            }

            default:
            {
                CurrentData |= DataBit;
                OutputWS2811I2S_FSM_State = c_OutputWS2811I2S::OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameStart;
                INC_WS2811_I2S_DEBUG_COUNTER (UnKnownFrameState);

                break;
            }
        } // switch (CurrentFsmState)

        // byte order is 2 3 0 1
        *((c_OutputI2S::I2S_Item_t*)(((uint32_t)(pDataToSend)) ^ 0x2)) = (CurrentData);

        --numSlices;
        ++pDataToSend;
    } // while(numSlices > 0)

} // ISR_GetNextDataSlicesToSend

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811I2S::ISR_SetUpNextDataBitToSend()
{
    // 1 bit of data has completed move to the next bit of data
    DataPatternMask = DataPatternMask >> 1;

    do // once
    {
        // do we need to set up the next data byte to send?
        if (0 == DataPatternMask)
        {
            // entire data byte has been sent
            INC_WS2811_I2S_DEBUG_COUNTER (DataByteEnd);
    
            // is there another data byte to send?
            if (false == c_OutputPixel::ISR_MoreDataToSend ())
            {
                // Frame is complete, set up for the next frame
                INC_WS2811_I2S_DEBUG_COUNTER (FrameEnds);
    
                OutputWS2811I2S_FSM_State = OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameStart;
                break;
            }
    
            // there is more data to send in the current frame
            INC_WS2811_I2S_DEBUG_COUNTER (DataBytes);
    
            // set up to output the next data byte
            c_OutputPixel::ISR_GetNextIntensityToSend (DataPattern);
            DataPatternMask = 0x80;
        } // End of byte sent processing
    
        // more bits to send in the current data byte
        INC_WS2811_I2S_DEBUG_COUNTER (DataBits);
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
        } // End set up next bit to write to buffer

    } while (false); // do once
} // ISR_SetUpNextDataBitToSend

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::PauseOutput (bool State)
{
    // DEBUG_START;

    // DEBUG_V (String ("PortId: ") + String (OutputPortDefinition.PortId));
    // DEBUG_V (String (" New State: ") + String (State));
    // DEBUG_V (String (" Old State: ") + String (IsPaused()));
    if(IsPaused())
    {
        // DEBUG_V ("Currently Paused, Make sure we are ready to start at the beginning of the next frame");
        OutputWS2811I2S_FSM_State = OutputWS2811I2S_FSM_States::_OutputWS2811I2S_FSM_State_FrameStart;
    }
    c_OutputWS2811::PauseOutput (State);
    I2Sdriver->SetOutputState (OutputPortDefinition.PortId, !State);

    // DEBUG_END;
} // PauseOutput

#endif // defined (SUPPORT_OutputProtocol_WS2811) && defined (SUPPORT_I2S)
