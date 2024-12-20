/*
* OutputTLS3001Rmt.cpp - TLS3001 driver code for ESPixelStick RMT Channel
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
#if defined(SUPPORT_OutputType_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "output/OutputTLS3001Rmt.hpp"

#define TLS3001_PIXEL_RMT_TICKS_BIT  uint16_t ( (TLS3001_PIXEL_NS_BIT  / RMT_TickLengthNS) / 2)

static fsm_RMT_state_SendStart fsm_RMT_state_SendStart_imp;
static fsm_RMT_state_SendReset fsm_RMT_state_SendReset_imp;
static fsm_RMT_state_SendData  fsm_RMT_state_SendData_imp;

static const c_OutputRmt::ConvertIntensityToRmtDataStreamEntry_t ConvertIntensityToRmtDataStream[] =
{
    // {{.duration0,.level0,.duration1,.level1},Type},

    {{TLS3001_PIXEL_RMT_TICKS_BIT,      1, TLS3001_PIXEL_RMT_TICKS_BIT,      0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STARTBIT_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT,      0, TLS3001_PIXEL_RMT_TICKS_BIT,      1}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT,      1, TLS3001_PIXEL_RMT_TICKS_BIT,      0}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT / 10, 0, TLS3001_PIXEL_RMT_TICKS_BIT / 10, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID},
    {{                               0, 0,                                0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STOPBIT_ID},
    {{                               0, 0,                                0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_LIST_END},
}; // ConvertIntensityToRmtDataStream

//----------------------------------------------------------------------------
c_OutputTLS3001Rmt::c_OutputTLS3001Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputTLS3001 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("TLS3001_PIXEL_RMT_TICKS_BIT: 0x") + String (TLS3001_PIXEL_RMT_TICKS_BIT, HEX));

    fsm_RMT_state_SendStart_imp.Init (this);

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

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    Rmt.SetNumStartBits (15);
    Rmt.SetNumStopBits  (0);
    Rmt.SetNumIdleBits  (0);
    Rmt.Begin (rmt_channel_t (OutputChannelId), gpio_num_t (DataPin), this, rmt_idle_level_t::RMT_IDLE_LEVEL_LOW);

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId     = rmt_channel_t(OutputChannelId);
    OutputRmtConfig.DataPin          = gpio_num_t(DataPin);
    OutputRmtConfig.idle_level       = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.pPixelDataSource = this;
    OutputRmtConfig.CitrdsArray      = ConvertIntensityToRmtDataStream;

    Rmt.Begin(OutputRmtConfig, this);


    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputTLS3001::SetConfig (jsonConfig);

    Rmt.set_pin (DataPin);
    Rmt.SetMinFrameDurationInUs (FrameDurationInMicroSec);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputTLS3001::SetOutputBufferSize (NumChannelsAvailable);
    Rmt.SetMinFrameDurationInUs (FrameDurationInMicroSec);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputTLS3001::GetStatus (jsonStatus);

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputTLS3001Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::RmtPoll ()
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
        pCurrentFsmState->Poll();
        Rmt.StartNewFrame ();

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

    // DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void fsm_RMT_state_SendReset::Init (c_OutputTLS3001Rmt* Parent)
{
    Parent->pCurrentFsmState = this;

} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
void fsm_RMT_state_SendReset::Poll (c_OutputTLS3001Rmt* Parent)
{
    fsm_RMT_state_SendStart_imp.Init (Parent);

} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void fsm_RMT_state_SendStart::Init (c_OutputTLS3001Rmt* Parent)
{
    Parent->pCurrentFsmState = this;

} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
void fsm_RMT_state_SendStart::Poll (c_OutputTLS3001Rmt* Parent)
{
    fsm_RMT_state_SendData_imp.Init (Parent);

} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void fsm_RMT_state_SendData::Init (c_OutputTLS3001Rmt* Parent)
{
    Parent->pCurrentFsmState = this;
    FrameCount = 0;

} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
void fsm_RMT_state_SendData::Poll (c_OutputTLS3001Rmt* Parent)
{
    if (TLS3001_MAX_CONSECUTIVE_DATA_FRAMES < ++FrameCount)
    {
        fsm_RMT_state_SendStart_imp.Init (Parent);
    }

} // fsm_RMT_state_SendData

#endif // defined(SUPPORT_OutputType_TLS3001) && defined (ARDUINO_ARCH_ESP32)
