/*
* OutputUCS1903Rmt.cpp - UCS1903 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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
#include "../ESPixelStick.h"
#ifdef SUPPORT_OutputType_UCS1903

#ifdef SUPPORT_RMT_OUTPUT

#include "OutputUCS1903Rmt.hpp"

// The adjustments compensate for rounding errors in the calculations
#define UCS1903_PIXEL_RMT_TICKS_BIT_0_HIGH    uint16_t ( (UCS1903_PIXEL_NS_BIT_0_HIGH / RMT_TickLengthNS) + 0.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_0_LOW     uint16_t ( (UCS1903_PIXEL_NS_BIT_0_LOW  / RMT_TickLengthNS) + 0.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_1_HIGH    uint16_t ( (UCS1903_PIXEL_NS_BIT_1_HIGH / RMT_TickLengthNS) - 1.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_1_LOW     uint16_t ( (UCS1903_PIXEL_NS_BIT_1_LOW  / RMT_TickLengthNS) + 1.0)
#define UCS1903_PIXEL_RMT_TICKS_IDLE          uint16_t ( (UCS1903_PIXEL_IDLE_TIME_NS  / RMT_TickLengthNS) + 1.0)

//----------------------------------------------------------------------------
c_OutputUCS1903Rmt::c_OutputUCS1903Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputUCS1903 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    rmt_item32_t BitValue;

    BitValue.duration0 = UCS1903_PIXEL_RMT_TICKS_BIT_0_HIGH;
    BitValue.level0 = 1;
    BitValue.duration1 = UCS1903_PIXEL_RMT_TICKS_BIT_0_LOW;
    BitValue.level1 = 0;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_DATA_BIT_ZERO_ID);

    BitValue.duration0 = UCS1903_PIXEL_RMT_TICKS_BIT_1_HIGH;
    BitValue.level0 = 1;
    BitValue.duration1 = UCS1903_PIXEL_RMT_TICKS_BIT_1_LOW;
    BitValue.level1 = 0;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_DATA_BIT_ONE_ID);

    BitValue.duration0 = UCS1903_PIXEL_RMT_TICKS_IDLE / 10;
    BitValue.level0 = 0;
    BitValue.duration1 = UCS1903_PIXEL_RMT_TICKS_IDLE / 10;
    BitValue.level1 = 1;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_INTERFRAME_GAP_ID);

    BitValue.duration0 = 1;
    BitValue.level0 = 0;
    BitValue.duration1 = 1;
    BitValue.level1 = 0;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_STARTBIT_ID);

    BitValue.duration0 = 0;
    BitValue.level0 = 0;
    BitValue.duration1 = 0;
    BitValue.level1 = 0;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_STOPBIT_ID);

    // DEBUG_V (String ("UCS1903_PIXEL_RMT_TICKS_BIT_0_H: 0x") + String (UCS1903_PIXEL_RMT_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String ("UCS1903_PIXEL_RMT_TICKS_BIT_0_L: 0x") + String (UCS1903_PIXEL_RMT_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("UCS1903_PIXEL_RMT_TICKS_BIT_1_H: 0x") + String (UCS1903_PIXEL_RMT_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("UCS1903_PIXEL_RMT_TICKS_BIT_1_L: 0x") + String (UCS1903_PIXEL_RMT_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputUCS1903Rmt

//----------------------------------------------------------------------------
c_OutputUCS1903Rmt::~c_OutputUCS1903Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputUCS1903Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputUCS1903Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputUCS1903::Begin ();

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    Rmt.Begin (rmt_channel_t (OutputChannelId), gpio_num_t (DataPin), this, rmt_idle_level_t::RMT_IDLE_LEVEL_LOW);

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputUCS1903Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputUCS1903::SetConfig (jsonConfig);

    uint32_t ifgNS = (InterFrameGapInMicroSec * 1000);
    uint32_t ifgTicks = ifgNS / RMT_TickLengthNS;

    // Default is 100us * 3
    rmt_item32_t BitValue;
    BitValue.duration0 = ifgTicks / 10;
    BitValue.level0 = 0;
    BitValue.duration1 = ifgTicks / 10;
    BitValue.level1 = 0;
    Rmt.SetRgb2Rmt (BitValue, c_OutputRmt::RmtFrameType_t::RMT_INTERFRAME_GAP_ID);

    Rmt.set_pin (DataPin);
    Rmt.SetMinFrameDurationInUs (FrameMinDurationInMicroSec);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputUCS1903Rmt::SetOutputBufferSize (uint16_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputUCS1903::SetOutputBufferSize (NumChannelsAvailable);
    Rmt.SetMinFrameDurationInUs (FrameMinDurationInMicroSec);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputUCS1903Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputUCS1903::GetStatus (jsonStatus);
    // Rmt.GetStatus (jsonStatus);
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputUCS1903Rmt::Render ()
{
    // DEBUG_START;

    if (Rmt.Render ())
    {
        ReportNewFrame ();
    }

    // DEBUG_END;

} // Render

#endif // def SUPPORT_RMT_OUTPUT
#endif // def SUPPORT_OutputType_UCS1903
