/*
* InputFPPRemotePlayListFsm.cpp
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
#include "utility/SaferStringConversion.hpp"
#include "FileMgr.hpp"
#include "input/InputFPPRemotePlayEffect.hpp"
#include "input/InputFPPRemotePlayFile.hpp"

//-----------------------------------------------------------------------------
bool fsm_PlayList_state_WaitForStart::Poll ()
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;
    return false;

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
void fsm_PlayList_state_WaitForStart::Start (String & FileName, float, uint32_t)
{
    // DEBUG_START;

    do // once
    {
        pInputFPPRemotePlayList->FileControl[CurrentFile].FileName = FileName;
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

    // JsonObject FileStatus = jsonStatus[(char*)CN_Idle].to<JsonObject> ();

    // DEBUG_END;

} // fsm_PlayList_state_WaitForStart::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayList_state_Idle::Poll ()
{
    // DEBUG_START;

    pInputFPPRemotePlayList->ProcessPlayListEntry ();

    // DEBUG_END;
    return false;

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
void fsm_PlayList_state_Idle::Start (String &, float, uint32_t )
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

    // JsonObject FileStatus = jsonStatus[(char*)CN_Idle].to<JsonObject> ();

    // DEBUG_END;

} // fsm_PlayList_state_Idle::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayList_state_PlayingFile::Poll ()
{
    // xDEBUG_START;

    bool Response = pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Poll ();

    if (pInputFPPRemotePlayList->pInputFPPRemotePlayItem->IsIdle ())
    {
        // DEBUG_V ("Done with all entries");
        pInputFPPRemotePlayList->Stop ();
    }

    // xDEBUG_END;
    return Response;

} // fsm_PlayList_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    Parent->pInputFPPRemotePlayItem = new c_InputFPPRemotePlayFile (Parent->GetInputChannelId ());

    pInputFPPRemotePlayList = Parent;
    pInputFPPRemotePlayList->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingFile_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingFile::Start (String & FileName, float SecondsElapsed, uint32_t PlayCount)
{
    // DEBUG_START;

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Start (FileName, SecondsElapsed, PlayCount);

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

    JsonObject FileStatus = jsonStatus[(char*)CN_File].to<JsonObject> ();
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetStatus (FileStatus);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingFile::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayList_state_PlayingEffect::Poll ()
{
    // DEBUG_START;

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Poll ();

    if (pInputFPPRemotePlayList->pInputFPPRemotePlayItem->IsIdle ())
    {
        // DEBUG_V ("Effect Processing Done");
        pInputFPPRemotePlayList->Stop ();
    }

    // DEBUG_END;
    return false;

} // fsm_PlayList_state_PlayingEffect::Poll

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Init (c_InputFPPRemotePlayList* Parent)
{
    // DEBUG_START;

    pInputFPPRemotePlayList = Parent;
    Parent->pInputFPPRemotePlayItem = new c_InputFPPRemotePlayEffect (Parent->GetInputChannelId ());

    Parent->pCurrentFsmState = &(Parent->fsm_PlayList_state_PlayingEffect_imp);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::Init

//-----------------------------------------------------------------------------
void fsm_PlayList_state_PlayingEffect::Start (String & FileName, float SecondsElapsed, uint32_t PlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: '") + String (FileName) + "'");
    // DEBUG_V (String ("FrameId: '") + String (FrameId) + "'");

    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->SetDuration (SecondsElapsed);
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->Start (FileName, 0.0, PlayCount);
    
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

    JsonObject EffectStatus = jsonStatus[(char*)CN_Effect].to<JsonObject> ();
    pInputFPPRemotePlayList->pInputFPPRemotePlayItem->GetStatus (EffectStatus);

    // DEBUG_END;

} // fsm_PlayList_state_PlayingEffect::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayList_state_Paused::Poll ()
{
    // DEBUG_START;

    if (pInputFPPRemotePlayList->PauseDelayTimer.IsExpired())
    {
        // DEBUG_V();
        pInputFPPRemotePlayList->Stop();
    }

    // DEBUG_END;
    return false;

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
void fsm_PlayList_state_Paused::Start (String& , float , uint32_t )
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

    JsonObject PauseStatus = jsonStatus[(char*)CN_Paused].to<JsonObject> ();

    time_t SecondsRemaining = pInputFPPRemotePlayList->PauseDelayTimer.GetTimeRemaining() / 1000u;

    char buf[12];
    // BUGBUG -- casting time_t to integer types is not portable code (can be real ... e.g., float)
    // BUGBUG -- no portable way to know maximum value of time_t, or if it's signed vs. unsigned (without type traits)
    ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(buf, (uint32_t)SecondsRemaining));
    PauseStatus[F ("TimeRemaining")] = buf;

    // DEBUG_END;

} // fsm_PlayList_state_Paused::GetStatus
