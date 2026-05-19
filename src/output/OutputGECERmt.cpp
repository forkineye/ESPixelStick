/*
* OutputGECERmt.cpp - GECE driver code for ESPixelStick RMT Channel
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
#if defined(SUPPORT_OutputProtocol_GECE) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputGECERmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputGECERmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputGECERmt*>(arg)->StartNewDataFrame();
}

//----------------------------------------------------------------------------
c_OutputGECERmt::c_OutputGECERmt(OM_OutputPortDefinition_t & OutputPortDefinition,
                                 c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputGECE(OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("GECE_PIXEL_RMT_TICKS_BIT_0_LOW: 0x") + String (GECE_PIXEL_RMT_TICKS_BIT_0_LOW, HEX));
    // DEBUG_V (String ("GECE_PIXEL_RMT_TICKS_BIT_0_HIGH: 0x") + String (GECE_PIXEL_RMT_TICKS_BIT_0_HIGH,  HEX));
    // DEBUG_V (String ("GECE_PIXEL_RMT_TICKS_BIT_1_H: 0x") + String (GECE_PIXEL_RMT_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("GECE_PIXEL_RMT_TICKS_BIT_1_L: 0x") + String (GECE_PIXEL_RMT_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputGECERmt

//----------------------------------------------------------------------------
c_OutputGECERmt::~c_OutputGECERmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGECERmt

//----------------------------------------------------------------------------
void c_OutputGECERmt::Begin ()
{
    // DEBUG_START;

    c_OutputGECE::Begin ();

    HasBeenInitialized = true;

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputGECERmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGECE::SetConfig (jsonConfig);

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
void c_OutputGECERmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputGECE::SetOutputBufferSize (NumChannelsAvailable);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputGECERmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputGECE::GetStatus (jsonStatus);
    Rmt.GetStatus (jsonStatus);

    #ifdef GECE_RMT_DEBUG_COUNTERS
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
    #endif // def GECE_RMT_DEBUG_COUNTERS

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputGECERmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputGECERmt::RmtPoll ()
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
void c_OutputGECERmt::StartNewDataFrame()
{
    // DEBUG_START;
    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_GECE_RMT_DEBUG_COUNTERS(FrameStarts);
    StartBitCount = 1;
    StartNewFrame();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputGECERmt::ISR_GetNextBitToSend (rmt_item32_t & DataToSend)
{
    INC_GECE_RMT_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;
    if(StartBitCount)
    {
        INC_GECE_RMT_DEBUG_COUNTERS(StartBits);
        StartBitCount = 0;
        DataToSend = StartBit;
        // set up for the next data byte
        c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
        StopBitCount = 1;
        CurrentDataMask = 0x02000000;
    }
    else if(CurrentDataMask)
    {
        INC_GECE_RMT_DEBUG_COUNTERS(DataBits);
        DataToSend = (DataPattern & CurrentDataMask) ? OneBit : ZeroBit ;
        CurrentDataMask = CurrentDataMask >> 1;
    }
    else if(StopBitCount)
    {
        INC_GECE_RMT_DEBUG_COUNTERS(StopBits);
        StopBitCount = 0;
        DataToSend = StopBit;

        if(c_OutputPixel::ISR_MoreDataToSend())
        {
            StartBitCount = 1;
        }
        else
        {
            INC_GECE_RMT_DEBUG_COUNTERS(FrameEnds);
            Response = false;
        }
    }
    else
    {
        INC_GECE_RMT_DEBUG_COUNTERS(Underrun);
        // nothing to send
        DataToSend.val = 0x0;
        Response = false;
    }

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputGECERmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputGECE::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputProtocol_GECE) && defined(ARDUINO_ARCH_ESP32)
