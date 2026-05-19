/*
* OutputTLS3001Rmt.cpp - TLS3001 driver code for ESPixelStick RMT Channel
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
#if defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "output/OutputTLS3001Rmt.hpp"

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, rmt_item32_t & DataToSend)
{
    return reinterpret_cast<c_OutputTLS3001Rmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
static void StartNewDataFrameBase(void * arg)
{
    return reinterpret_cast<c_OutputTLS3001Rmt*>(arg)->StartNewDataFrame();
}

//----------------------------------------------------------------------------
c_OutputTLS3001Rmt::c_OutputTLS3001Rmt(OM_OutputPortDefinition_t & OutputPortDefinition,
                                       c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputTLS3001 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("TLS3001_PIXEL_RMT_TICKS_BIT: 0x") + String (TLS3001_PIXEL_RMT_TICKS_BIT, HEX));
    fsm_RMT_state_SendSync_imp.SetParent(this);
    fsm_RMT_state_SendReset_imp.SetParent(this);
    fsm_RMT_state_SendDataStart_imp.SetParent(this);
    fsm_RMT_state_SendData_imp.SetParent(this);
    fsm_RMT_state_SendDataIdle_imp.SetParent(this);

    fsm_RMT_state_SendDataIdle_imp.Init ();

    // DEBUG_V("RmtResetOneMsDelay");
    Rmt.SetBitDuration(NanoSecondsInAMilliSecond, RmtResetOneMsDelay, NumResetDelayBits);
    RmtResetOneMsDelay.level0 = 0;
    RmtResetOneMsDelay.level1 = 0;
    // DEBUG_V(String("NumResetDelayBits: ") + String(NumResetDelayBits));
    // DEBUG_V(String("duration0: ") + String(RmtResetOneMsDelay.duration0));
    // DEBUG_V(String("duration1: ") + String(RmtResetOneMsDelay.duration1));

    // DEBUG_END;

} // c_OutputTLS3001Rmt

//----------------------------------------------------------------------------
c_OutputTLS3001Rmt::~c_OutputTLS3001Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTLS3001Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputTLS3001Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputTLS3001::Begin ();
    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputTLS3001::SetConfig (jsonConfig);

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = uint32_t(OutputPortDefinition.PortId);
    OutputRmtConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.arg                     = this;
    OutputRmtConfig.ISR_GetNextIntensityBit = ISR_GetNextBitToSendBase;
    OutputRmtConfig.StartNewDataFrame       = StartNewDataFrameBase;

    // DEBUG_V();
    SetBitTimes();
    fsm_RMT_state_SendDataIdle_imp.Init ();
    Rmt.Begin(OutputRmtConfig, this);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::SetBitTimes ()
{
    // DEBUG_START;

    double NumChannelsAvailable = double(GetNumOutputBufferChannelsServiced());
    double NumPixels            = NumChannelsAvailable / double(PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL);
    double BaudRateMhz          = double(TLS3001_PIXEL_DATA_RATE) / MicroSecondsInASecond;
    // calculate the after sync delay based on bit rate and number of pixels on port
    // (Number of connected chips) ÷ (Communication baud rate in MHz) × 30
    double delayUs              = (NumPixels / BaudRateMhz) * 30;
    double delyNS               = NanoSecondsInAMicroSecond * delayUs;

    // DEBUG_V(String("NumChannelsAvailable: ") + String(NumChannelsAvailable));
    // DEBUG_V(String("           NumPixels: ") + String(NumPixels));
    // DEBUG_V(String("         BaudRateMhz: ") + String(BaudRateMhz));
    // DEBUG_V(String("             delayUs: ") + String(delayUs));
    // DEBUG_V(String("              delyNS: ") + String(delyNS));

    // DEBUG_V("RmtSyncIdle");
    Rmt.SetBitDuration(delyNS, RmtSyncIdle, NumSyncIdleBits);
    RmtSyncIdle.level0 = 0;
    RmtSyncIdle.level1 = 0;

    double BitsPerChannel = 13.0; // 1 zero + 12 data
    double SecPerBit = 1.0 / double(TLS3001_PIXEL_DATA_RATE);
    double NsPerBit = SecPerBit * NanoSecondsInASecond;
    double NsPerChannel = NsPerBit * BitsPerChannel;
    double FrameLenNs = NsPerChannel * NumChannelsAvailable;
    double MinFrameLenNs = 25 * NanoSecondsInAMilliSecond; // Min frame len is 25ms
    double DeltaFrameLenNs = RMT_TickLengthNS;

    if(FrameLenNs < MinFrameLenNs)
    {
        DeltaFrameLenNs = MinFrameLenNs - FrameLenNs;
    }
    // DEBUG_V(String(" BitsPerChannel: ") + String(BitsPerChannel));
    // DEBUG_V(String("      SecPerBit: ") + String(SecPerBit));
    // DEBUG_V(String("       NsPerBit: ") + String(NsPerBit));
    // DEBUG_V(String("   NsPerChannel: ") + String(NsPerChannel));
    // DEBUG_V(String("     FrameLenNs: ") + String(FrameLenNs));
    // DEBUG_V(String("  MinFrameLenNs: ") + String(MinFrameLenNs));
    // DEBUG_V(String("DeltaFrameLenNs: ") + String(DeltaFrameLenNs));

    // DEBUG_V("SendDataIfg");
    Rmt.SetBitDuration(int(DeltaFrameLenNs), RmtIfgBit, NumIfgBitsPerFrame);
    RmtIfgBit.level0 = 0;
    RmtIfgBit.level1 = 0;

    // DEBUG_END;
} // SetBitTimes

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputTLS3001::SetOutputBufferSize (NumChannelsAvailable);
    // DEBUG_V(String("    TLS3001_PIXEL_DATA_RATE: ") + String(TLS3001_PIXEL_DATA_RATE));

    SetBitTimes();

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputTLS3001::GetStatus (jsonStatus);
    Rmt.GetStatus (jsonStatus);
#ifdef USE_RMT_DEBUG_COUNTERS
    // jsonStatus[F("Can Refresh")] = CanRefresh;
    // jsonStatus[F("Cannot Refresh")] = CannotRefresh;
#endif // def USE_RMT_DEBUG_COUNTERS

    #ifdef USE_TLS3001RMT_COUNTERS
    JsonObject CounterStatus = jsonStatus.createNestedObject("TLS3001");
    CounterStatus[F("MinFrameLenMs")]                   = TLS3001RMTCounters.MinFrameLenMs;
    CounterStatus[F("MaxFrameLenMs")]                   = TLS3001RMTCounters.MaxFrameLenMs;
    CounterStatus[F("MinPollTimeMs")]                   = TLS3001RMTCounters.MinPollTimeMs;
    CounterStatus[F("MaxPollTimeMs")]                   = TLS3001RMTCounters.MaxPollTimeMs;
    CounterStatus[F("PollCounter")]                     = TLS3001RMTCounters.PollCounter;
    CounterStatus[F("NoGpio")]                          = TLS3001RMTCounters.NoGpio;
    CounterStatus[F("FramesStarted")]                   = TLS3001RMTCounters.FrameStarted;
    CounterStatus[F("FrameErrorNotFinished")]           = TLS3001RMTCounters.FrameErrorNotFinished;
    CounterStatus[F("TooSoon")]                         = TLS3001RMTCounters.TooSoon;
    CounterStatus[F("SendReset_Init")]                  = TLS3001RMTCounters.SendReset_Init;
    CounterStatus[F("SendReset_GetNextBitToSend")]      = TLS3001RMTCounters.SendReset_GetNextBitToSend;
    CounterStatus[F("SendSync_Init")]                   = TLS3001RMTCounters.SendSync_Init;
    CounterStatus[F("SendSync_GetNextBitToSend")]       = TLS3001RMTCounters.SendSync_GetNextBitToSend;
    CounterStatus[F("SendDataStart_Init")]              = TLS3001RMTCounters.SendDataStart_Init;
    CounterStatus[F("SendDataStart_GetNextBitToSend")]  = TLS3001RMTCounters.SendDataStart_GetNextBitToSend;
    CounterStatus[F("SendData_Init")]                   = TLS3001RMTCounters.SendData_Init;
    CounterStatus[F("SendData_GetNextBitToSend")]       = TLS3001RMTCounters.SendData_GetNextBitToSend;
    CounterStatus[F("SendData_RefreshData")]            = TLS3001RMTCounters.SendData_RefreshData;
    CounterStatus[F("SendDataIdle_Init")]               = TLS3001RMTCounters.SendDataIdle_Init;
    CounterStatus[F("SendDataIdle_GetNextBitToSend")]   = TLS3001RMTCounters.SendDataIdle_GetNextBitToSend;

    #endif // def USE_TLS3001RMT_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputTLS3001Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::StartNewDataFrame()
{
    // DEBUG_START;
    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INCREMENT_TLS3001_COUNTER(FrameStarted);

    if(FrameResetIsNeeded())
    {
        fsm_RMT_state_SendReset_imp.Init();
    }
    else
    {
        fsm_RMT_state_SendDataStart_imp.Init();
    }
    c_OutputTLS3001::StartNewFrame();

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::RmtPoll ()
{
    // DEBUG_START;
    bool Response = false;
    do // Once
    {
        INCREMENT_TLS3001_COUNTER(PollCounter);

        if (gpio_num_t(-1) == OutputPortDefinition.gpios.data)
        {
            INCREMENT_TLS3001_COUNTER(NoGpio);
            break;
        }

        if(!canRefresh())
        {
            INCREMENT_TLS3001_COUNTER(TooSoon);
            // too soon to start another frame
            break;
        }

        if(true == SendingData)
        {
            INCREMENT_TLS3001_COUNTER(FrameErrorNotFinished);
            ForceFrameReset();
        }
        Response = Rmt.StartNewFrame ();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputTLS3001::PauseOutput(State);
    Rmt.PauseOutput(State);
    if(State)
    {
        fsm_RMT_state_SendDataIdle_imp.Init();
    }

    // DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputTLS3001Rmt::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    bool Response = false;
    switch(CurrentFsmState)
    {
        case OutputTLS3001RmtFsmStates_t::TLS3001Sync:
        {
            Response = fsm_RMT_state_SendSync_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataStart:
        {
            Response = fsm_RMT_state_SendDataStart_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataSend:
        {
            Response = fsm_RMT_state_SendData_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataIdle:
        {
            Response = fsm_RMT_state_SendDataIdle_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        default:
        case OutputTLS3001RmtFsmStates_t::TLS3001Reset:
        {
            Response = fsm_RMT_state_SendReset_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
    };
    return Response;
} // ISR_GetNextIntensityToSend

//----------------------------------------------------------------------------
// FSM
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendReset::Init ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendReset_Init);
    // DEBUG_START;

    // Serial.print('b');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001Reset;
    Parent->SendingData = true;
    Parent->ResetFrameCounter();

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000; // reset
    NumberOfIdleBitsToSend = Parent->NumResetDelayBits;

    // DEBUG_END;
} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendReset::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    INCREMENT_TLS3001_FSM_COUNTER(SendReset_GetNextBitToSend);
    // Serial.print('2');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->RmtOneBit;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->RmtOneBit : Parent->RmtZeroBit;
        CommandCodeMask = CommandCodeMask >> 1;
    }
    else if(NumberOfIdleBitsToSend)
    {
        --NumberOfIdleBitsToSend;
        DataToSend = Parent->RmtResetOneMsDelay;
        if(0 == NumberOfIdleBitsToSend)
        {
            Parent->fsm_RMT_state_SendSync_imp.Init();
        }
    }
    else
    {
        Parent->fsm_RMT_state_SendSync_imp.Init();
        Parent->fsm_RMT_state_SendSync_imp.ISR_GetNextBitToSend(DataToSend);
    }

    return true;
} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendSync::Init ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendSync_Init);
    // DEBUG_START;

    // Serial.print('c');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001Sync;
    Parent->SendingData = true;

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000;
    NumberOfZeroBitsToSend = 15;
    NumberOfIdleBitsToSend = Parent->NumSyncIdleBits;

    // DEBUG_END;
} // fsm_RMT_state_SendSync

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendSync::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    INCREMENT_TLS3001_FSM_COUNTER(SendSync_GetNextBitToSend);
    // Serial.print('3');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->RmtOneBit;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->RmtOneBit : Parent->RmtZeroBit;
        CommandCodeMask = CommandCodeMask >> 1;
    }
    else if(NumberOfZeroBitsToSend)
    {
        --NumberOfZeroBitsToSend;
        DataToSend = Parent->RmtZeroBit;
    }
    else if (NumberOfIdleBitsToSend)
    {
        DataToSend = Parent->RmtSyncIdle;
        -- NumberOfIdleBitsToSend;
        if(0 == NumberOfIdleBitsToSend)
        {
            // no more bits to send. Go to the data state
            Parent->fsm_RMT_state_SendDataStart_imp.Init();
        }
    }

    return true;
} // fsm_RMT_state_SendSync

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendDataStart::Init ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendDataStart_Init);
    // DEBUG_START;
    // Serial.print('d');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataStart;
    Parent->SendingData = true;
    Parent->IncrementFrameCounter();

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000;

    // DEBUG_END;
} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendDataStart::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    INCREMENT_TLS3001_FSM_COUNTER(SendDataStart_GetNextBitToSend);
    // Serial.print('4');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->RmtOneBit;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->RmtOneBit : Parent->RmtZeroBit;
        CommandCodeMask = CommandCodeMask >> 1;

        if(0 == CommandCodeMask)
        {
            // no more bits to send. Go to the data state and send the first zero bit.
            Parent->fsm_RMT_state_SendData_imp.Init();
        }
    }

    return true;
} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendData::Init ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendData_Init);
    // DEBUG_START;
    // Serial.print('e');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataSend;
    Parent->SendingData = true;

    ISR_RefreshData();
    // NumPostDataBitsToSend = Parent->NumIfgBitsPerFrame;
    NumPostDataBitsToSend = 0;

    // DEBUG_END;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendData::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    INCREMENT_TLS3001_FSM_COUNTER(SendData_GetNextBitToSend);
    bool Response = true;
    // Serial.print('5');
    // starting zero bit
    if(DataPatternMask)
    {
        DataToSend = (DataPattern & DataPatternMask) ? Parent->RmtOneBit : Parent->RmtZeroBit;
        DataPatternMask = DataPatternMask >> 1;

        // is there more data to send?
        if(0 == DataPatternMask)
        {
            ISR_RefreshData();
        }
    }
    else if(NumPostDataBitsToSend)
    {
        --NumPostDataBitsToSend;
        DataToSend = Parent->RmtIfgBit;
    }
    else
    {
        Parent->fsm_RMT_state_SendDataIdle_imp.Init();
        Response = false;
    }

    return Response;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendData::ISR_RefreshData ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendData_RefreshData);
    MoreDataAvailable = Parent->ISR_MoreDataToSend();

    if(MoreDataAvailable)
    {
        uint32_t NewData = 0;
        Parent->c_OutputTLS3001::ISR_GetNextIntensityToSend(NewData);
        // NewData = 0xff;

        DataPatternMask = 0b1000000000000;
        // convert 8 bit data to 12 bit data
        DataPattern = (NewData * 4095) / 255;
        // DataPattern = 0b111111111111;
        DataPattern = DataPattern & ~DataPatternMask;
    }
    else
    {
        DataPatternMask = 0;
    }

    return MoreDataAvailable;
} // ISR_RefreshData

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendDataIdle::Init ()
{
    INCREMENT_TLS3001_FSM_COUNTER(SendDataIdle_Init);
    // DEBUG_START;
    // Serial.print('f');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataIdle;
    Parent->SendingData = false;
    // DEBUG_END;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendDataIdle::ISR_GetNextBitToSend (rmt_item32_t &DataToSend)
{
    INCREMENT_TLS3001_FSM_COUNTER(SendDataIdle_GetNextBitToSend);
    // Serial.print('6');
    return false;
} // fsm_RMT_state_SendDataIdle

#endif // defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)
