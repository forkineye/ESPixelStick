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
c_InputFPPRemotePlayFile::c_InputFPPRemotePlayFile (c_InputMgr::e_InputChannelIds InputChannelId) :
    c_InputFPPRemotePlayItem (InputChannelId)
{
    // DEBUG_START;

    fsm_PlayFile_state_Idle_imp.Init (this);

    // DEBUG_END;
} // c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::~c_InputFPPRemotePlayFile ()
{
    // DEBUG_START;

    for (uint32_t LoopCount = 10000; (LoopCount != 0) && (!IsIdle ()); LoopCount--)
    {
        Stop ();
        Poll (nullptr, 0);
    }
    // DEBUG_END;

} // ~c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Start (String & FileName, uint32_t FrameId, uint32_t PlayCount)
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, FrameId, PlayCount);

    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Stop ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();

    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Sync (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    SyncCount++;
    if (pCurrentFsmState->Sync (FileName, FrameId))
    {
        SyncAdjustmentCount++;
    }

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

    uint32_t mseconds = LastPlayedFrameId * FrameStepTimeMS;
    uint32_t msecondsTotal = FrameStepTimeMS * TotalNumberOfFramesInSequence;

    uint32_t secs = mseconds / 1000;
    uint32_t secsTot = msecondsTotal / 1000;

    JsonStatus[F ("SyncCount")]           = SyncCount;
    JsonStatus[F ("SyncAdjustmentCount")] = SyncAdjustmentCount;
    JsonStatus[F ("TimeOffset")]          = TimeCorrectionFactor;

    String temp = GetFileName ();

    JsonStatus[CN_current_sequence]  = temp;
    JsonStatus[CN_playlist]          = temp;
    JsonStatus[CN_seconds_elapsed]   = String (secs);
    JsonStatus[CN_seconds_played]    = String (secs);
    JsonStatus[CN_seconds_remaining] = String (secsTot - secs);
    JsonStatus[CN_sequence_filename] = temp;

    int mins = secs / 60;
    secs = secs % 60;

    secsTot = secsTot - secs;
    int minRem = secsTot / 60;
    secsTot = secsTot % 60;

    char buf[8];
    sprintf (buf, "%02d:%02d", mins, secs);
    JsonStatus[CN_time_elapsed] = buf;

    sprintf (buf, "%02d:%02d", minRem, secsTot);
    JsonStatus[CN_time_remaining] = buf;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
uint32_t c_InputFPPRemotePlayFile::CalculateFrameId (time_t now, int32_t SyncOffsetMS)
{
    // DEBUG_START;

    time_t   CurrentPlayTime         = now - StartTimeMS;
    time_t   AdjustedPlayTime        = CurrentPlayTime + SyncOffsetMS;
    time_t   AdjustedFrameStepTimeMS = time_t (float (FrameStepTimeMS) * TimeCorrectionFactor);
    uint32_t CurrentFrameId          = AdjustedPlayTime / AdjustedFrameStepTimeMS;

    // DEBUG_END;

    return max (CurrentFrameId, LastPlayedFrameId);

} // CalculateFrameId

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::CalculatePlayStartTime ()
{
    // DEBUG_START;

    StartTimeMS = millis () - uint32_t ((float (FrameStepTimeMS) * TimeCorrectionFactor) * float (LastPlayedFrameId));

    // DEBUG_END;

} // CalculatePlayStartTime
