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
#include "InputMgr.hpp"
#include "../service/fseq.h"

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
    // DEBUG_START;

    // DEBUG_V (String (" Parent: '") + String ((uint32_t)Parent) + "'");
    p_InputFPPRemotePlayFile = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Idle_imp);

    p_InputFPPRemotePlayFile->PlayItemName       = String ("");
    p_InputFPPRemotePlayFile->LastPlayedFrameId  = 0;
    p_InputFPPRemotePlayFile->RemainingPlayCount = 0;

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: ") + FileName);

    p_InputFPPRemotePlayFile->PlayItemName       = FileName;
    p_InputFPPRemotePlayFile->LastPlayedFrameId  = FrameId;
    p_InputFPPRemotePlayFile->RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_InputFPPRemotePlayFile->PlayItemName);
    // DEBUG_V (String ("            FrameId: ") + p_InputFPPRemotePlayFile->LastPlayedFrameId);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

    p_InputFPPRemotePlayFile->fsm_PlayFile_state_Starting_imp.Init (p_InputFPPRemotePlayFile);

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
bool fsm_PlayFile_state_Idle::Sync (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    Start (FileName, FrameId, 1);

    // DEBUG_END;

    return false;

} // fsm_PlayFile_state_Idle::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    p_InputFPPRemotePlayFile->fsm_PlayFile_state_PlayingFile_imp.Init (p_InputFPPRemotePlayFile);
    // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    p_InputFPPRemotePlayFile = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Starting_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: ") + FileName);
    p_InputFPPRemotePlayFile->PlayItemName       = FileName;
    p_InputFPPRemotePlayFile->LastPlayedFrameId  = FrameId;
    p_InputFPPRemotePlayFile->RemainingPlayCount = RemainingPlayCount;
    // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Stop (void)
{
    // DEBUG_START;

    p_InputFPPRemotePlayFile->PlayItemName       = String("");
    p_InputFPPRemotePlayFile->LastPlayedFrameId  = 0;
    p_InputFPPRemotePlayFile->RemainingPlayCount = 0;

    p_InputFPPRemotePlayFile->fsm_PlayFile_state_Idle_imp.Init (p_InputFPPRemotePlayFile);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Starting::Sync (String& FileName, uint32_t FrameId)
{
    // DEBUG_START;
    bool response = false;

    Start (FileName, FrameId, 1);

    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_Starting::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Poll (uint8_t* Buffer, size_t BufferSize)
{
    // DEBUG_START;

    do // once
    {
        time_t now = millis ();
        int32_t SyncOffset = p_InputFPPRemotePlayFile->GetSyncOffsetMS ();
        uint32_t CurrentFrame = p_InputFPPRemotePlayFile->CalculateFrameId (now, SyncOffset);

        // have we reached the end of the file?
        if (p_InputFPPRemotePlayFile->TotalNumberOfFramesInSequence <= CurrentFrame)
        {
            // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);
            if (0 != p_InputFPPRemotePlayFile->RemainingPlayCount)
            {
                // DEBUG_V (String ("RemainingPlayCount: ") + String (p_InputFPPRemotePlayFile->RemainingPlayCount));
                logcon (String ("Replaying:: FileName:      '") + p_InputFPPRemotePlayFile->GetFileName () + "'");
                --p_InputFPPRemotePlayFile->RemainingPlayCount;
                // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

                p_InputFPPRemotePlayFile->StartTimeMS = now;
                p_InputFPPRemotePlayFile->LastPlayedFrameId = -1;
                CurrentFrame = 0;
            }
            else
            {
                // DEBUG_V (String ("CurrentFrame: ") + String(CurrentFrame));
                // DEBUG_V (String ("Done Playing:: FileName:  '") + p_InputFPPRemotePlayFile->GetFileName () + "'");
                Stop ();
                break;
            }
        }

        if (CurrentFrame == p_InputFPPRemotePlayFile->LastPlayedFrameId)
        {
            // DEBUG_V (String ("keep waiting"));
            break;
        }

        uint32_t FilePosition    = p_InputFPPRemotePlayFile->DataOffset + (p_InputFPPRemotePlayFile->ChannelsPerFrame * CurrentFrame);
        size_t   MaxBytesToRead  = (p_InputFPPRemotePlayFile->ChannelsPerFrame > BufferSize) ? BufferSize : p_InputFPPRemotePlayFile->ChannelsPerFrame;
        byte* CurrentDestination = Buffer;
        // DEBUG_V (String ("               MaxBytesToRead: ") + String (MaxBytesToRead));

        p_InputFPPRemotePlayFile->LastPlayedFrameId = CurrentFrame;

        for (auto & CurrentSparseRange : p_InputFPPRemotePlayFile->SparseRanges)
        {
            size_t ActualBytesToRead = min (MaxBytesToRead, CurrentSparseRange.ChannelCount);
            if (0 == ActualBytesToRead)
            {
                // no data to read for this range
                continue;
            }

            uint32_t AdjustedFilePosition = FilePosition + CurrentSparseRange.DataOffset;

            // DEBUG_V (String ("                 FilePosition: ") + String (FilePosition));
            // DEBUG_V (String ("         AdjustedFilePosition: ") + String (uint32_t(AdjustedFilePosition), HEX));
            // DEBUG_V (String ("           CurrentDestination: ") + String (uint32_t(CurrentDestination), HEX));
            // DEBUG_V (String ("            ActualBytesToRead: ") + String (ActualBytesToRead));
            size_t ActualBytesRead = FileMgr.ReadSdFile (p_InputFPPRemotePlayFile->FileHandleForFileBeingPlayed,
                                                         CurrentDestination,
                                                         ActualBytesToRead,
                                                         AdjustedFilePosition);
            MaxBytesToRead -= ActualBytesRead;
            CurrentDestination += ActualBytesRead;

            if (ActualBytesRead != ActualBytesToRead)
            {
                // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_InputFPPRemotePlayFile->TotalNumberOfFramesInSequence));
                // DEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));

                if (0 != p_InputFPPRemotePlayFile->FileHandleForFileBeingPlayed)
                {
                    logcon (F ("File Playback Failed to read enough data"));
                    Stop ();
                }
            }
        }

        // DEBUG_V (String ("      DataOffset: ") + String (p_InputFPPRemotePlayFile->DataOffset));
        // DEBUG_V (String ("      BufferSize: ") + String (BufferSize));
        // DEBUG_V (String ("ChannelsPerFrame: ") + String (p_InputFPPRemotePlayFile->ChannelsPerFrame));
        // DEBUG_V (String ("    FilePosition: ") + String (FilePosition));
        // DEBUG_V (String ("  MaxBytesToRead: ") + String (MaxBytesToRead));

        InputMgr.RestartBlankTimer (p_InputFPPRemotePlayFile->GetInputChannelId ());

    } while (false);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    p_InputFPPRemotePlayFile = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_PlayingFile_imp);

    do // once
    {
        // DEBUG_V (String ("FileName: '") + p_InputFPPRemotePlayFile->PlayItemName + "'");
        // DEBUG_V (String (" FrameId: '") + p_InputFPPRemotePlayFile->LastPlayedFrameId + "'");
        // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);
        if (0 == p_InputFPPRemotePlayFile->RemainingPlayCount)
        {
            Stop ();
            break;
        }

        --p_InputFPPRemotePlayFile->RemainingPlayCount;
        // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

        if (!p_InputFPPRemotePlayFile->ParseFseqFile ())
        {
            Stop ();
            break;
        }

        p_InputFPPRemotePlayFile->LastPlayedFrameId = 0;
        p_InputFPPRemotePlayFile->CalculatePlayStartTime ();

        // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_InputFPPRemotePlayFile->LastPlayedFrameId));
        // DEBUG_V (String ("                  StartTimeMS: ") + String (p_InputFPPRemotePlayFile->StartTimeMS));
        // DEBUG_V (String ("           RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

        logcon (String (F ("Start Playing:: FileName: '")) + p_InputFPPRemotePlayFile->PlayItemName + "'");

        Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_PlayingFile_imp);

    } while (false);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Start (String& FileName, uint32_t FrameId, uint32_t PlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("                     FileName: ") + FileName);
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_InputFPPRemotePlayFile->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_InputFPPRemotePlayFile->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

    if (FileName == p_InputFPPRemotePlayFile->GetFileName ())
    {
        // Keep playing the same file
        // p_InputFPPRemotePlayFile->LastPlayedFrameId   = FrameId;
        // p_InputFPPRemotePlayFile->LastRcvdSyncFrameId = FrameId;
        // p_InputFPPRemotePlayFile->RemainingPlayCount  = PlayCount - 1;
        // p_InputFPPRemotePlayFile->CalculatePlayStartTime ();
    }
    else
    {
        Stop ();
        p_InputFPPRemotePlayFile->Start (FileName, FrameId, PlayCount);
    }
    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Stop (void)
{
    // DEBUG_START;

    logcon (String (F ("Stop Playing::  FileName: '")) + p_InputFPPRemotePlayFile->PlayItemName + "'");
    // DEBUG_V (String (" FileHandleForFileBeingPlayed: ") + String (FileHandleForFileBeingPlayed));
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_InputFPPRemotePlayFile->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_InputFPPRemotePlayFile->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("RemainingPlayCount: ") + p_InputFPPRemotePlayFile->RemainingPlayCount);

    p_InputFPPRemotePlayFile->fsm_PlayFile_state_Stopping_imp.Init (p_InputFPPRemotePlayFile);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_PlayingFile::Sync (String& FileName, uint32_t TargetFrameId)
{
    // DEBUG_START;
    bool response = false;

    do // once
    {
        // are we on the correct file?
        if (FileName != p_InputFPPRemotePlayFile->GetFileName ())
        {
            // DEBUG_V ("Sync: Filename change");
            Stop ();
            p_InputFPPRemotePlayFile->Start (FileName, TargetFrameId, 1);
            break;
        }

        if (p_InputFPPRemotePlayFile->LastRcvdSyncFrameId >= TargetFrameId)
        {
            // DEBUG_V ("Duplicate or older sync msg");
            break;
        }
        p_InputFPPRemotePlayFile->LastRcvdSyncFrameId = TargetFrameId;

        time_t now = millis ();

        uint32_t CurrentFrame = p_InputFPPRemotePlayFile->CalculateFrameId (now);
        uint32_t FrameDiff = CurrentFrame - TargetFrameId;

        if (CurrentFrame < TargetFrameId)
        {
            FrameDiff = TargetFrameId - CurrentFrame;
        }

        // DEBUG_V (String ("     CurrentFrame: ") + String (CurrentFrame));
        // DEBUG_V (String ("    TargetFrameId: ") + String (TargetFrameId));
        // DEBUG_V (String ("        FrameDiff: ") + String (FrameDiff));

        if (2 > FrameDiff)
        {
            // DEBUG_V ("No Need to adjust the time");
            break;
        }

        // DEBUG_V ("Need to adjust the start time");
        // DEBUG_V (String ("StartTimeMS: ") + String (p_InputFPPRemotePlayFile->StartTimeMS));

        if (CurrentFrame > TargetFrameId)
        {
            p_InputFPPRemotePlayFile->TimeCorrectionFactor += TimeOffsetStep;
        }
        else
        {
            p_InputFPPRemotePlayFile->TimeCorrectionFactor -= TimeOffsetStep;
        }

        // p_InputFPPRemotePlayFile->CalculatePlayStartTime ();

        response = true;
        // DEBUG_V (String ("StartTimeMS: ") + String (p_InputFPPRemotePlayFile->StartTimeMS));

    } while (false);

    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_PlayingFile::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Poll (uint8_t* Buffer, size_t BufferSize)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (p_InputFPPRemotePlayFile->FileHandleForFileBeingPlayed));

    FileMgr.CloseSdFile (p_InputFPPRemotePlayFile->FileHandleForFileBeingPlayed);
    p_InputFPPRemotePlayFile->FileHandleForFileBeingPlayed = 0;
    p_InputFPPRemotePlayFile->PlayItemName = String ("");

    p_InputFPPRemotePlayFile->fsm_PlayFile_state_Idle_imp.Init (p_InputFPPRemotePlayFile);

    if (FileName != "")
    {
        p_InputFPPRemotePlayFile->Start (FileName, FrameId, PlayCount);
    }

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    FileName  = "";
    FrameId   = 0;
    PlayCount = 0;

    p_InputFPPRemotePlayFile = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Stopping_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Start (String& _FileName, uint32_t _FrameId, uint32_t _PlayCount)
{
    // DEBUG_START;

    FileName  = _FileName;
    FrameId   = _FrameId;
    PlayCount = _PlayCount;

    // DEBUG_END

} // fsm_PlayFile_state_Stopping::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Stop (void)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Stopping::Sync (String& FileName, uint32_t TargetFrameId)
{
    // DEBUG_START;
    bool response = false;


    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_Stopping::Sync
