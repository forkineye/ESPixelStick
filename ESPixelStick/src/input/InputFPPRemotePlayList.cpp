/*
* InputFPPRemotePlayList.cpp
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
*   Playlist object used to parse and play a playlist
*/

#include "InputFPPRemotePlayList.hpp"
#include "../service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::c_InputFPPRemotePlayList () :
    c_InputFPPRemotePlayItem()
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
void c_InputFPPRemotePlayList::Start (String & FileName, uint32_t FrameId, uint32_t PlayCount)
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, FrameId, PlayCount);

    // DEBUG_END;

} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Stop ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();

    // DEBUG_END;

} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Sync (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Sync (FileName, FrameId);

    // DEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pCurrentFsmState->Poll (Buffer, BufferSize);

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    jsonStatus[CN_name]  = GetFileName ();
    jsonStatus[F ("entry")] = PlayListEntryId;
    jsonStatus[CN_count] = PlayListRepeatCount;

    pCurrentFsmState->GetStatus (jsonStatus);

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayList::ProcessPlayListEntry ()
{
    // DEBUG_START;
    bool response = false;

    DynamicJsonDocument JsonPlayListDoc (2048);

    extern void PrettyPrint (JsonArray & jsonStuff, String Name);
    extern void PrettyPrint (JsonObject & jsonStuff, String Name);

    do // once
    {
        // DEBUG_V ("");
        uint32_t FrameId = 0;
        uint32_t PlayCount = 1;
        PauseEndTime = millis () + 10000;

        // Get the playlist file
        String FileData;
        if (0 == FileMgr.ReadSdFile (PlayItemName, FileData))
        {
            NewLogToCon (String (F ("Could not read Playlist file: '")) + PlayItemName + "'");
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (PlayItemName, PauseEndTime, 1);
            break;
        }
        // DEBUG_V ("");

        DeserializationError error = deserializeJson ((JsonPlayListDoc), (const String)FileData);

        // DEBUG_V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (F ("SD file: '")) + PlayItemName + "' ";
            NewLogToCon (CN_Heap_colon + String (ESP.getFreeHeap ()));
            NewLogToCon (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            NewLogToCon (String (F ("++++")) + FileData + String (F ("----")));
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (PlayItemName, PauseEndTime, PlayCount);
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

            PauseEndTime = millis () + 1000;
            fsm_PlayList_state_Paused_imp.Init (this);
            pCurrentFsmState->Start (PlayItemName, PauseEndTime, PlayCount);
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
            setFromJSON (PlayCount, JsonPlayListArrayEntry, F ("playcount"));
            // DEBUG_V (String ("PlayListEntryPlayCount: '") + String (FrameId) + "'");

            fsm_PlayList_state_PlayingFile_imp.Init (this);
        }

        else if (String (CN_effect) == PlayListEntryType)
        {
            JsonObject EffectConfig = JsonPlayListArrayEntry[CN_config];
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
            PauseEndTime = (PlayListEntryDuration * 1000) + millis ();
            // DEBUG_V (String ("         PauseEndTime: '") + String (PauseEndTime) + "'");

            fsm_PlayList_state_Paused_imp.Init (this);
            response = true;
        }

        else
        {
            NewLogToCon (String (F ("Unsupported Play List Entry type: '")) + PlayListEntryType + "'");
            PauseEndTime = millis () + 10000;
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
