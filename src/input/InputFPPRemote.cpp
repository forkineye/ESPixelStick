/*
* InputFPPRemote.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
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
#include <Int64String.h>
#include "InputFPPRemote.h"
#include "../service/FPPDiscovery.h"

#if defined ARDUINO_ARCH_ESP32
#   include <functional>
#endif

//-----------------------------------------------------------------------------
c_InputFPPRemote::c_InputFPPRemote (
    c_InputMgr::e_InputChannelIds NewInputChannelId,
    c_InputMgr::e_InputType       NewChannelType,
    uint8_t                     * BufferStart,
    uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)
{
    // DEBUG_START;

//     WebMgr.RegisterFPPRemoteCallback ([this](EspFPPRemoteDevice* pDevice) {this->onMessage (pDevice); });

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputFPPRemote::~c_InputFPPRemote ()
{
    FseqFileToPlay = No_FPP_LocalFileToPlay;
    FPPDiscovery.PlayFile (FseqFileToPlay);
    FPPDiscovery.Disable ();

} // ~c_InputFPPRemote

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Begin()
{
    // DEBUG_START;

    LOG_PORT.println (String (F ("** 'FPP Remote' Initialization for Input: '")) + InputChannelId + String (F ("' **")));

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }

    HasBeenInitialized = true;

    FPPDiscovery.Enable ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[JSON_NAME_MISO]         = miso_pin;
    jsonConfig[JSON_NAME_MOSI]         = mosi_pin;
    jsonConfig[JSON_NAME_CLOCK]        = clk_pin;
    jsonConfig[JSON_NAME_CS]           = cs_pin;
    jsonConfig[JSON_NAME_FILE_TO_PLAY] = FseqFileToPlay;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetStatus (JsonObject & /* jsonStatus */)
{
    // DEBUG_START;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Process ()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        FPPDiscovery.ReadNextFrame (InputDataBuffer, InputDataBufferSize);
    }

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

} // SetBufferInfo

//-----------------------------------------------------------------------------
boolean c_InputFPPRemote::SetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    setFromJSON (miso_pin, jsonConfig, JSON_NAME_MISO);
    setFromJSON (mosi_pin, jsonConfig, JSON_NAME_MOSI);
    setFromJSON (clk_pin,  jsonConfig, JSON_NAME_CLOCK);
    setFromJSON (cs_pin,   jsonConfig, JSON_NAME_CS);

    setFromJSON (FseqFileToPlay, jsonConfig, JSON_NAME_FILE_TO_PLAY);

    FileMgr.SetSpiIoPins (miso_pin, mosi_pin, clk_pin, cs_pin);
    FPPDiscovery.PlayFile (FseqFileToPlay);

    // DEBUG_END;
    return true;
} // SetConfig

//-----------------------------------------------------------------------------
//TODO: Add MQTT configuration validation
void c_InputFPPRemote::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate
