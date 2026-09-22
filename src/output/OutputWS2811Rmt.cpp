/*
* OutputWS2811Rmt.cpp - WS2811 driver code for ESPixelStick RMT Channel
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
#if defined(SUPPORT_OutputProtocol_WS2811) && defined(SUPPORT_RMT)

#include "output/OutputWS2811Rmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputWS2811Rmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputWS2811Rmt*>(arg)->StartNewDataFrame();
} // StartNewDataFrameBase

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::c_OutputWS2811Rmt (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputWS2811 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_0_H: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_0_L: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_1_H: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_1_L: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_1_LOW,  HEX));

    ZeroBit = {WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH, 1, WS2811_PIXEL_RMT_TICKS_BIT_0_LOW, 0};
    OneBit  = {WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH, 1, WS2811_PIXEL_RMT_TICKS_BIT_1_LOW, 0};

    // DEBUG_END;

} // c_OutputWS2811Rmt

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::~c_OutputWS2811Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputWS2811Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputWS2811::Begin ();

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputWS2811Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputWS2811::SetConfig (jsonConfig);

    Rmt.SetBitDuration((InterFrameGapInMicroSec * NanoSecondsInAMicroSecond), IfgBit, IfgBitCount);
    IfgBit.level0 = 0;
    IfgBit.level1 = 0;

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = uint32_t(OutputPortDefinition.PortId);
    OutputRmtConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_HIGH;
    OutputRmtConfig.arg                     = this;
    OutputRmtConfig.ISR_GetNextIntensityBit = ISR_GetNextBitToSendBase;
    OutputRmtConfig.StartNewDataFrame       = StartNewDataFrameBase;
    OutputRmtConfig.BufferStart             = ISR_GetBufferAddress();
    OutputRmtConfig.NumBytesInFrame         = OM_MAX_NUM_CHANNELS;

    // DEBUG_V();
    Rmt.Begin(OutputRmtConfig, this);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputWS2811::SetOutputBufferSize (NumChannelsAvailable);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputWS2811::GetStatus (jsonStatus);
    Rmt.GetStatus (jsonStatus);
#ifdef USE_RMT_DEBUG_COUNTERS
    jsonStatus[F("FrameDurationInMicroSec")] = FrameDurationInMicroSec;
    jsonStatus[F("FrameStartTimeInMicroSec")] = GetFrameStartTimeInMicroSec();
    uint32_t now = micros();
    jsonStatus[F("Now")] = now;
    jsonStatus[F("FrameStartDelta")] = now - GetFrameStartTimeInMicroSec();
#endif // def USE_RMT_DEBUG_COUNTERS
    #ifdef WS2811_RMT_DEBUG_COUNTERS
    JsonObject JsonCounters = jsonStatus["JsonCounters"].to<JsonObject>();
    JsonWrite(JsonCounters, "GetNextBit",  RmtDebugCounters.GetNextBit);
    JsonWrite(JsonCounters, "FrameStarts", RmtDebugCounters.FrameStarts);
    JsonWrite(JsonCounters, "FrameEnds",   RmtDebugCounters.FrameEnds);
    JsonWrite(JsonCounters, "StartBits",   RmtDebugCounters.StartBits);
    JsonWrite(JsonCounters, "IfgBits",     RmtDebugCounters.IfgBits);
    JsonWrite(JsonCounters, "DataBits",    RmtDebugCounters.DataBits);
    JsonWrite(JsonCounters, "StopBits",    RmtDebugCounters.StopBits);
    JsonWrite(JsonCounters, "Underrun",    RmtDebugCounters.Underrun);
    JsonWrite(JsonCounters, "Can Refresh", CanRefresh);
    JsonWrite(JsonCounters, "Cannot Refresh", Cannot);
    #endif // def WS2811_RMT_DEBUG_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputWS2811Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputWS2811Rmt::RmtPoll ()
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
            INC_WS2811_RMT_DEBUG_COUNTERS(CannotRefresh);

            // DEBUG_V ("not ready to send yet");
            break;
        }
        INC_WS2811_RMT_DEBUG_COUNTERS(CanRefresh);

        // DEBUG_V(String("get the next frame started on ") + String(DataPin));
        Response = Rmt.StartNewFrame ();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::StartNewDataFrame()
{
    // DEBUG_START;

    ISR_StartNewFrame();

    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_WS2811_RMT_DEBUG_COUNTERS(FrameStarts);
    IfgBitCurrentCount = IfgBitCount;

    // set up for the next data byte
    INC_WS2811_RMT_DEBUG_COUNTERS(DataBytes);
    c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
    DataPatternMask = 0x80;

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputWS2811Rmt::ISR_GetNextBitToSend (rmt_item32_t & DataToSend)
{
    INC_WS2811_RMT_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;

    if (IfgBitCurrentCount)
    {
        INC_WS2811_RMT_DEBUG_COUNTERS(IfgBits);
        DataToSend = IfgBit;
        --IfgBitCurrentCount;
    }
    else if (DataPatternMask)
    {
        INC_WS2811_RMT_DEBUG_COUNTERS(DataBits);
        DataToSend = (DataPattern & DataPatternMask) ? OneBit : ZeroBit;

        DataPatternMask = DataPatternMask >> 1;
        if(0 == DataPatternMask)
        {
            if(c_OutputPixel::ISR_MoreDataToSend())
            {
                INC_WS2811_RMT_DEBUG_COUNTERS(DataBytes);
                c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
                DataPatternMask = 0x80;
            }
            else
            {
                INC_WS2811_RMT_DEBUG_COUNTERS(FrameEnds);
            }
        }
    }
    else
    {
        INC_WS2811_RMT_DEBUG_COUNTERS(Underrun);
        // nothing to send
        Response = false;
    }

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputWS2811::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputProtocol_WS2811) && defined(SUPPORT_RMT)
