/*
* OutputGECERmt.cpp - GECE driver code for ESPixelStick RMT Channel
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
#if defined(SUPPORT_OutputType_GECE) && defined(ARDUINO_ARCH_ESP32)

#include "output/OutputGECERmt.hpp"

// The adjustments compensate for rounding errors in the calculations
#define GECE_PIXEL_RMT_TICKS_BIT_0_HIGH    uint16_t ( (GECE_PIXEL_NS_BIT_0_HIGH / RMT_TickLengthNS) + 0.0)
#define GECE_PIXEL_RMT_TICKS_BIT_0_LOW     uint16_t ( (GECE_PIXEL_NS_BIT_0_LOW  / RMT_TickLengthNS) + 0.0)
#define GECE_PIXEL_RMT_TICKS_BIT_1_HIGH    uint16_t ( (GECE_PIXEL_NS_BIT_1_HIGH / RMT_TickLengthNS) - 1.0)
#define GECE_PIXEL_RMT_TICKS_BIT_1_LOW     uint16_t ( (GECE_PIXEL_NS_BIT_1_LOW  / RMT_TickLengthNS) + 1.0)
#define GECE_PIXEL_RMT_TICKS_STOP          uint16_t ( (GECE_PIXEL_STOP_TIME_NS  / RMT_TickLengthNS) + 1.0)
#define GECE_PIXEL_RMT_TICKS_START         uint16_t ( (GECE_PIXEL_START_TIME_NS  / RMT_TickLengthNS) + 1.0)

static const c_OutputRmt::ConvertIntensityToRmtDataStreamEntry_t ConvertIntensityToRmtDataStream[] =
{
    // {{.duration0,.level0,.duration1,.level1},Type},

    {{GECE_PIXEL_RMT_TICKS_BIT_0_LOW, 0, GECE_PIXEL_RMT_TICKS_BIT_0_HIGH, 1}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID},
    {{GECE_PIXEL_RMT_TICKS_BIT_1_LOW, 0, GECE_PIXEL_RMT_TICKS_BIT_1_HIGH, 1}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID},
    {{GECE_PIXEL_RMT_TICKS_START / 2, 0, GECE_PIXEL_RMT_TICKS_START / 2,  0}, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID},
    {{GECE_PIXEL_RMT_TICKS_START / 2, 1, GECE_PIXEL_RMT_TICKS_START / 2,  1}, c_OutputRmt::RmtDataBitIdType_t::RMT_STARTBIT_ID},
    {{GECE_PIXEL_RMT_TICKS_STOP  / 2, 0, GECE_PIXEL_RMT_TICKS_STOP  / 2,  0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STOPBIT_ID},
    {{GECE_PIXEL_RMT_TICKS_STOP,      0, GECE_PIXEL_RMT_TICKS_START,      1}, c_OutputRmt::RmtDataBitIdType_t::RMT_STOP_START_BIT_ID},
    {{                             0, 0,                               0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_LIST_END},
}; // RmtBitDefinitions

//----------------------------------------------------------------------------
c_OutputGECERmt::c_OutputGECERmt(c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                 gpio_num_t outputGpio,
                                 uart_port_t uart,
                                 c_OutputMgr::e_OutputType outputType) : c_OutputGECE(OutputChannelId, outputGpio, uart, outputType)
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

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId            = rmt_channel_t(OutputChannelId);
    OutputRmtConfig.DataPin                 = gpio_num_t(DataPin);
    OutputRmtConfig.idle_level              = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.pPixelDataSource        = this;
    OutputRmtConfig.IntensityDataWidth      = GECE_PACKET_SIZE;
    OutputRmtConfig.SendInterIntensityBits  = true;
    OutputRmtConfig.CitrdsArray             = ConvertIntensityToRmtDataStream;

    Rmt.Begin(OutputRmtConfig, this);

    HasBeenInitialized = true;

    // Start output
    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputGECERmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGECE::SetConfig (jsonConfig);

    Rmt.set_pin (DataPin);

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
void c_OutputGECERmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputGECE::PauseOutput(State);
    Rmt.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_GECE) && defined(ARDUINO_ARCH_ESP32)
