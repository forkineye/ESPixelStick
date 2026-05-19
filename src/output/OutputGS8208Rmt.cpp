/*
* OutputGS8208Rmt.cpp - GS8208 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2025 Shelby Merrick
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

#if defined(SUPPORT_OutputProtocol_GS8208) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputGS8208Rmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputGS8208Rmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputGS8208Rmt*>(arg)->StartNewDataFrame();
}

//----------------------------------------------------------------------------
c_OutputGS8208Rmt::c_OutputGS8208Rmt (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputGS8208 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("GS8208_PIXEL_RMT_TICKS_BIT_0_H: 0x") + String (GS8208_PIXEL_RMT_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String ("GS8208_PIXEL_RMT_TICKS_BIT_0_L: 0x") + String (GS8208_PIXEL_RMT_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("GS8208_PIXEL_RMT_TICKS_BIT_1_H: 0x") + String (GS8208_PIXEL_RMT_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("GS8208_PIXEL_RMT_TICKS_BIT_1_L: 0x") + String (GS8208_PIXEL_RMT_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputGS8208Rmt

//----------------------------------------------------------------------------
c_OutputGS8208Rmt::~c_OutputGS8208Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGS8208Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputGS8208Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputGS8208::Begin ();

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputGS8208Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGS8208::SetConfig (jsonConfig);

    // Default is 100us * 3
    Rmt.SetBitDuration(InterFrameGapInMicroSec * NanoSecondsInAMicroSecond, ifgBit, ifgBitCount);
    ifgBit.level0 = 0;
    ifgBit.level1 = 0;

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = uint32_t(OutputPortDefinition.PortId);
    OutputRmtConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.arg                     = this;
    OutputRmtConfig.ISR_GetNextIntensityBit = ISR_GetNextBitToSendBase;
    OutputRmtConfig.StartNewDataFrame       = StartNewDataFrameBase;

    Rmt.Begin(OutputRmtConfig, this);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGS8208Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputGS8208::SetOutputBufferSize (NumChannelsAvailable);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputGS8208Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputGS8208::GetStatus (jsonStatus);
    Rmt.GetStatus (jsonStatus);

        #ifdef GS8208_RMT_DEBUG_COUNTERS
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
    #endif // def GS8208_RMT_DEBUG_COUNTERS

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputGS8208Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputGS8208Rmt::RmtPoll ()
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
void c_OutputGS8208Rmt::StartNewDataFrame()
{
    // DEBUG_START;
    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_GS8208_RMT_DEBUG_COUNTERS(FrameStarts);
    ifgBitCurrentCount = ifgBitCount;
    StartNewFrame();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputGS8208Rmt::ISR_GetNextBitToSend (rmt_item32_t & DataToSend)
{
    INC_GS8208_RMT_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;
    if(ifgBitCurrentCount)
    {
        INC_GS8208_RMT_DEBUG_COUNTERS(StartBits);
        --ifgBitCurrentCount;
        DataToSend = ifgBit;
        // set up for the next data byte
        c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
        DataPatternMask = 0x80;
    }
    else if(DataPatternMask)
    {
        INC_GS8208_RMT_DEBUG_COUNTERS(DataBits);
        DataToSend = (DataPattern & DataPatternMask) ? OneBit : ZeroBit ;
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
                INC_GS8208_RMT_DEBUG_COUNTERS(FrameEnds);
                Response = false;
            }
        }
    }
    else
    {
        INC_GS8208_RMT_DEBUG_COUNTERS(Underrun);
        // nothing to send
        DataToSend.val = 0x0;
        Response = false;
    }

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputGS8208Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputGS8208::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputProtocol_GS8208) && defined(ARDUINO_ARCH_ESP32)
