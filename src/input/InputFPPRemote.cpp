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

#define AllocatePlayer(ClassType, ChannelID) \
{ \
    if(!PlayerInfo.InUse) \
    { \
        static_assert(sizeof(PlayerInfo.Player) >= sizeof(ClassType)); \
        new(PlayerInfo.Player) ClassType(ChannelID); \
        PlayerInfo.InUse = true; \
        pInputFPPRemotePlayItem = (c_InputFPPRemotePlayItem*)(&PlayerInfo.Player[0]); \
    } \
    else \
    { \
        logcon("ERROR: Trying to start the player when it is already running"); \
    } \
}

#define DeAllocatePlayer() \
{ \
    if(PlayerInfo.InUse) \
    { \
        pInputFPPRemotePlayItem = nullptr; \
        PlayerInfo.InUse = false; \
        ((c_InputFPPRemotePlayItem*)(&PlayerInfo.Player[0]))->~c_InputFPPRemotePlayItem(); \
        memset(PlayerInfo.Player, 0x0, sizeof(PlayerInfo.Player)); \
    } \
    else \
    { \
        logcon(F("ERROR: Trying to stop player when no player is running")); \
    } \
}

//-----------------------------------------------------------------------------
c_InputFPPRemote::c_InputFPPRemote (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                    c_InputMgr::e_InputType       NewChannelType,
                                    uint32_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)
{
    // DEBUG_START;

    SafeStrncpy(FileBeingPlayed, CN_No_LocalFileToPlay, sizeof(FileBeingPlayed));
    SafeStrncpy(ConfiguredFileToPlay, CN_No_LocalFileToPlay, sizeof(ConfiguredFileToPlay));

    memset(StatusType, 0x0, sizeof(StatusType));

//     WebMgr.RegisterFPPRemoteCallback ([this](EspFPPRemoteDevice* pDevice) {this->onMessage (pDevice); });

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputFPPRemote::~c_InputFPPRemote ()
{
    if (HasBeenInitialized)
    {
        // DEBUG_V("StopPlaying");
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
        // DEBUG_V("PlayingRemoteFile");
        FPPDiscovery.GetStatus (LocalPlayerStatus);
        FilePlayCount = pInputFPPRemotePlayItem->GetPlayedFileCount();
    }

    else if (PlayingFile ())
    {
        // DEBUG_V("Playing Local File");
        JsonObject PlayerObjectStatus = LocalPlayerStatus[StatusType].to<JsonObject> ();
        JsonWrite(PlayerObjectStatus, CN_count, FilePlayCount);
        if(pInputFPPRemotePlayItem)
        {
            pInputFPPRemotePlayItem->GetStatus (PlayerObjectStatus);
        }
    }

    else
    {
        // DEBUG_V ("Not Playing");
    }

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemote::ClearStatistics ()
{
    // DEBUG_START;

    FilePlayCount = 0;

    if (PlayingRemoteFile ())
    {
        // DEBUG_V();
        FPPDiscovery.ClearStatistics ();
    }

    else if (PlayingFile ())
    {
        if(pInputFPPRemotePlayItem)
        {
            pInputFPPRemotePlayItem->ClearStatistics ();
        }
    }

    else
    {
        // DEBUG_V ("Not Playing");
    }

    // DEBUG_END;

} // ClearStatistics

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
            // StopPlaying();
            StartPlaying(*CurrentFileInList, 0, false);
            break;
        } while (FileIterator != CurrentFileInList);

    } while(false);

    // DEBUG_END;

} // PlayNextFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::Process ()
{
    // DEBUG_START;
    do // once
    {
        if (!IsInputChannelActive || IsBooting)
        {
            // DEBUG_V ("dont do anything if the channel is not active");
            break;
        }

        if (PlayingRemoteFile ())
        {
            // DEBUG_V ("Poll Remote File Play");
            while(Poll ()) {}
            break;
        }

        if (PlayingFile ())
        {
            // DEBUG_V ("Poll Local File Play");
            while(Poll ()) {}

            if (pInputFPPRemotePlayItem->IsIdle ())
            {
                // DEBUG_V ("Idle Processing");
                String temp = String(ConfiguredFileToPlay);
                StartPlaying (temp, 0, FppSyncOverride);
                // DEBUG_V("Start Playing Remote Done");
            }
            break;
        }

        // DEBUG_V("not playing anything. Start Playing");
        String temp = String(ConfiguredFileToPlay);
        StartPlaying(temp, 0, FppSyncOverride);
        // DEBUG_V("Start Playing anything Done");

    } while(false);

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
    SafeStrncpy(ConfiguredFileToPlay, FileToPlay.c_str(), sizeof(ConfiguredFileToPlay));

    // Clear outbuffer on config change
    OutputMgr.ClearBuffer();

    // make sure any changes made get implemented.
    StopPlaying();

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
            DeAllocatePlayer();
        }
        else
        {
            // DEBUG_V("Not playing a file");
        }

        Stopping = false;

    } while (false);

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlaying (String& FileName, time_t ElapsedSeconds, bool IsRemote)
{
    // DEBUG_START;

    do // once
    {
        if(!IsInputChannelActive || IsBooting)
        {
            // DEBUG_V(String("IsInputChannelActive: ") + String(IsInputChannelActive));
            // DEBUG_V(String("           IsBooting: ") + String(IsBooting));
            break;
        }

        StopPlaying ();
        bool ConfiguredLocalFileToPlay = !String(ConfiguredFileToPlay).equals(CN_No_LocalFileToPlay);

        // DEBUG_V (String("                 FileName: '") + FileName + "'");
        // DEBUG_V (String("     ConfiguredFileToPlay: '") + ConfiguredFileToPlay + "'");
        // DEBUG_V (String("          FppSyncOverride: ")  + String(FppSyncOverride));
        // DEBUG_V (String("                 IsRemote: ")  + String(IsRemote));
        // DEBUG_V (String("ConfiguredLocalFileToPlay: ")  + String(ConfiguredLocalFileToPlay));

        if (!ConfiguredLocalFileToPlay || (FppSyncOverride && ConfiguredLocalFileToPlay))
        {
            if(!ConfiguredLocalFileToPlay)
            {
                FileName.clear();
            }
            // DEBUG_V("we are allowed to play remote files")
            StartPlayingRemoteFile (FileName, ElapsedSeconds);
            pInputFPPRemotePlayItem->SetPlayedFileCount (++FilePlayCount);
        }
        else if (!FileName.isEmpty () && !FileName.equals("null") && ConfiguredLocalFileToPlay)
        {
            // DEBUG_V("we are only allowed to play local files");
            StartPlayingLocalFile (FileName);
            pInputFPPRemotePlayItem->SetPlayedFileCount (++FilePlayCount);
        }
        else
        {
            // Very noisy message
            // logcon("No file to play");
            break;
        }

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlayingLocalFile (String& FileName)
{
    // DEBUG_START;
    // DEBUG_V(String("FileName: ") + FileName);

    do // once
    {
        // are we playing anything?
        if(pInputFPPRemotePlayItem)
        {
            // DEBUG_V(String("Playing FileName: ") + pInputFPPRemotePlayItem->GetFileName());
            // are we playing this file already?
            if(FileName.equals(pInputFPPRemotePlayItem->GetFileName()))
            {
                // DEBUG_V("dont change anything.");
                if(pInputFPPRemotePlayItem->IsIdle())
                {
                    // DEBUG_V("Restart the idle channel");
                    pInputFPPRemotePlayItem->Start(FileName, 0, 1);
                }
                break;
            }
        }

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
                DeAllocatePlayer();
            }
            // DEBUG_V ("Start a new Local File");
            AllocatePlayer(c_InputFPPRemotePlayList, GetInputChannelId ());
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
            SafeStrncpy(StatusType, String(F("PlayList")).c_str(), sizeof(StatusType));
        }
        else
        {
            int Last_dot_fseq_Position = FileName.lastIndexOf(CN_Dotfseq);
            String Last_fseq_Text = FileName.substring(Last_dot_fseq_Position);
            if (String(CN_Dotfseq) != Last_fseq_Text)
            {
                logcon(String(F("File Name does not end with a valid .fseq or .pl extension: '")) + FileName + "'");
                SafeStrncpy(StatusType, String(F("Invalid File Name")).c_str(), sizeof(StatusType));
                memset(FileBeingPlayed, 0x0, sizeof(FileBeingPlayed));
                break;
            }

            if(pInputFPPRemotePlayItem)
            {
                // DEBUG_V ("Delete existing item");
                DeAllocatePlayer();
                // pInputFPPRemotePlayItem = nullptr;
                // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
            }
            // DEBUG_V ("Start Local FSEQ file player");
            AllocatePlayer(c_InputFPPRemotePlayFile, GetInputChannelId ());
            SafeStrncpy(StatusType, CN_File, sizeof(StatusType));
            // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        }

        // DEBUG_V (String ("Start Playing FileName: '") + FileName + "'");
        pInputFPPRemotePlayItem->ClearFileNames();
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);
        pInputFPPRemotePlayItem->Start (FileName, 0, 1);
        SafeStrncpy(FileBeingPlayed, FileName.c_str(), sizeof(FileBeingPlayed));
    } while (false);

    // DEBUG_END;

} // StartPlayingLocalFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::StartPlayingRemoteFile (String& FileName, time_t ElapsedSeconds)
{
    // DEBUG_START;
    // DEBUG_V(String("FileName: ") + FileName);

    do // once
    {
        if (PlayingRemoteFile ())
        {
            // DEBUG_V ("Already Playing in remote mode");
            break;
        }

        if(PlayingFile())
        {
            // DEBUG_V("StopPlaying");
            StopPlaying ();
        }

        // DEBUG_V ("Instantiate a new Remote FSEQ file player");
        if(pInputFPPRemotePlayItem)
        {
            // DEBUG_V ("Delete existing play item");
            DeAllocatePlayer();
        }
        // DEBUG_V ("Start Remote FSEQ file player");
        AllocatePlayer(c_InputFPPRemotePlayFile, GetInputChannelId ());
        // DEBUG_V(String("pInputFPPRemotePlayItem: 0x") + String(uint32_t(pInputFPPRemotePlayItem), HEX));
        pInputFPPRemotePlayItem->SetSyncOffsetMS (SyncOffsetMS);
        pInputFPPRemotePlayItem->SetSendFppSync (SendFppSync);

        String Background = String(ConfiguredFileToPlay).equals(CN_No_LocalFileToPlay) ? emptyString : String(ConfiguredFileToPlay);
        pInputFPPRemotePlayItem->SetBackgroundFileName(Background);

        SafeStrncpy(StatusType, CN_File, sizeof(StatusType));
        SafeStrncpy(FileBeingPlayed, FileName.c_str(), sizeof(FileBeingPlayed));

        FPPDiscovery.SetInputFPPRemotePlayFile (this);
        // DEBUG_V("Enable FPP Discovery");
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
    //xDEBUG_START;

    bool response = false;

    do // once
    {
        if (!PlayingFile ())
        {
            //xDEBUG_V("Not Playing a file of any kind.");
            break;
        }

        //xDEBUG_V(String("     FppSyncOverride: ") + String(FppSyncOverride));
        //xDEBUG_V(String("ConfiguredFileToPlay: '") + ConfiguredFileToPlay + "'");
        if (!FppSyncOverride && !String(ConfiguredFileToPlay).equals(CN_No_LocalFileToPlay))
        {
            //xDEBUG_V("Cant Play a remote file. Do Nothing");
            break;
        }

        //xDEBUG_V("Playing a remote file");
        response = true;

    } while (false);

    //xDEBUG_END;
    return response;

} // PlayingRemoteFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::FppStopRemoteFilePlay ()
{
    // DEBUG_START;

    if(PlayingFile())
    {
        FilePlayCount = pInputFPPRemotePlayItem->GetPlayedFileCount();
    }

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
        SafeDelay(5);
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
        // DEBUG_V ("Ask FSM for status");
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
        // DEBUG_V ("Ask FSM if idle");
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
        (String(ConfiguredFileToPlay).equals(CN_No_LocalFileToPlay) || FppSyncOverride))
    {
        // DEBUG_V ("FPP Is allowed to control playing files");
        Response = true;
    }

    // DEBUG_END;
    return Response;
} // AllowedToPlayRemoteFile

//-----------------------------------------------------------------------------
void c_InputFPPRemote::SetBlankTimerIsRunning (bool BlankTimerRunning)
{
    // DEBUG_START;

    c_InputCommon::SetBlankTimerIsRunning(BlankTimerRunning);
    if(BlankTimerRunning)
    {
        StopPlaying();
    }

    // DEBUG_END;
} // SetBlankTimerIsRunning
