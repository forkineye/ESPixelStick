/*
* OutputTM1814Rmt.cpp - TM1814 driver code for ESPixelStick RMT Channel
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
#if defined (SUPPORT_OutputProtocol_TM1814) && defined (ARDUINO_ARCH_ESP32)

#include "output/OutputTM1814Rmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputTM1814Rmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputTM1814Rmt*>(arg)->StartNewDataFrame();
}

//----------------------------------------------------------------------------
c_OutputTM1814Rmt::c_OutputTM1814Rmt (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputTM1814 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("TM1814_PIXEL_RMT_TICKS_BIT_0_HIGH: 0x") + String (TM1814_PIXEL_RMT_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String (" TM1814_PIXEL_RMT_TICKS_BIT_0_LOW: 0x") + String (TM1814_PIXEL_RMT_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("TM1814_PIXEL_RMT_TICKS_BIT_1_HIGH: 0x") + String (TM1814_PIXEL_RMT_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String (" TM1814_PIXEL_RMT_TICKS_BIT_1_LOW: 0x") + String (TM1814_PIXEL_RMT_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputTM1814Rmt

//----------------------------------------------------------------------------
c_OutputTM1814Rmt::~c_OutputTM1814Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTM1814Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputTM1814Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputTM1814::Begin ();

    HasBeenInitialized = true;

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputTM1814Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputTM1814::SetConfig (jsonConfig);

    Rmt.SetBitDuration((InterFrameGapInMicroSec * NanoSecondsInAMicroSecond), IfgBit, IfgBitCount);
    IfgBit.level0 = 0;
    IfgBit.level1 = 0;

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = rmt_channel_t(OutputPortDefinition.PortId);
    OutputRmtConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_HIGH;
    OutputRmtConfig.arg                     = this;
    OutputRmtConfig.ISR_GetNextIntensityBit = ISR_GetNextBitToSendBase;
    OutputRmtConfig.StartNewDataFrame       = StartNewDataFrameBase;

    Rmt.Begin(OutputRmtConfig, this);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputTM1814::SetOutputBufferSize (NumChannelsAvailable);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputTM1814Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputTM1814::GetStatus (jsonStatus);

    // jsonStatus["DataISRcounter"] = DataISRcounter;
    // jsonStatus["FrameStartCounter"] = FrameStartCounter;
    // jsonStatus["FrameEndISRcounter"] = FrameEndISRcounter;

    #ifdef TM1814_RMT_DEBUG_COUNTERS
    JsonObject JsonCounters = jsonStatus["JsonCounters"].to<JsonObject>();
    JsonWrite(JsonCounters, "GetNextBit",  RmtDebugCounters.GetNextBit);
    JsonWrite(JsonCounters, "FrameStarts", RmtDebugCounters.FrameStarts);
    JsonWrite(JsonCounters, "FrameEnds",   RmtDebugCounters.FrameEnds);
    JsonWrite(JsonCounters, "BreakBits",   RmtDebugCounters.BreakBits);
    JsonWrite(JsonCounters, "MabBits",     RmtDebugCounters.MabBits);
    JsonWrite(JsonCounters, "StartBits",   RmtDebugCounters.StartBits);
    JsonWrite(JsonCounters, "DataBits",    RmtDebugCounters.DataBits);
    JsonWrite(JsonCounters, "StopBits",    RmtDebugCounters.StopBits);
    JsonWrite(JsonCounters, "Underrun",    RmtDebugCounters.Underrun);
    #endif // def TM1814_RMT_DEBUG_COUNTERS

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputTM1814Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputTM1814Rmt::RmtPoll ()
{
    // DEBUG_START;
    bool Response = false;
    do // Once
    {
        if (gpio_num_t(-1) == OutputPortDefinition.gpios.data)
        {
            break;
        }

        if(!canRefresh())
        {
            // DEBUG_V ("not ready to send yet");
            break;
        }

        // DEBUG_V("get the next frame started");
        Response = Rmt.StartNewFrame ();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputTM1814Rmt::StartNewDataFrame()
{
    // DEBUG_START;
    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_TM1814_RMT_DEBUG_COUNTERS(FrameStarts);
    IfgBitCurrentCount = IfgBitCount;
    StartNewFrame();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputTM1814Rmt::ISR_GetNextBitToSend (rmt_item32_t & DataToSend)
{
    INC_TM1814_RMT_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;
    if(IfgBitCurrentCount)
    {
        INC_TM1814_RMT_DEBUG_COUNTERS(StartBits);
        --IfgBitCurrentCount;
        DataToSend = IfgBit;
        // set up for the next data byte
        c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
        DataPatternMask = 0x80;
    }
    else if(DataPatternMask)
    {
        INC_TM1814_RMT_DEBUG_COUNTERS(DataBits);
        DataToSend = (DataPattern & DataPatternMask) ? OneBit : ZeroBit;
        DataPatternMask = DataPatternMask >> 1;
        if(0 == DataPatternMask)
        {
            if(c_OutputPixel::ISR_MoreDataToSend())
            {
                c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
                DataPatternMask = 0x80;
            }
            else
            {
                INC_TM1814_RMT_DEBUG_COUNTERS(FrameEnds);
                Response = false;
            }
        }
    }
    else
    {
        INC_TM1814_RMT_DEBUG_COUNTERS(Underrun);
        // nothing to send
        DataToSend.val = 0x0;
        Response = false;
    }

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputTM1814Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputTM1814::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined (SUPPORT_OutputProtocol_TM1814) && defined (ARDUINO_ARCH_ESP32)
