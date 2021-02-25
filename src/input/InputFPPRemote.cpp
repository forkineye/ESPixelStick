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
#include "InputFPPRemotePlayFile.hpp"
#include "InputFPPRemotePlayList.hpp"

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
    StopPlaying ();
    FPPDiscovery.Disable ();

} // ~c_InputFPPRemote

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Begin()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }

    HasBeenInitialized = true;

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
    if (PlayingFile ())
    {
        jsonConfig[JSON_NAME_FILE_TO_PLAY] = pInputFPPRemotePlayItem->GetFileName();
    }
    else
    {
        jsonConfig[JSON_NAME_FILE_TO_PLAY] = No_LocalFileToPlay;
    }

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonObject LocalPlayerStatus = jsonStatus.createNestedObject (F ("LocalPlayer"));
    LocalPlayerStatus[CN_active] = PlayingFile ();

    if (PlayingFile ())
    {
        JsonObject PlayerObjectStatus = LocalPlayerStatus.createNestedObject (StatusType);
        pInputFPPRemotePlayItem->GetStatus (PlayerObjectStatus);
    }

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Process ()
{
    // DEBUG_START;

    if (PlayingFile ())
    {
        pInputFPPRemotePlayItem->Poll (InputDataBuffer, InputDataBufferSize);

        if (pInputFPPRemotePlayItem->IsIdle ())
        {
            // DEBUG_V ("Idle Processing");
            String FileName = pInputFPPRemotePlayItem->GetFileName ();
            StartPlaying (FileName);
        }
    }
    else
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

    String FileToPlay;
    setFromJSON (FileToPlay, jsonConfig, JSON_NAME_FILE_TO_PLAY);

    FileMgr.SetSpiIoPins (miso_pin, mosi_pin, clk_pin, cs_pin);

    // DEBUG_V ("Config Processing");
    StartPlaying (FileToPlay);

    // DEBUG_END;

    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StopPlaying ()
{
    // DEBUG_START;

    if (PlayingFile ())
    {
        pInputFPPRemotePlayItem->Stop ();
        delete pInputFPPRemotePlayItem;
        pInputFPPRemotePlayItem = nullptr;
    }

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlaying (String & FileName)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V (String ("FileName: '") + FileName + "'");
        if ((0 == FileName.length ()) ||
            (FileName == No_LocalFileToPlay) ||
            (FileName == String("null")) )
        {
            StopPlaying ();
            // DEBUG_V ("Enable FPP Remote");
            FPPDiscovery.Enable ();
            break;
        }
        // DEBUG_V ("Disable FPP Remote");
        FPPDiscovery.Disable ();
        // DEBUG_V ("Disable FPP Remote Done");

        // are we already playing a file?
        if (PlayingFile ())
        {
            // DEBUG_V ("PlayingFile");
            // has the file changed?
            if (pInputFPPRemotePlayItem->GetFileName () != FileName)
            {
                // DEBUG_V ("StopPlaying");
                StopPlaying ();
            }
            else
            {
                // DEBUG_V ("Play It Again");
                pInputFPPRemotePlayItem->Start (FileName, 0, 1);
                break;
            }
        }

        // DEBUG_V ("Start A New File");

        if (-1 != FileName.indexOf (".pl"))
        {
            // DEBUG_V ("Start Playlist");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayList ();
            StatusType = F ("PlayList");
        }
        else
        {
            // DEBUG_V ("Start Local FSEQ file player");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile ();
            StatusType = CN_File;
        }

        // DEBUG_V (String ("FileName: '") + FileName + "'");
        // DEBUG_V ("Start Playing");
        pInputFPPRemotePlayItem->Start (FileName, 0, 1);

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
//TODO: Add MQTT configuration validation
void c_InputFPPRemote::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate
