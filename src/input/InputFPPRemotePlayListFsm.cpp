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
#include "InputFPPRemotePlayEffect.hpp"

//-----------------------------------------------------------------------------
void fsm_PlayList_state_WaitForStart::Poll (uint8_t* Buffer, size_t BufferSize)
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayList_state_Idle::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_WaitForStart::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_WaitForStart_imp);

    // DEBUG_END;

} // fsm_PlayList_state_WaitForStart::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_WaitForStart::Start (String& FileName, uint32_t)
{
    // DEBUG_START;

    do // once
    {
        pInputFPPRemotePlayList->PlayItemName = FileName;
        pInputFPPRemotePlayList->PlayListEntryId = 0;

        // DEBUG_V (String ("PlayItemName: '") + pInputFPPRemotePlayList->PlayItemName + "'");

        pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);
    
    } while (false);

    // DEBUG_END;

} // fsm_PlayList_state_WaitForStart::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_WaitForStart::Stop (void)
{
    // DEBUG_START;

    // Do nothing

    // DEBUG_END;

} // fsm_PlayList_state_WaitForStart::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_WaitForStart::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject FileStatus = jsonStatus.createNestedObject (CN_Idle);

    // DEBUG_END;

} // fsm_PlayList_state_WaitForStart::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayList_state_Idle::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->ProcessPlayListEntry ();

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

    // do nothing

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

    JsonObject FileStatus = jsonStatus.createNestedObject (CN_Idle);

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
        // DEBUG_V ("Done with all entries");
        Stop ();
    }

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    Parent->pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile ();

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingFile_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->SetPlayCount (FrameId);
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Start (FileName, 0);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Stop (void)
{
    // DEBUG_START;

    c_InputFPPRemotePlayItem * pInputFPPRemotePlayItemTemp = pInputFPPRemotePlayList->pInputFPPRemotePlayItem;

    // This redirects async requests to a safe place.
    pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);
    // DEBUG_V ("");
    
    pInputFPPRemotePlayItemTemp->Stop ();
    delete pInputFPPRemotePlayItemTemp;
    
    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    jsonStatus[F ("repeat")] = pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetRepeatCount ();

    JsonObject FileStatus = jsonStatus.createNestedObject (CN_File);
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
        // DEBUG_V ("Effect Processing Done");
        Stop ();
    }

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    Parent->pInputFPPRemotePlayItem = new c_InputFPPRemotePlayEffect ();

    Parent->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingEffect_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: '") + String (FileName) + "'");
    // DEBUG_V (String ("FrameId: '") + String (FrameId) + "'");

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->SetDuration (FrameId);
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Start (FileName, 0);
    
    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Start

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Stop (void)
{
    // DEBUG_START;

    c_InputFPPRemotePlayItem* pInputFPPRemotePlayItemTemp = pInputFPPRemotePlayList->pInputFPPRemotePlayItem;

    // This redirects async requests to a safe place.
    pInputFPPRemotePlayList->fsm_PlayList_state_Idle_imp.Init (pInputFPPRemotePlayList);
    // DEBUG_V ("");

    pInputFPPRemotePlayItemTemp->Stop ();
    delete pInputFPPRemotePlayItemTemp;

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Stop

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject EffectStatus = jsonStatus.createNestedObject (CN_Effect);
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
        Stop();
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

    JsonObject PauseStatus = jsonStatus.createNestedObject (CN_Paused);
    
    time_t now = millis ();

    time_t SecondsRemaining = (pInputFPPRemotePlayList->PauseEndTime - now) / 1000;
    if (now > pInputFPPRemotePlayList->PauseEndTime)
    {
        SecondsRemaining = 0;
    }

    time_t MinutesRemaining = min(time_t(99), time_t(SecondsRemaining / 60));
    SecondsRemaining = SecondsRemaining % 60;

    char buf[10];
    sprintf (buf, "%02d:%02d", MinutesRemaining, SecondsRemaining);
    PauseStatus[F ("TimeRemaining")] = buf;

    // DEBUG_END;

} // fsm_PlayList_state_Paused::GetStatus
