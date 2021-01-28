/*
* InputFPPRemotePlayListFsm.cpp
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
#include "../FileMgr.hpp"

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayList_state_Idle::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_Idle_imp);

    // DEBUG_END;

} // fsm_PlayList_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::Start (String & FileName, uint32_t )
{
    // DEBUG_START;

    do // once
    {
        pInputFPPRemotePlayList->PlayItemName = FileName;
        pInputFPPRemotePlayList->PlayListEntryId = 0;

        if (!pInputFPPRemotePlayList->ProcessPlayListEntry ())
        {
            LOG_PORT.println (String (F ("Could not start PlayList: '")) + FileName + "'");
            break;
        }
    
        // DEBUG_V (String ("PlayItemName: '") + pInputFPPRemotePlayList->PlayItemName + "'");

    } while (false);

    // DEBUG_END;

} // fsm_PlayList_state_Idle::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::Stop (void)
{
    // DEBUG_START;

    // Do nothing

    // DEBUG_END;

} // fsm_PlayList_state_Idle::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject FileStatus = jsonStatus.createNestedObject (F ("Idle"));

    // DEBUG_END;

} // fsm_PlayList_state_Idle::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Poll (Buffer, BufferSize);
    
    if (pInputFPPRemotePlayList->pInputFPPRemotePlayItem->IsIdle ())
    {
        // DEBUG_V ("Idle Processing");
        if (false == pInputFPPRemotePlayList->ProcessPlayListEntry ())
        {
            // DEBUG_V ("Done with all entries");
            Stop ();
        }
    }

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingFile_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    // ignore

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Stop (void)
{
    // DEBUG_START;

    if (nullptr != pInputFPPRemotePlayList->pInputFPPRemotePlayItem)
    {
        // DEBUG_V ("");
        pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Stop ();
        delete pInputFPPRemotePlayList->pInputFPPRemotePlayItem;
    }
    // DEBUG_V ("");
    pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    jsonStatus[F ("repeat")] = pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetRepeatCount ();

    JsonObject FileStatus = jsonStatus.createNestedObject (F("File"));
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetStatus (FileStatus);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Poll (Buffer, BufferSize);

    if (pInputFPPRemotePlayList->pInputFPPRemotePlayItem->IsIdle ())
    {
        // DEBUG_V ("Idle Processing");
        if (false == pInputFPPRemotePlayList->ProcessPlayListEntry ())
        {
            // DEBUG_V ("Done with all entries");
            Stop ();
        }
        // DEBUG_V ("Idle Processing Done");
    }

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingEffect_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    // ignore

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Stop (void)
{
    // DEBUG_START;

    if (nullptr != pInputFPPRemotePlayList->pInputFPPRemotePlayItem)
    {
        // DEBUG_V ("");
        pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Stop ();
        delete pInputFPPRemotePlayList->pInputFPPRemotePlayItem;
    }
    // DEBUG_V ("");
    pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject EffectStatus = jsonStatus.createNestedObject (F ("Effect"));
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetStatus (EffectStatus);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayList_state_Paused::Poll (uint8_t* Buffer, size_t BufferSize)
{
    // DEBUG_START;

    if (pInputFPPRemotePlayList->PauseEndTime <= millis ())
    {
        // DEBUG_V ("");
        if (false == pInputFPPRemotePlayList->ProcessPlayListEntry ())
        {
            // DEBUG_V ("Done with all entries");
            Stop ();
        }
    }

    // DEBUG_END;

} // fsm_PlayList_state_Paused::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Paused::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_Paused_imp);

    // DEBUG_END;

} // fsm_PlayList_state_Paused::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Paused::Start (String& FileName, uint32_t FrameId)
{
    // DEBUG_START;

    // ignore

    // DEBUG_END;

} // fsm_PlayList_state_Paused::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Paused::Stop (void)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);

    // DEBUG_END;

} // fsm_PlayList_state_Paused::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_Paused::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject FileStatus = jsonStatus.createNestedObject (F ("Paused"));

    // DEBUG_END;

} // fsm_PlayList_state_Paused::GetStatus
