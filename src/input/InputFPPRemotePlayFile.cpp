/*
* InputFPPRemotePlayFile.cpp
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
*   PlayFile object used to parse and play an FSEQ File
*/

#include "InputFPPRemotePlayFile.hpp"
#include "../service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::c_InputFPPRemotePlayFile () :
    c_InputFPPRemotePlayItem ()
{
    // DEBUG_START;

    fsm_PlayFile_state_Idle_imp.Init (this);

    // DEBUG_END;
} // c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::~c_InputFPPRemotePlayFile ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();

    // DEBUG_END;

} // ~c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, FrameId);

    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Stop ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();
    PlayItemName = String ("");

    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    SyncCount++;
    SyncAdjustmentCount += pCurrentFsmState->Sync (FrameId);

    // DEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pCurrentFsmState->Poll (Buffer, BufferSize);

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::GetStatus (JsonObject& JsonStatus)
{
    // DEBUG_START;

    uint32_t mseconds = CurrentFrameId * FrameStepTime;
    uint32_t msecondsTotal = FrameStepTime * TotalNumberOfFramesInSequence;

    uint32_t secs = mseconds / 1000;
    uint32_t secsTot = msecondsTotal / 1000;

    JsonStatus[F ("SyncCount")]           = SyncCount;
    JsonStatus[F ("SyncAdjustmentCount")] = SyncAdjustmentCount;

    String temp = GetFileName ();

    JsonStatus[F ("current_sequence")]  = temp;
    JsonStatus[F ("playlist")]          = temp;
    JsonStatus[F ("seconds_elapsed")]   = String (secs);
    JsonStatus[F ("seconds_played")]    = String (secs);
    JsonStatus[F ("seconds_remaining")] = String (secsTot - secs);
    JsonStatus[F ("sequence_filename")] = temp;


    int mins = secs / 60;
    secs = secs % 60;

    secsTot = secsTot - secs;
    int minRem = secsTot / 60;
    secsTot = secsTot % 60;

    char buf[8];
    sprintf (buf, "%02d:%02d", mins, secs);
    JsonStatus[F ("time_elapsed")] = buf;

    sprintf (buf, "%02d:%02d", minRem, secsTot);
    JsonStatus[F ("time_remaining")] = buf;

    // DEBUG_END;

} // GetStatus
