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
                                    uint32_t                        BufferSize) :
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
    jsonConfig[CN_SendFppSync] = SendFppSync;

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
void c_InputFPPRemote::PlayNextFile ()
{
    // DEBUG_START;
    do // once
    {
        std::vector<String> ListOfFiles;
        FileMgr.GetListOfSdFiles(ListOfFiles);
        ListOfFiles.shrink_to_fit();
        // DEBUG_V(String("Num Files Found: ") + String(ListOfFiles.size()));

        if(0 == ListOfFiles.size())
        {
            // no files. Dont continue
            break;
        }

        // DEBUG_V(String("File Being Played: '") + FileBeingPlayed + "'");
        // find the current file
        std::vector<String>::iterator FileIterator = ListOfFiles.end();
        std::vector<String>::iterator CurrentFileInList = ListOfFiles.begin();
        while (CurrentFileInList != ListOfFiles.end())
        {
            // DEBUG_V(String("CurrentFileInList: '") + *CurrentFileInList + "'");

            if((*CurrentFileInList).equals(FileBeingPlayed))
            {
                // DEBUG_V("Found File");
                FileIterator = ++CurrentFileInList;
                break;
            }
            ++CurrentFileInList;
        }

        // DEBUG_V("now find the next file");
        do
        {
            // did we wrap?
            if(ListOfFiles.end() == CurrentFileInList)
            {
                // DEBUG_V("Handle wrap");
                CurrentFileInList = ListOfFiles.begin();
            }
            // DEBUG_V(String("CurrentFileInList: '") + *CurrentFileInList + "'");

            // is this a valid file?
            if( (-1 != (*CurrentFileInList).indexOf(".fseq")) && (-1 != (*CurrentFileInList).indexOf(".pl")))
            {
                // DEBUG_V("try the next file");
                // get the next file
                ++CurrentFileInList;

                continue;
            }

            // DEBUG_V(String("CurrentFileInList: '") + *CurrentFileInList + "'");

            // did we come all the way around?
            if((*CurrentFileInList).equals(FileBeingPlayed))
            {
                // DEBUG_V("no other file to play. Keep playing it.");
                break;
            }

            // DEBUG_V(String("Succes - we have found a new file to play: '") + *CurrentFileInList + "'");
            StopPlaying();
            StartPlaying(*CurrentFileInList);
            break;
        } while (FileIterator != CurrentFileInList);

    } while(false);

    // DEBUG_END;

} // PlayNextFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Process ()
{
    // DEBUG_START;
    if (!IsInputChannelActive || StayDark)
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

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputFPPRemote::ProcessButtonActions(c_ExternalInput::InputValue_t value)
{
    // DEBUG_START;

    if(c_ExternalInput::InputValue_t::longOn == value)
    {
        // DEBUG_V("flip the dark flag");
        StayDark = !StayDark;
        // DEBUG_V(String("StayDark: ") + String(StayDark));
    }
    else if(c_ExternalInput::InputValue_t::shortOn == value)
    {
        // DEBUG_V("Are we playing a local file?");
        PlayNextFile();
    }
    else if(c_ExternalInput::InputValue_t::off == value)
    {
        // DEBUG_V("Got input Off notification");
    }

    // DEBUG_END;
} // ProcessButtonActions

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetBufferInfo (uint32_t BufferSize)
{
    InputDataBufferSize = BufferSize;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::SetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    String FileToPlay;
    setFromJSON (FileToPlay,   jsonConfig, JSON_NAME_FILE_TO_PLAY);
    setFromJSON (SyncOffsetMS, jsonConfig, CN_SyncOffset);
    setFromJSON (SendFppSync,  jsonConfig, CN_SendFppSync);

    if (pInputFPPRemotePlayItem)
    {
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);
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
        int Last_dot_pl_Position = FileName.lastIndexOf(CN_Dotpl);
        String Last_pl_Text = FileName.substring(Last_dot_pl_Position);
        if (String(CN_Dotpl) == Last_pl_Text)
        {
            // DEBUG_V ("Start Playlist");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayList (GetInputChannelId ());
            StatusType = F ("PlayList");
        }
        else
        {
            int Last_dot_fseq_Position = FileName.lastIndexOf(CN_Dotfseq);
            String Last_fseq_Text = FileName.substring(Last_dot_fseq_Position);
            if (String(CN_Dotfseq) != Last_fseq_Text)
            {
                logcon(String(F("File Name does not end with a valid .fseq or .pl extension: '")) + FileName + "'");
                StatusType = F("Invalid File Name");
                FileBeingPlayed.clear();
                break;
            }

            // DEBUG_V ("Start Local FSEQ file player");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (GetInputChannelId ());
            StatusType = CN_File;
        }

        // DEBUG_V (String ("FileName: '") + FileName + "'");
        // DEBUG_V ("Start Playing");
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);
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
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);
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
