/*
* OutputSerialRmt.cpp - WS2811 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 - 2025 Shelby Merrick
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
#if (defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputSerialRmt.hpp"

//----------------------------------------------------------------------------
c_OutputSerialRmt::c_OutputSerialRmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputSerial (OutputChannelId, outputGpio, uart, outputType)
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

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = rmt_channel_t(OutputChannelId);
    OutputRmtConfig.DataPin                 = gpio_num_t(DataPin);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.pSerialDataSource       = this;
    OutputRmtConfig.SendInterIntensityBits  = true;
    OutputRmtConfig.SendEndOfFrameBits      = true;
    OutputRmtConfig.NumFrameStartBits       = 1;
    OutputRmtConfig.NumIdleBits             = 1;
    OutputRmtConfig.DataDirection           = c_OutputRmt::OutputRmtConfig_t::DataDirection_t::LSB2MSB;

    SetUpRmtBitTimes();
    Rmt.Begin(OutputRmtConfig, this);
    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputSerialRmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputSerial::SetConfig (jsonConfig);

    SetUpRmtBitTimes();

    Rmt.set_pin (DataPin);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSerialRmt::SetUpRmtBitTimes()
{
    rmt_item32_t BitValue;

    float BitTimeNS = (1.0 / float(CurrentBaudrate)) * NanoSecondsInASecond;
    uint32_t OneBitTimeInRmtTicks = uint32_t(BitTimeNS / RMT_TickLengthNS) + 1;
    // DEBUG_V(String(" CurrentBaudrate: ") + String(CurrentBaudrate));
    // DEBUG_V(String("       BitTimeNS: ") + String(BitTimeNS));
    // DEBUG_V(String("RMT_TickLengthNS: ") + String(RMT_TickLengthNS));
    // DEBUG_V(String(" BitTimeRmtTicks: ") + String(OneBitTimeInRmtTicks));

    BitValue.duration0 = OneBitTimeInRmtTicks;
    BitValue.level0 = 1;
    BitValue.duration1 = OneBitTimeInRmtTicks;
    BitValue.level1 = 1;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID);

    BitValue.duration0 = OneBitTimeInRmtTicks / 2;
    BitValue.level0 = 0;
    BitValue.duration1 = OneBitTimeInRmtTicks / 2;
    BitValue.level1 = 0;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_STARTBIT_ID);

    // ISR will process two bits at a time. This updates the bit durration based on baudrate
    BitValue.duration0 = OneBitTimeInRmtTicks / 2;
    BitValue.level0 = 0;
    BitValue.duration1 = OneBitTimeInRmtTicks / 2;
    BitValue.level1 = 0;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID);

    BitValue.duration0 = OneBitTimeInRmtTicks / 2;
    BitValue.level0 = 1;
    BitValue.duration1 = OneBitTimeInRmtTicks / 2;
    BitValue.level1 = 1;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID);

    BitValue.duration0 = OneBitTimeInRmtTicks;
    BitValue.level0 = 1;
    BitValue.duration1 = OneBitTimeInRmtTicks;
    BitValue.level1 = 0;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_TWO_ID);

    BitValue.duration0 = OneBitTimeInRmtTicks;
    BitValue.level0 = 1;
    BitValue.duration1 = OneBitTimeInRmtTicks;
    BitValue.level1 = 1;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_THREE_ID);

    BitValue.duration0 = OneBitTimeInRmtTicks * 2; // Two stop bits
    BitValue.level0 = 1;
    BitValue.duration1 = OneBitTimeInRmtTicks;     // one start bit
    BitValue.level1 = 0;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_STOP_START_BIT_ID);

    // max number of bits per 25ms frame
    float NanoSecsPerFrame = 25.0 * NanoSecondsInAMilliSecond;
    float MaxBitsPerFrame = float(NanoSecsPerFrame / BitTimeNS);
    // number of bits used in frame
    float NumBitsUsedInFrame = OutputBufferSize * (1.0 + 8.0 + 2.0);
    // number of unused frame bits
    float NumUnusedBitsInFrame = MaxBitsPerFrame - NumBitsUsedInFrame;

    if(NumBitsUsedInFrame >= MaxBitsPerFrame)
    {
        // frame is bigger than min frame size set minimum idle time
        NumUnusedBitsInFrame = 40;
    }
    float NumUnusedRmtTicksInFrame = min(float(65535.0), (OneBitTimeInRmtTicks * NumUnusedBitsInFrame));
    // DEBUG_V(String("        NanoSecsPerFrame: ") + String(NanoSecsPerFrame));
    // DEBUG_V(String("         MaxBitsPerFrame: ") + String(MaxBitsPerFrame));
    // DEBUG_V(String("      NumBitsUsedInFrame: ") + String(NumBitsUsedInFrame));
    // DEBUG_V(String("    NumUnusedBitsInFrame: ") + String(NumUnusedBitsInFrame));
    // DEBUG_V(String("    OneBitTimeInRmtTicks: ") + String(OneBitTimeInRmtTicks));
    // DEBUG_V(String("NumUnusedRmtTicksInFrame: ") + String(OneBitTimeInRmtTicks * NumUnusedBitsInFrame));
    // DEBUG_V(String("NumUnusedRmtTicksInFrame: ") + String(NumUnusedRmtTicksInFrame));

    BitValue.duration0 = uint16_t(NumUnusedRmtTicksInFrame/2.0);
    BitValue.level0 = 1;
    BitValue.duration1 = uint16_t(NumUnusedRmtTicksInFrame/2.0);
    BitValue.level1 = 1;
    Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_END_OF_FRAME);

#if defined(SUPPORT_OutputType_DMX)
    if (c_OutputMgr::e_OutputType::OutputType_DMX == OutputType)
    {
        // turn it into a break signal
        BitValue.duration0 = (DMX_BREAK_US * NanoSecondsInAMicroSecond) / RMT_TickLengthNS;
        BitValue.level0 = 0;
        BitValue.duration1 = 2 * ((DMX_MAB_US   * NanoSecondsInAMicroSecond) / RMT_TickLengthNS);
        BitValue.level1 = 1;
        Rmt.SetIntensity2Rmt(BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID);
    }
#endif // defined(SUPPORT_OutputType_DMX)

#if defined(SUPPORT_OutputType_Serial)
#endif // defined(SUPPORT_OutputType_Serial)

#if defined(SUPPORT_OutputType_Renard)
#endif // defined(SUPPORT_OutputType_Renard)

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
        if (gpio_num_t(-1) == DataPin)
        {
            break;
        }

        // DEBUG_V("get the next frame started");
        ReportNewFrame ();
        Rmt.StartNewFrame ();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputSerialRmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputSerial::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // (defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)) && defined(ARDUINO_ARCH_ESP32)
