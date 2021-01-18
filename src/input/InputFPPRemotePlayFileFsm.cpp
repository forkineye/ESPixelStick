/*
* InputFPPRemotePlayFileFsm.cpp
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
*   Fsm object used to parse and play an File
*/

#include "InputFPPRemotePlayFile.hpp"
#include "../service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Poll (void)
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Init (c_InputFPPRemotePlayFile* Parent)
{
    DEBUG_START;

    p_InputFPPRemotePlayFile = Parent;
    p_InputFPPRemotePlayFile->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Idle_imp);

    DEBUG_END;

} // fsm_PlayFile_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Start (void)
{
    // DEBUG_START;

    // open the Fsm file

    // set context to the first entry in the file

    // do the first item in the file

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Stop (void)
{
    // DEBUG_START;

    // Do nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Stop

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Sync (time_t syncTime)
{
    // DEBUG_START;

    // Do Nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Poll (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Start (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Stop (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Sync (time_t syncTime)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Paused::Poll (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Paused::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Paused::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Paused::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Paused::Start (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Paused::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Paused::Stop (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Paused::Stop

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Paused::Sync (time_t syncTime)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Paused::Sync

