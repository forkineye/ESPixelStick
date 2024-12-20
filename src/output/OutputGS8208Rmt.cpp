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

#if defined(SUPPORT_OutputType_GS8208) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputGS8208Rmt.hpp"

// The adjustments compensate for rounding errors in the calculations
#define GS8208_PIXEL_RMT_TICKS_BIT_0_HIGH    uint16_t ( (GS8208_PIXEL_NS_BIT_0_HIGH / RMT_TickLengthNS) + 0.0)
#define GS8208_PIXEL_RMT_TICKS_BIT_0_LOW     uint16_t ( (GS8208_PIXEL_NS_BIT_0_LOW  / RMT_TickLengthNS) + 0.0)
#define GS8208_PIXEL_RMT_TICKS_BIT_1_HIGH    uint16_t ( (GS8208_PIXEL_NS_BIT_1_HIGH / RMT_TickLengthNS) - 1.0)
#define GS8208_PIXEL_RMT_TICKS_BIT_1_LOW     uint16_t ( (GS8208_PIXEL_NS_BIT_1_LOW  / RMT_TickLengthNS) + 1.0)
#define GS8208_PIXEL_RMT_TICKS_IDLE          uint16_t ( (GS8208_PIXEL_IDLE_TIME_NS  / RMT_TickLengthNS) + 1.0)

static const c_OutputRmt::ConvertIntensityToRmtDataStreamEntry_t ConvertIntensityToRmtDataStream[] =
{
    // {{.duration0,.level0,.duration1,.level1},Type},

    {{GS8208_PIXEL_RMT_TICKS_BIT_0_HIGH, 1, GS8208_PIXEL_RMT_TICKS_BIT_0_LOW, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID},
    {{GS8208_PIXEL_RMT_TICKS_BIT_1_HIGH, 1, GS8208_PIXEL_RMT_TICKS_BIT_1_LOW, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID},
    {{                                2, 0,                                2, 1}, c_OutputRmt::RmtDataBitIdType_t::RMT_STARTBIT_ID},
    {{                                0, 0,                                0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STOPBIT_ID},
    {{                                0, 0,                                0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_LIST_END},
}; // ConvertIntensityToRmtDataStream

//----------------------------------------------------------------------------
c_OutputGS8208Rmt::c_OutputGS8208Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputGS8208 (OutputChannelId, outputGpio, uart, outputType)
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

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId     = rmt_channel_t(OutputChannelId);
    OutputRmtConfig.DataPin          = gpio_num_t(DataPin);
    OutputRmtConfig.idle_level       = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.pPixelDataSource = this;
    OutputRmtConfig.CitrdsArray      = ConvertIntensityToRmtDataStream;

    Rmt.Begin(OutputRmtConfig, this);

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputGS8208Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGS8208::SetConfig (jsonConfig);

    uint32_t ifgNS = (InterFrameGapInMicroSec * NanoSecondsInAMicroSecond);
    uint32_t ifgTicks = ifgNS / RMT_TickLengthNS;

    // Default is 100us * 3
    rmt_item32_t BitValue;
    // by default there are 6 rmt_item32_t instances replicated for the start of a frame.
    // 6 instances times 2 time periods per instance = 12
    BitValue.duration0 = ifgTicks / 12;
    BitValue.level0 = 0;
    BitValue.duration1 = ifgTicks / 12;
    BitValue.level1 = 0;
    Rmt.SetIntensity2Rmt (BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID);

    Rmt.set_pin (DataPin);

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
void c_OutputGS8208Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputGS8208::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_GS8208) && defined(ARDUINO_ARCH_ESP32)
