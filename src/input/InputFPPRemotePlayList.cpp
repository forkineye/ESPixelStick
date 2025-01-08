/*
* InputFPPRemotePlayList.cpp
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
*   Playlist object used to parse and play a playlist
*/

#include "input/InputFPPRemotePlayList.hpp"
#include "service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::c_InputFPPRemotePlayList (c_InputMgr::e_InputChannelIds InputChannelId) :
    c_InputFPPRemotePlayItem(InputChannelId)
{
    // DEBUG_START;

    fsm_PlayList_state_WaitForStart_imp.Init (this);

    // DEBUG_END;
} // c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::~c_InputFPPRemotePlayList ()
{
    // DEBUG_START;

    Stop ();

    // DEBUG_END;

} // ~c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Start (String & FileName, float ElapsedSeconds, uint32_t PlayCount)
{
    // DEBUG_START;

    if(!InputIsPaused())
    {
        pCurrentFsmState->Start (FileName, ElapsedSeconds, PlayCount);
    }

    // DEBUG_END;

} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Stop ()
{
    // DEBUG_START;

    if(!InputIsPaused())
    {
        pCurrentFsmState->Stop ();
    }

    // DEBUG_END;

} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Sync (String & FileName, float ElapsedSeconds)
{
    // DEBUG_START;

    if(!InputIsPaused())
    {
        pCurrentFsmState->Sync (FileName, ElapsedSeconds);
    }

    // DEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayList::Poll ()
{
    // DEBUG_START;
    bool Response = false;
    if(!InputIsPaused())
    {
        // Show that we have received a poll
        Response = pCurrentFsmState->Poll ();
    }

    // xDEBUG_END;
    return Response;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonWrite(jsonStatus, CN_name,  GetFileName ());
    JsonWrite(jsonStatus, CN_entry, PlayListEntryId);
    JsonWrite(jsonStatus, CN_count, PlayListRepeatCount);

    pCurrentFsmState->GetStatus (jsonStatus);

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayList::ProcessPlayListEntry ()
{
    // DEBUG_START;
    bool response = false;

    JsonDocument JsonPlayListDoc;

    do // once
    {
        // DEBUG_V ("");
        uint32_t FrameId = 0;
        uint32_t PlayCount = 1;
        PauseDelayTimer.StartTimer(10000, false);

        // Get the playlist file
        String FileData;
        if (0 == FileMgr.ReadSdFile (FileControl[CurrentFile].FileName, FileData))
        {
            logcon (String (F ("Could not read Playlist file: '")) + FileControl[CurrentFile].FileName + "'");
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (FileControl[CurrentFile].FileName, PauseDelayTimer.GetTimeRemaining() / 1000, 1);
            break;
        }
        // DEBUG_V ("");

        DeserializationError error = deserializeJson ((JsonPlayListDoc), (const String)FileData);

        // DEBUG_V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (F ("SD file: '")) + FileControl[CurrentFile].FileName + "' ";
            logcon (CN_Heap_colon + String (ESP.getFreeHeap ()));
            logcon (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            logcon (String (F ("++++")) + FileData + String (F ("----")));
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (FileControl[CurrentFile].FileName, PauseDelayTimer.GetTimeRemaining() / 1000, PlayCount);
            break;
        }

        JsonArray JsonPlayListArray = JsonPlayListDoc.as<JsonArray> ();
        // PrettyPrint (JsonPlayListArray, String ("PlayList Array"));

        if (PlayListEntryId >= JsonPlayListDoc.size ())
        {
            // DEBUG_V ("No more entries to play. Start over");
            PlayListRepeatCount++;
            PlayListEntryId = 0;
        }

        // DEBUG_V (String ("            PlayListEntryId: '") + String(PlayListEntryId) + "'");
        JsonObject JsonPlayListArrayEntry = JsonPlayListArray[PlayListEntryId];
        // PrettyPrint (JsonPlayListArrayEntry, String ("PlayList Array Entry"));

        // DEBUG_V (String ("       JsonPlayListDoc:size: '") + String (JsonPlayListDoc.size ()) + "'");
        // DEBUG_V (String ("JsonPlayListArrayEntry:size: '") + String (JsonPlayListArrayEntry.size ()) + "'");
        if ((0 == JsonPlayListArrayEntry.size ()))
        {
            // DEBUG_V ("Entry is empty. Do a Pause");

            PauseDelayTimer.StartTimer(1000, false);
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (FileControl[CurrentFile].FileName, PauseDelayTimer.GetTimeRemaining() / 1000, PlayCount);
            break;
        }

        // next time process the next entry
        ++PlayListEntryId;
        // DEBUG_V (String ("            PlayListEntryId: '") + String (PlayListEntryId) + "'");

        String PlayListEntryType;
        setFromJSON (PlayListEntryType, JsonPlayListArrayEntry, CN_type);

        // DEBUG_V (String("PlayListEntryType: '") + PlayListEntryType + "'");

        String PlayListEntryName;
        setFromJSON (PlayListEntryName, JsonPlayListArrayEntry, CN_name);

        if (String(CN_file) == PlayListEntryType)
        {
            FrameId = 1;
            PlayCount = 1;
            setFromJSON (PlayCount, JsonPlayListArrayEntry, CN_playcount);
            // DEBUG_V (String ("PlayListEntryPlayCount: '") + String (FrameId) + "'");

            fsm_PlayList_state_PlayingFile_imp.Init (this);
        }

        else if (String (CN_effect) == PlayListEntryType)
        {
            JsonObject EffectConfig = JsonPlayListArrayEntry[(char*)CN_config].to<JsonObject>();
            serializeJson (EffectConfig, PlayListEntryName);

            FrameId = 10;
            setFromJSON (FrameId, JsonPlayListArrayEntry, CN_duration);

            fsm_PlayList_state_PlayingEffect_imp.Init (this);
        }

        else if (String (F ("pause")) == PlayListEntryType)
        {
            uint32_t PlayListEntryDuration = 0;
            setFromJSON (PlayListEntryDuration, JsonPlayListArrayEntry, CN_duration);
            // DEBUG_V (String ("PlayListEntryDuration: '") + String (PlayListEntryDuration) + "'");
            PauseDelayTimer.StartTimer(PlayListEntryDuration * 1000, false);
            // DEBUG_V (String ("         PauseEndTime: '") + String (PauseEndTime) + "'");

            fsm_PlayList_state_Paused_imp.Init (this);
            response = true;
        }

        else
        {
            logcon (String (F ("Unsupported Play List Entry type: '")) + PlayListEntryType + "'");
            PauseDelayTimer.StartTimer(10000, false);
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (PlayListEntryName, FrameId, PlayCount);
            break;
        }

        // DEBUG_V (String ("PlayListEntryName: '") + String (PlayListEntryName) + "'");
        pCurrentFsmState->Start (PlayListEntryName, FrameId, PlayCount);

        response = true;

    } while (false);

    // DEBUG_END;

    return response;

} // ProcessPlayListEntry
