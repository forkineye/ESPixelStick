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
void fsm_PlayFile_state_Idle::Poll (uint8_t * Buffer, size_t BufferSize)
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
void fsm_PlayFile_state_Idle::Start (String & FileName, uint32_t FrameId)
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
bool fsm_PlayFile_state_Idle::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    // Do Nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Poll (uint8_t * Buffer, size_t BufferSize)
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
void fsm_PlayFile_state_PlayingFile::Start (String & FileName, uint32_t FrameId)
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
bool fsm_PlayFile_state_PlayingFile::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Sync
