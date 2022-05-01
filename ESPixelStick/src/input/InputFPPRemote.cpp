/*
* InputFPPRemote.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
c_InputFPPRemote::c_InputFPPRemote (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                    c_InputMgr::e_InputType       NewChannelType,
                                    size_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)
{
    // DEBUG_START;

//     WebMgr.RegisterFPPRemoteCallback ([this](EspFPPRemoteDevice* pDevice) {this->onMessage (pDevice); });

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputFPPRemote::~c_InputFPPRemote ()
{
    if (HasBeenInitialized)
    {
        StopPlaying ();
    }
} // ~c_InputFPPRemote

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Begin ()
{
    // DEBUG_START;

    HasBeenInitialized = true;

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    if (PlayingFile ())
    {
        jsonConfig[JSON_NAME_FILE_TO_PLAY] = pInputFPPRemotePlayItem->GetFileName ();
    }
    else
    {
        jsonConfig[JSON_NAME_FILE_TO_PLAY] = No_LocalFileToPlay;
    }
    jsonConfig[CN_SyncOffset] = SyncOffsetMS;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject LocalPlayerStatus = jsonStatus.createNestedObject (F ("Player"));
    LocalPlayerStatus[CN_id] = InputChannelId;
    LocalPlayerStatus[CN_active] = PlayingFile ();

    if (PlayingRemoteFile ())
    {
        FPPDiscovery.GetStatus (LocalPlayerStatus);
    }

    else if (PlayingFile ())
    {
        JsonObject PlayerObjectStatus = LocalPlayerStatus.createNestedObject (StatusType);
        pInputFPPRemotePlayItem->GetStatus (PlayerObjectStatus);
    }

    else
    {
        // DEBUG_V ("Not Playing");
    }

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Process ()
{
    // DEBUG_START;
    if (!IsInputChannelActive)
    {
        // DEBUG_V ("dont do anything if the channel is not active");
        StopPlaying ();
    }
    else if (PlayingRemoteFile ())
    {
        // DEBUG_V ("PlayingRemoteFile");
        FPPDiscovery.ReadNextFrame ();
    }
    else if (PlayingFile ())
    {
        // DEBUG_V ("Local File Play");
        pInputFPPRemotePlayItem->Poll ();

        if (pInputFPPRemotePlayItem->IsIdle ())
        {
            // DEBUG_V ("Idle Processing");
            StartPlaying (FileBeingPlayed);
        }
    }
    else
    {
    }

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetBufferInfo (size_t BufferSize)
{
    InputDataBufferSize = BufferSize;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::SetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    String FileToPlay;
    setFromJSON (FileToPlay, jsonConfig, JSON_NAME_FILE_TO_PLAY);
    setFromJSON (SyncOffsetMS, jsonConfig, CN_SyncOffset);
    if (pInputFPPRemotePlayItem)
    {
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
    }

    // DEBUG_V ("Config Processing");
    // Clear outbuffer on config change
    memset (OutputMgr.GetBufferAddress (), 0x0, OutputMgr.GetBufferUsedSize ());
    StartPlaying (FileToPlay);

    // DEBUG_END;

    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StopPlaying ()
{
    // DEBUG_START;
    do // once
    {
        if (!PlayingFile ())
        {
            // DEBUG_ ("Not currently playing a file");
            break;
        }

        // DEBUG_V ();
        FPPDiscovery.Disable ();
        FPPDiscovery.ForgetInputFPPRemotePlayFile ();

        pInputFPPRemotePlayItem->Stop ();

        while (!pInputFPPRemotePlayItem->IsIdle ())
        {
            pInputFPPRemotePlayItem->Poll ();
            pInputFPPRemotePlayItem->Stop ();
        }

        delete pInputFPPRemotePlayItem;
        pInputFPPRemotePlayItem = nullptr;

        FileBeingPlayed = "";

    } while (false);

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlaying (String& FileName)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V (String ("FileName: '") + FileName + "'");
        if ((0 == FileName.length ()) ||
            (FileName == String ("null")))
        {
            // DEBUG_V ("No file to play");
            StopPlaying ();
            break;
        }

        if (FileName == No_LocalFileToPlay)
        {
            StartPlayingRemoteFile (FileName);
        }
        else
        {
            StartPlayingLocalFile (FileName);
        }

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlayingLocalFile (String& FileName)
{
    // DEBUG_START;

    do // once
    {
        if (PlayingRemoteFile ())
        {
            StopPlaying ();
        }

        // are we already playing a local file?
        if (PlayingFile ())
        {
            // DEBUG_V ("PlayingFile");
            // has the file changed?
            if (FileBeingPlayed != FileName)
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
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayList (GetInputChannelId ());
            StatusType = F ("PlayList");
        }
        else
        {
            // DEBUG_V ("Start Local FSEQ file player");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (GetInputChannelId ());
            StatusType = CN_File;
        }

        // DEBUG_V (String ("FileName: '") + FileName + "'");
        // DEBUG_V ("Start Playing");
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->Start (FileName, 0, 1);
        FileBeingPlayed = FileName;

    } while (false);

    // DEBUG_END;

} // StartPlayingLocalFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlayingRemoteFile (String& FileName)
{
    // DEBUG_START;

    do // once
    {
        if (PlayingRemoteFile ())
        {
            // DEBUG_V ("Already Playing in remote mode");
            break;
        }

        StopPlaying ();

        // DEBUG_V ("Instantiate an FSEQ file player");
        pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (GetInputChannelId ());
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        StatusType = CN_File;
        FileBeingPlayed = FileName;

        FPPDiscovery.SetInputFPPRemotePlayFile ((c_InputFPPRemotePlayFile *) pInputFPPRemotePlayItem);
        FPPDiscovery.Enable ();

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::PlayingFile ()
{
    // DEBUG_START;

    bool response = false;

    do // once
    {
        if (nullptr == pInputFPPRemotePlayItem)
        {
            break;
        }

        response = true;
    } while (false);

    // DEBUG_END;
    return response;

} // PlayingFile

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::PlayingRemoteFile ()
{
    // DEBUG_START;

    bool response = false;

    do // once
    {
        if (!PlayingFile ())
        {
            break;
        }

        if (FileBeingPlayed != No_LocalFileToPlay)
        {
            break;
        }

        response = true;

    } while (false);

    // DEBUG_END;
    return response;

} // PlayingRemoteFile
