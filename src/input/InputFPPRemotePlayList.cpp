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

    fsm_PlayList_state_Idle_imp.Init (this);

    // DEBUG_END;
} // c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::~c_InputFPPRemotePlayList ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();

    // DEBUG_END;

} // ~c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, FrameId);

    // DEBUG_END;

} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Stop ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();
    fsm_PlayList_state_Idle_imp.Init (this);

    // DEBUG_END;

} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Sync (FrameId);

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

    // DEBUG_END;

} // GetStatus
