/*
* InputFPPRemote.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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
#include "input/InputFPPRemote.h"
#include "service/FPPDiscovery.h"
#include "input/InputFPPRemotePlayFile.hpp"
#include "input/InputFPPRemotePlayList.hpp"
#include <Int64String.h>


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
        // DEBUG_V();
        StopPlaying ();
        FPPDiscovery.ForgetInputFPPRemotePlayFile ();
        FPPDiscovery.Disable();
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

    FPPDiscovery.GetConfig(jsonConfig);
    JsonWrite(jsonConfig, CN_fseqfilename, ConfiguredFileToPlay);
    JsonWrite(jsonConfig, CN_SyncOffset,   SyncOffsetMS);
    JsonWrite(jsonConfig, CN_SendFppSync,  SendFppSync);
    JsonWrite(jsonConfig, CN_FPPoverride,  FppSyncOverride);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject LocalPlayerStatus = jsonStatus[F ("Player")].to<JsonObject> ();
    JsonWrite(LocalPlayerStatus, CN_id,     InputChannelId);
    JsonWrite(LocalPlayerStatus, CN_active, PlayingFile ());

    if (PlayingRemoteFile ())
    {
        FPPDiscovery.GetStatus (LocalPlayerStatus);
    }

    else if (PlayingFile ())
    {
        JsonObject PlayerObjectStatus = LocalPlayerStatus[StatusType].to<JsonObject> ();
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

    if (!IsInputChannelActive)
    {
        // DEBUG_V ("dont do anything if the channel is not active");
    }
    else if (PlayingRemoteFile ())
    {
        // DEBUG_V ("Remote File Play");
        while(Poll ()) {}
    }
    else if (PlayingFile ())
    {
        // DEBUG_V ("Local File Play");
        while(Poll ()) {}

        if (pInputFPPRemotePlayItem->IsIdle ())
        {
            // DEBUG_V ("Idle Processing");
            StartPlaying (FileBeingPlayed);
        }
    }
    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::Poll ()
{
    // DEBUG_START;
    bool Response = false;
    if(pInputFPPRemotePlayItem && IsInputChannelActive)
    {
        Response = pInputFPPRemotePlayItem->Poll ();
    }

    // DEBUG_END;
    return Response;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemote::ProcessButtonActions(c_ExternalInput::InputValue_t value)
{
    // DEBUG_START;

    if(c_ExternalInput::InputValue_t::longOn == value)
    {
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
    FPPDiscovery.SetConfig(jsonConfig);
    setFromJSON (FileToPlay,      jsonConfig, CN_fseqfilename);
    setFromJSON (SyncOffsetMS,    jsonConfig, CN_SyncOffset);
    setFromJSON (SendFppSync,     jsonConfig, CN_SendFppSync);
    setFromJSON (FppSyncOverride, jsonConfig, CN_FPPoverride);
    ConfiguredFileToPlay = FileToPlay;

    if (PlayingFile())
    {
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);
        SetBackgroundFile();
    }

    // DEBUG_V ("Config Processing");
    // Clear outbuffer on config change
    OutputMgr.ClearBuffer();
    StartPlaying (FileToPlay);

    // DEBUG_END;

    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetOperationalState (bool ActiveFlag)
{
    // DEBUG_START;

    if(pInputFPPRemotePlayItem)
    {
        pInputFPPRemotePlayItem->SetOperationalState (ActiveFlag);
    }

    FPPDiscovery.SetOperationalState(ActiveFlag);

    // DEBUG_END;
} // SetOperationalState

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StopPlaying ()
{
    // DEBUG_START;

    do // once
    {
        if (!PlayingFile ())
        {
            // DEBUG_V ("Not currently playing a file");
            break;
        }

        // handle reentrancy
        if(Stopping)
        {
            // DEBUG_V("already in the process of stopping");
            break;
        }
        Stopping = true;

        if(PlayingFile())
        {
            // DEBUG_V("Tell player to stop");
            FppStopRemoteFilePlay();

            // DEBUG_V("Delete current playing file");
            delete pInputFPPRemotePlayItem;
            pInputFPPRemotePlayItem = nullptr;
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        }

        Stopping = false;

    } while (false);

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlaying (String& FileName)
{
    // DEBUG_START;

    do // once
    {
        if(!IsInputChannelActive)
        {
            break;
        }

        // DEBUG_V (String ("FileName: '") + FileName + "'");
        if ((FileName.isEmpty ()) ||
            (FileName.equals("null")))
        {
            // DEBUG_V ("No file to play");
            StopPlaying ();
            break;
        }

        if (FppSyncOverride || FileName.equals(CN_No_LocalFileToPlay))
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
        // DEBUG_V("make sure we are stopped (clears pInputFPPRemotePlayItem)");
        StopPlaying();

        // DEBUG_V ("Start A New File");
        int Last_dot_pl_Position = FileName.lastIndexOf(CN_Dotpl);
        String Last_pl_Text = FileName.substring(Last_dot_pl_Position);
        if (String(CN_Dotpl) == Last_pl_Text)
        {
            if(pInputFPPRemotePlayItem)
            {
                // DEBUG_V ("Delete existing play item");
                delete pInputFPPRemotePlayItem;
                pInputFPPRemotePlayItem = nullptr;
                // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
            }
            // DEBUG_V ("Start a new Local File");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayList (GetInputChannelId ());
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
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

            if(pInputFPPRemotePlayItem)
            {
                // DEBUG_V ("Delete existing item");
                delete pInputFPPRemotePlayItem;
                pInputFPPRemotePlayItem = nullptr;
                // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
            }
            // DEBUG_V ("Start Local FSEQ file player");
            pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (GetInputChannelId ());
            StatusType = CN_File;
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        }

        // DEBUG_V (String ("Start Playing FileName: '") + FileName + "'");
        pInputFPPRemotePlayItem->ClearFileNames();
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

        // DEBUG_V();
        StopPlaying ();

        // DEBUG_V ("Instantiate a new FSEQ file player");
        if(pInputFPPRemotePlayItem)
        {
            // DEBUG_V ("Delete existing play item");
            c_InputFPPRemotePlayItem * temp = pInputFPPRemotePlayItem;
            pInputFPPRemotePlayItem = nullptr;
            delete temp;
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        }
        // DEBUG_V ("Start Local FSEQ file player");
        pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (GetInputChannelId ());
        // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);

        SetBackgroundFile();

        StatusType = CN_File;
        FileBeingPlayed = FileName;

        FPPDiscovery.SetInputFPPRemotePlayFile (this);
        FPPDiscovery.Enable ();

    } while (false);

    // DEBUG_END;

} // StartPlayingRemoteFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::PlayingFile ()
{
    return (nullptr != pInputFPPRemotePlayItem);
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

        if (!FppSyncOverride && !FileBeingPlayed.equals(CN_No_LocalFileToPlay))
        {
            break;
        }

        response = true;

    } while (false);

    // DEBUG_END;
    return response;

} // PlayingRemoteFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::FppStartRemoteFilePlay (String & FileName, uint32_t ElapsedTimeSec)
{
    // DEBUG_START;

    if (AllowedToPlayRemoteFile())
    {
        // DEBUG_V ("Ask FSM to start playing");
        pInputFPPRemotePlayItem->Start (FileName, ElapsedTimeSec, 1);
    }

    // DEBUG_END;
} // FppStartRemoteFilePlay

//-----------------------------------------------------------------------------
void c_InputFPPRemote::FppStopRemoteFilePlay ()
{
    // DEBUG_START;

    // only process if the pointer is valid
    while (AllowedToPlayRemoteFile())
    {
        // DEBUG_V("Pointer is valid");
        if(pInputFPPRemotePlayItem->IsIdle())
        {
            // DEBUG_V("Play item is Idle.");
            break;
        }

        // DEBUG_V("try to stop");
        pInputFPPRemotePlayItem->ClearFileNames();
        pInputFPPRemotePlayItem->Stop ();
        pInputFPPRemotePlayItem->Poll ();

        FeedWDT();
        delay(5);
    }

    // DEBUG_END;
} // FppStopRemoteFilePlay

//-----------------------------------------------------------------------------
void c_InputFPPRemote::FppSyncRemoteFilePlay (String & FileName, uint32_t ElapsedTimeSec)
{
    // DEBUG_START;

    if (AllowedToPlayRemoteFile())
    {
        // DEBUG_V("Send Sync message")
        pInputFPPRemotePlayItem->Sync(FileName, ElapsedTimeSec);
    }

    // DEBUG_END;
} // FppSyncRemoteFilePlay

//-----------------------------------------------------------------------------
void c_InputFPPRemote::GetFppRemotePlayStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    if (pInputFPPRemotePlayItem)
    {
        // DEBUG_V ("Ask FSM to start playing");
        pInputFPPRemotePlayItem->GetStatus (jsonStatus);
    }

    // DEBUG_END;
} // GetFppRemotePlayStatus

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::IsIdle ()
{
    // DEBUG_START;
    bool Response = true;
    if (pInputFPPRemotePlayItem)
    {
        // DEBUG_V ("Ask FSM to start playing");
        Response = pInputFPPRemotePlayItem->IsIdle ();
    }
    return Response;
    // DEBUG_END;
} // GetFppRemotePlayStatus

//-----------------------------------------------------------------------------
bool c_InputFPPRemote::AllowedToPlayRemoteFile()
{
    // DEBUG_START;

    bool Response = false;

    if (pInputFPPRemotePlayItem &&
        (ConfiguredFileToPlay.equals(CN_No_LocalFileToPlay) || FppSyncOverride))
    {
        // DEBUG_V ("FPP Is allowed to control playing files");
        Response = true;
    }

    // DEBUG_END;
    return Response;
} // AllowedToPlayRemoteFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetBackgroundFile ()
{
    // DEBUG_START;

    if(FppSyncOverride)
    {
        String Background = ConfiguredFileToPlay.equals(CN_No_LocalFileToPlay) ? emptyString : ConfiguredFileToPlay;
        pInputFPPRemotePlayItem->SetBackgroundFileName(Background);
        // DEBUG_V(String("Background: ") + Background);
    }
    else
    {
        pInputFPPRemotePlayItem->ClearFileNames();
    }

    // DEBUG_END;
} // SetBackgroundFile
