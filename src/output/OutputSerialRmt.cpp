/*
* OutputSerialRmt.cpp - Serial protocol driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 - 2026 Shelby Merrick
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
#if (defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputSerialRmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputSerialRmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputSerialRmt*>(arg)->StartNewDataFrame();
}

//----------------------------------------------------------------------------
c_OutputSerialRmt::c_OutputSerialRmt (OM_OutputPortDefinition_t & OutputPortDefinition,
    c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputSerial (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_END;

} // c_OutputSerialRmt

//----------------------------------------------------------------------------
c_OutputSerialRmt::~c_OutputSerialRmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputSerialRmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputSerialRmt::Begin ()
{
    // DEBUG_START;

    c_OutputSerial::Begin ();

    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputSerialRmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputSerial::SetConfig (jsonConfig);
    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = uint32_t(OutputPortDefinition.DeviceId);
    OutputRmtConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level              = idle_level;
    OutputRmtConfig.arg                     = this;
    OutputRmtConfig.ISR_GetNextIntensityBit = ISR_GetNextBitToSendBase;
    OutputRmtConfig.StartNewDataFrame       = StartNewDataFrameBase;

    Rmt.Begin(OutputRmtConfig, this);
    SetUpRmtBitTimes();

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSerialRmt::SetUpRmtBitTimes()
{
    // DEBUG_START;

    float BitTimeNS = (1.0 / float(CurrentBaudrate)) * NanoSecondsInASecond;
    // DEBUG_V(String(" CurrentBaudrate: ") + String(CurrentBaudrate));
    // DEBUG_V(String("       BitTimeNS: ") + String(BitTimeNS));

    // DEBUG_V(String("BitTimeNS: ") + String(BitTimeNS));
    // DEBUG_V(String("RMT_TickLengthNS: ") + String(RMT_TickLengthNS));

    uint32_t BitCount = 0;

    // one start bit
    Rmt.SetBitDuration(BitTimeNS, StartBit, BitCount);
    StartBit.level0 = 0;
    StartBit.level1 = 0;
    // DEBUG_V(String("StartBit: ") + String(StartBit.duration0 * 2) + " ticks");

    // 00
    Rmt.SetBitDuration(BitTimeNS * 2, DataBitArray[0], BitCount);
    DataBitArray[0].level0 = 0;
    DataBitArray[0].level1 = 0;
    // DEBUG_V(String("DataBitArray[0]: ") + String(DataBitArray[0].duration0 * 2) + " ticks");

    // 01
    Rmt.SetBitDuration(BitTimeNS * 2, DataBitArray[1], BitCount);
    DataBitArray[1].level0 = 1;
    DataBitArray[1].level1 = 0;
    // DEBUG_V(String("DataBitArray[1]: ") + String(DataBitArray[1].duration0 * 2) + " ticks");

    // 10
    Rmt.SetBitDuration(BitTimeNS * 2, DataBitArray[2], BitCount);
    DataBitArray[2].level0 = 0;
    DataBitArray[2].level1 = 1;
    // DEBUG_V(String("DataBitArray[2]: ") + String(DataBitArray[2].duration0 * 2) + " ticks");

    // 11
    Rmt.SetBitDuration(BitTimeNS * 2, DataBitArray[3], BitCount);
    DataBitArray[3].level0 = 1;
    DataBitArray[3].level1 = 1;
    // DEBUG_V(String("DataBitArray[3]: ") + String(DataBitArray[3].duration0 * 2) + " ticks");

    // two stop bits
    Rmt.SetBitDuration(BitTimeNS * 2, StopBit, BitCount);
    StopBit.level0 = 1;
    StopBit.level1 = 1;
    // DEBUG_V(String("StopBit: ") + String(StopBit.duration0 * 2) + " ticks");

    // Break bits
    Rmt.SetBitDuration((DMX_BREAK_US * NanoSecondsInAMicroSecond), BreakBit, BitCount);
    BreakBit.level0 = 0;
    BreakBit.level1 = 0;
    // DEBUG_V(String("BreakBit: ") + String(BreakBit.duration0 * 2) + " ticks");

    // Break bits
    Rmt.SetBitDuration((DMX_MAB_US * NanoSecondsInAMicroSecond), MabBit, BitCount);
    MabBit.level0 = 1;
    MabBit.level1 = 1;
    // DEBUG_V(String("MabBit: ") + String(MabBit.duration0 * 2) + " ticks");

    // DEBUG_END;
} // SetUpRmtBitTimes

//----------------------------------------------------------------------------
void c_OutputSerialRmt::SetOutputBufferSize(uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputSerial::SetOutputBufferSize (NumChannelsAvailable);
    SetUpRmtBitTimes();

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputSerialRmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // DEBUG_START;

    c_OutputSerial::GetStatus(jsonStatus);
    Rmt.GetStatus(jsonStatus);

    #ifdef SERIAL_RMT_DEBUG_COUNTERS
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
    #endif // def SERIAL_RMT_DEBUG_COUNTERS

    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputSerialRmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputSerialRmt::RmtPoll ()
{
    // DEBUG_START;
    bool Response = false;
    do // Once
    {
        if (gpio_num_t(-1) == OutputPortDefinition.gpios.data)
        {
            break;
        }

        // What for the next possible frame time
        if(!canRefresh()) {break;}

        // DEBUG_V("get the next frame started");
        Response = Rmt.StartNewFrame ();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputSerialRmt::StartNewDataFrame()
{
    // DEBUG_START;
    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_SERIAL_RMT_DEBUG_COUNTERS(FrameStarts);
    #if defined(SUPPORT_OutputProtocol_DMX)
    if(OutputType == c_OutputMgr::e_OutputProtocolType::OutputProtocol_DMX)
    {
        BreakBitCount = 1;
    }
    #endif //  defined(SUPPORT_OutputProtocol_DMX)
    StartBitCount = 1;
    StartNewFrame();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputSerialRmt::ISR_GetNextBitToSend (rmt_item32_t & DataToSend)
{
    INC_SERIAL_RMT_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;
    if(BreakBitCount)
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(BreakBits);
        BreakBitCount = 0;
        DataToSend = BreakBit;
        MabBitCount = 1;
    }
    else if(MabBitCount)
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(MabBits);
        MabBitCount = 0;
        DataToSend = MabBit;
        StartBitCount = 1;
    }
    else if(StartBitCount)
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(StartBits);
        StartBitCount = 0;
        DataToSend = StartBit;
        // set up for the next data byte
        c_OutputSerial::ISR_GetNextIntensityToSend(DataPattern);
        CurrentDataPairId = 4;
        StopBitCount = 1;
    }
    else if(CurrentDataPairId)
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(DataBits);
        --CurrentDataPairId;
        DataToSend = DataBitArray[DataPattern & 0x3];
        DataPattern = DataPattern >> 2;
    }
    else if(StopBitCount)
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(StopBits);
        StopBitCount = 0;
        DataToSend = StopBit;

        if(c_OutputSerial::ISR_MoreDataToSend())
        {
            StartBitCount = 1;
        }
        else
        {
            INC_SERIAL_RMT_DEBUG_COUNTERS(FrameEnds);
            Response = false;
        }
    }
    else
    {
        INC_SERIAL_RMT_DEBUG_COUNTERS(Underrun);
        // nothing to send
        DataToSend.val = 0x0;
        Response = false;
    }

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputSerialRmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputSerial::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // (defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(ARDUINO_ARCH_ESP32)
