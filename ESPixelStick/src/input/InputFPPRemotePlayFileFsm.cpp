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

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Poll ()
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_Idle::TimerPoll ()
{
    // nothing to do

} // fsm_PlayFile_state_Idle::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    // DEBUG_V (String (" Parent: 0x") + String ((uint32_t)Parent, HEX));
    p_Parent = Parent;
    p_Parent->pCurrentFsmState = &(p_Parent->fsm_PlayFile_state_Idle_imp);
    p_Parent->ClearFileInfo ();

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Start (String & FileName, uint32_t StartingFrameId, uint32_t RemainingPlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: ") + FileName);

    p_Parent->PlayItemName                 = FileName;
    p_Parent->FrameControl.StartingFrameId = StartingFrameId;
    p_Parent->RemainingPlayCount           = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->PlayItemName);
    // DEBUG_V (String ("    StartingFrameId: ") + p_Parent->FrameControl.StartingFrameId);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Starting_imp.Init (p_Parent);

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
void fsm_PlayFile_state_Starting::Poll ()
{
    // DEBUG_START;

    p_Parent->fsm_PlayFile_state_PlayingFile_imp.Init (p_Parent);
    // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_Starting::TimerPoll ()
{
    // nothing to do

} // fsm_PlayFile_state_Starting::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Starting_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("FileName: ") + FileName);
    p_Parent->PlayItemName                 = FileName;
    p_Parent->FrameControl.StartingFrameId = FrameId;
    p_Parent->RemainingPlayCount           = RemainingPlayCount;
    // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Stop (void)
{
    // DEBUG_START;

    p_Parent->RemainingPlayCount = 0;
    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

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
void fsm_PlayFile_state_PlayingFile::Poll ()
{
    // xDEBUG_START;

    do // once
    {
        // have we reached the end of the file?
        if (p_Parent->FrameControl.TotalNumberOfFramesInSequence <= p_Parent->FrameControl.LastPlayedFrameId)
        {
            // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
            if (0 != p_Parent->RemainingPlayCount)
            {
                // DEBUG_V (String ("RemainingPlayCount: ") + String (p_Parent->RemainingPlayCount));
                logcon (String ("Replaying:: FileName:      '") + p_Parent->GetFileName () + "'");
                --p_Parent->RemainingPlayCount;
                // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

                p_Parent->FrameControl.LastPlayedFrameId = 0;
                p_Parent->CalculatePlayStartTime (0);
            }
            else
            {
                // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
                // DEBUG_V (String ("      Done Playing:: FileName: '") + p_Parent->GetFileName () + "'");
                Stop ();
                break;
            }
        }

        InputMgr.RestartBlankTimer (p_Parent->GetInputChannelId ());

    } while (false);

    // xDEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_PlayingFile::TimerPoll ()
{
    // xDEBUG_START;

    do // once
    {
        int32_t  SyncOffsetMS = p_Parent->GetSyncOffsetMS ();
        uint32_t CurrentFrame = p_Parent->CalculateFrameId (SyncOffsetMS);
        
        // xDEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
        // xDEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));

        // have we reached the end of the file?
        if (p_Parent->FrameControl.TotalNumberOfFramesInSequence <= CurrentFrame)
        {
            // trigger the background task to stop playing this file
            p_Parent->FrameControl.LastPlayedFrameId = CurrentFrame;
            break;
        }

        if (CurrentFrame == p_Parent->FrameControl.LastPlayedFrameId)
        {
            // xDEBUG_V (String ("keep waiting"));
            break;
        }

        uint32_t FilePosition       = p_Parent->FrameControl.DataOffset + (p_Parent->FrameControl.ChannelsPerFrame * CurrentFrame);
        size_t   MaxBytesToRead     = (p_Parent->FrameControl.ChannelsPerFrame > p_Parent->BufferSize) ? p_Parent->BufferSize : p_Parent->FrameControl.ChannelsPerFrame;
        byte   * CurrentDestination = p_Parent->Buffer;
        // xDEBUG_V (String ("               MaxBytesToRead: ") + String (MaxBytesToRead));

        p_Parent->FrameControl.LastPlayedFrameId = CurrentFrame;

        for (auto& CurrentSparseRange : p_Parent->SparseRanges)
        {
            size_t ActualBytesToRead = min (MaxBytesToRead, CurrentSparseRange.ChannelCount);
            if (0 == ActualBytesToRead)
            {
                // no data to read for this range
                continue;
            }

            uint32_t AdjustedFilePosition = FilePosition + CurrentSparseRange.DataOffset;

            // xDEBUG_V (String ("                 FilePosition: ") + String (FilePosition));
            // xDEBUG_V (String ("         AdjustedFilePosition: ") + String (uint32_t(AdjustedFilePosition), HEX));
            // xDEBUG_V (String ("           CurrentDestination: ") + String (uint32_t(CurrentDestination), HEX));
            // xDEBUG_V (String ("            ActualBytesToRead: ") + String (ActualBytesToRead));
            size_t ActualBytesRead = FileMgr.ReadSdFile (p_Parent->FileHandleForFileBeingPlayed,
                CurrentDestination,
                ActualBytesToRead,
                AdjustedFilePosition);
            MaxBytesToRead -= ActualBytesRead;
            CurrentDestination += ActualBytesRead;

            if (ActualBytesRead != ActualBytesToRead)
            {
                // xDEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
                // xDEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));

                if (0 != p_Parent->FileHandleForFileBeingPlayed)
                {
                    // logcon (F ("File Playback Failed to read enough data"));
                    Stop ();
                }
            }
        }

        // xDEBUG_V (String ("       DataOffset: ") + String (p_Parent->DataOffset));
        // xDEBUG_V (String ("       BufferSize: ") + String (p_Parent->BufferSize));
        // xDEBUG_V (String (" ChannelsPerFrame: ") + String (p_Parent->ChannelsPerFrame));
        // xDEBUG_V (String ("     FilePosition: ") + String (FilePosition));
        // xDEBUG_V (String ("   MaxBytesToRead: ") + String (MaxBytesToRead));
        // xDEBUG_V (String ("GetInputChannelId: ") + String (p_Parent->GetInputChannelId ()));

    } while (false);

} // fsm_PlayFile_state_PlayingFile::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    p_Parent = Parent;

    do // once
    {
        // DEBUG_V (String ("FileName: '") + p_Parent->PlayItemName + "'");
        // DEBUG_V (String (" FrameId: '") + p_Parent->LastPlayedFrameId + "'");
        // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
        if (0 == p_Parent->RemainingPlayCount)
        {
            Stop ();
            break;
        }

        --p_Parent->RemainingPlayCount;
        // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

        if (!p_Parent->ParseFseqFile ())
        {
            p_Parent->fsm_PlayFile_state_Stopping_imp.Init (p_Parent);
            break;
        }

        p_Parent->CalculatePlayStartTime (p_Parent->FrameControl.StartingFrameId);

        // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
        // DEBUG_V (String ("                  StartTimeMS: ") + String (p_Parent->StartTimeMS));
        // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

        logcon (String (F ("Start Playing:: FileName: '")) + p_Parent->PlayItemName + "'");

        Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_PlayingFile_imp);
        Parent->FrameControl.ElapsedPlayTimeMS = 0;

    } while (false);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Start (String& FileName, uint32_t FrameId, uint32_t PlayCount)
{
    // DEBUG_START;

    // DEBUG_V (String ("                     FileName: ") + FileName);
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    Stop ();
    p_Parent->Start (FileName, FrameId, PlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Stop (void)
{
    // DEBUG_START;

    logcon (String (F ("Stop Playing::  FileName: '")) + p_Parent->PlayItemName + "'");
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Stopping_imp.Init (p_Parent);

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
        if (FileName != p_Parent->GetFileName ())
        {
            // DEBUG_V ("Sync: Filename change");
            Stop ();
            p_Parent->Start (FileName, TargetFrameId, 1);
            break;
        }

        if (p_Parent->SyncControl.LastRcvdSyncFrameId >= TargetFrameId)
        {
            // DEBUG_V ("Duplicate or older sync msg");
            break;
        }
        p_Parent->SyncControl.LastRcvdSyncFrameId = TargetFrameId;

        uint32_t CurrentFrame = p_Parent->CalculateFrameId ();
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
        // DEBUG_V (String ("StartTimeMS: ") + String (p_Parent->StartTimeMS));

        if (CurrentFrame > TargetFrameId)
        {
            // p_Parent->SyncControl.TimeCorrectionFactor += TimeOffsetStep;
        }
        else
        {
            // p_Parent->SyncControl.TimeCorrectionFactor -= TimeOffsetStep;
        }
        // p_Parent->SyncControl.AdjustedFrameStepTimeMS = uint32_t (float (p_Parent->FrameControl.FrameStepTimeMS) * p_Parent->SyncControl.TimeCorrectionFactor);

        // p_Parent->LastPlayedFrameId = TargetFrameId-1;
        p_Parent->CalculatePlayStartTime (TargetFrameId);

        response = true;
        // DEBUG_V (String ("StartTimeMS: ") + String (p_Parent->StartTimeMS));

    } while (false);

    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_PlayingFile::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Poll ()
{
    // DEBUG_START;

    // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (p_Parent->FileHandleForFileBeingPlayed));

    FileMgr.CloseSdFile (p_Parent->FileHandleForFileBeingPlayed);
    p_Parent->FileHandleForFileBeingPlayed = 0;
    p_Parent->SaveTimeCorrectionFactor ();
    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

    if (FileName != "")
    {
        // DEBUG_V ("Restarting File");
        p_Parent->Start (FileName, StartingFrameId, PlayCount);
    }

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_Stopping::TimerPoll ()
{
    // nothing to do

} // fsm_PlayFile_state_Stopping::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;

    FileName        = "";
    StartingFrameId = 0;
    PlayCount       = 0;

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Stopping_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Start (String& _FileName, uint32_t _FrameId, uint32_t _PlayCount)
{
    // DEBUG_START;

    FileName        = _FileName;
    StartingFrameId = _FrameId;
    PlayCount       = _PlayCount;

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

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Stopping::Sync
