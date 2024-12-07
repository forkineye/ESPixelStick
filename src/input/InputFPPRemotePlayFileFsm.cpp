/*
* InputFPPRemotePlayFileFsm.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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

#include "input/InputFPPRemotePlayFile.hpp"
#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Idle::Poll ()
{
    // DEBUG_START;

    // DEBUG_V("fsm_PlayFile_state_Idle::Poll");

    // do nothing

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Idle::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_Idle::TimerPoll ()
{
    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
    p_Parent->FrameControl.TotalNumberOfFramesInSequence = 0;

} // fsm_PlayFile_state_Idle::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Init");

    // DEBUG_V (String (" Parent: 0x") + String ((uint32_t)Parent, HEX));
    p_Parent = Parent;
    p_Parent->pCurrentFsmState = &(p_Parent->fsm_PlayFile_state_Idle_imp);
    p_Parent->ClearFileInfo ();

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Start");

    // DEBUG_V (String ("FileName: ") + FileName);

    p_Parent->PlayItemName = FileName;
    p_Parent->FrameControl.ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000);
    p_Parent->RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->PlayItemName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FrameControl.ElapsedPlayTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Starting_imp.Init (p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Stop");

    // Do nothing

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Idle::Sync (String& FileName, float ElapsedSeconds)
{
    // DEBUG_START;
    // DEBUG_V("State: Idle");

    Start (FileName, ElapsedSeconds, 1);

    // DEBUG_END;

    return false;

} // fsm_PlayFile_state_Idle::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Starting::Poll ()
{
    // DEBUG_START;

    p_Parent->fsm_PlayFile_state_PlayingFile_imp.Init (p_Parent);
    // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->FrameControl.TotalNumberOfFramesInSequence));

    // DEBUG_END;
    return true;

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
    // DEBUG_V("fsm_PlayFile_state_Starting::Init");

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Starting_imp);

    if(p_Parent->SendFppSync)
    {
        FPPDiscovery.GenerateFppSyncMsg(SYNC_PKT_START, p_Parent->GetFileName(), 0, float(0.0));
    }

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Starting::Start");

    // DEBUG_V (String ("FileName: ") + FileName);
    p_Parent->PlayItemName = FileName;
    p_Parent->FrameControl.ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000);
    p_Parent->RemainingPlayCount = RemainingPlayCount;
    // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Starting::Stop");

    p_Parent->RemainingPlayCount = 0;
    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Starting::Sync (String& FileName, float ElapsedSeconds)
{
    // DEBUG_START;
    // DEBUG_V("State:Starting");

    bool response = false;

    Start (FileName, ElapsedSeconds, 1);

    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_Starting::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_PlayingFile::Poll ()
{
    // xDEBUG_START;

    bool Response = false;

    do // once
    {
        if(c_FileMgr::INVALID_FILE_HANDLE == p_Parent->FileHandleForFileBeingPlayed)
        {
            // DEBUG_V("Bad FileHandleForFileBeingPlayed");
            Stop();
            break;
        }
        // DEBUG_V (String ("LastPlayedFrameId: ") + String (LastPlayedFrameId));
        // have we reached the end of the file?
        if (p_Parent->FrameControl.TotalNumberOfFramesInSequence <= LastPlayedFrameId)
        {
            // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
            if (0 != p_Parent->RemainingPlayCount)
            {
                // DEBUG_V (String ("RemainingPlayCount: ") + String (p_Parent->RemainingPlayCount));
                // DEBUG_V (String ("Replaying:: FileName:      '") + p_Parent->GetFileName () + "'");
                --p_Parent->RemainingPlayCount;
                // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

                p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
                LastPlayedFrameId = 0;
            }
            else
            {
                // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
                // DEBUG_V (String ("      Done Playing:: FileName: '") + p_Parent->GetFileName () + "'");
                p_Parent->Stop ();
                Response = true;
                break;
            }
        }
        // DEBUG_V();
        InputMgr.RestartBlankTimer (p_Parent->GetInputChannelId ());

    } while (false);

    // xDEBUG_END;
    return Response;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_PlayingFile::TimerPoll ()
{
    // xDEBUG_START;

    do // once
    {
        if(c_FileMgr::INVALID_FILE_HANDLE == p_Parent->FileHandleForFileBeingPlayed)
        {
            // DEBUG_V("Bad FileHandleForFileBeingPlayed");
            Stop();
            break;
        }

        uint32_t CurrentFrame = p_Parent->CalculateFrameId (p_Parent->FrameControl.ElapsedPlayTimeMS, p_Parent->GetSyncOffsetMS ());

        // xDEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
        // xDEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));

        // have we reached the end of the file?
        if (p_Parent->FrameControl.TotalNumberOfFramesInSequence <= CurrentFrame)
        {
            LastPlayedFrameId = CurrentFrame;
            break;
        }

        if (CurrentFrame == LastPlayedFrameId)
        {
            // xDEBUG_V (String ("keep waiting"));
            break;
        }

        uint32_t FilePosition = p_Parent->FrameControl.DataOffset + (p_Parent->FrameControl.ChannelsPerFrame * CurrentFrame);
        uint32_t BufferSize = OutputMgr.GetBufferUsedSize();
        uint32_t MaxBytesToRead = (p_Parent->FrameControl.ChannelsPerFrame > BufferSize) ? BufferSize : p_Parent->FrameControl.ChannelsPerFrame;

        uint32_t CurrentDestination = 0;
        // xDEBUG_V (String ("               MaxBytesToRead: ") + String (MaxBytesToRead));

        LastPlayedFrameId = CurrentFrame;

        if(p_Parent->SendFppSync)
        {
            FPPDiscovery.GenerateFppSyncMsg(SYNC_PKT_SYNC, p_Parent->GetFileName(), CurrentFrame, float(p_Parent->FrameControl.ElapsedPlayTimeMS) / 1000.0);
        }

        for (auto& CurrentSparseRange : p_Parent->SparseRanges)
        {
            uint32_t ActualBytesToRead = min (MaxBytesToRead, CurrentSparseRange.ChannelCount);
            if (0 == ActualBytesToRead)
            {
                // no data to read for this range
                continue;
            }

            uint32_t AdjustedFilePosition = FilePosition + CurrentSparseRange.DataOffset;

            /// DEBUG_V (String ("                 FilePosition: ") + String (FilePosition));
            /// DEBUG_V (String ("         AdjustedFilePosition: ") + String (uint32_t(AdjustedFilePosition), HEX));
            /// DEBUG_V (String ("           CurrentDestination: ") + String (uint32_t(CurrentDestination), HEX));
            /// DEBUG_V (String ("            ActualBytesToRead: ") + String (ActualBytesToRead));
            uint32_t ActualBytesRead = p_Parent->ReadFile(CurrentDestination, ActualBytesToRead, AdjustedFilePosition);

            MaxBytesToRead -= ActualBytesRead;
            CurrentDestination += ActualBytesRead;

            if (ActualBytesRead != ActualBytesToRead)
            {
                // xDEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
                // xDEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));

                // DEBUG_V (F ("File Playback Failed to read enough data"));
                p_Parent->Stop ();
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
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Init");

    p_Parent = Parent;

    do // once
    {
        LastPlayedFrameId = 0;

        // DEBUG_V (String ("FileName: '") + p_Parent->PlayItemName + "'");
        // DEBUG_V (String (" FrameId: '") + p_Parent->LastPlayedFrameId + "'");
        // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
        if (0 == p_Parent->RemainingPlayCount)
        {
            // DEBUG_V();
            p_Parent->Stop ();
            break;
        }

        --p_Parent->RemainingPlayCount;
        // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);


        if (!p_Parent->ParseFseqFile ())
        {
            p_Parent->fsm_PlayFile_state_Error_imp.Init (p_Parent);
            break;
        }

        // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
        // DEBUG_V (String ("                  StartTimeMS: ") + String (p_Parent->StartTimeMS));
        // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

        // DEBUG_V (String (F ("Start Playing:: FileName: '")) + p_Parent->PlayItemName + "'");

        Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_PlayingFile_imp);
        Parent->FrameControl.ElapsedPlayTimeMS = 0;

    } while (false);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Start (String& FileName, float ElapsedSeconds, uint32_t PlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Start");

    // DEBUG_V (String ("                     FileName: ") + FileName);
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->RemainingPlayCount);
    // DEBUG_V();
    p_Parent->Stop ();
    // DEBUG_V();
    p_Parent->Start (FileName, ElapsedSeconds, PlayCount);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Stop");

    // DEBUG_V (String (F ("Stop Playing::  FileName: '")) + p_Parent->PlayItemName + "'");
    // DEBUG_V (String ("            LastPlayedFrameId: ") + String (p_Parent->LastPlayedFrameId));
    // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->TotalNumberOfFramesInSequence));
    // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Stopping_imp.Init (p_Parent);

    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
    LastPlayedFrameId = 0;

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_PlayingFile::Sync (String& FileName, float ElapsedSeconds)
{
    // DEBUG_START;
    bool response = false;

    do // once
    {
        // are we on the correct file?
        if (!FileName.equals(p_Parent->GetFileName ()))
        {
            // DEBUG_V ("Sync: Filename change");
            p_Parent->Stop ();
            // DEBUG_V();
            p_Parent->Start (FileName, ElapsedSeconds, 1);
            break;
        }

        if (p_Parent->SyncControl.LastRcvdElapsedSeconds >= ElapsedSeconds)
        {
            // DEBUG_V ("Duplicate or older sync msg");
            break;
        }

        // DEBUG_V (String ("old LastRcvdElapsedSeconds: ") + String (p_Parent->SyncControl.LastRcvdElapsedSeconds));

        p_Parent->SyncControl.LastRcvdElapsedSeconds = ElapsedSeconds;

        // DEBUG_V (String ("new LastRcvdElapsedSeconds: ") + String (p_Parent->SyncControl.LastRcvdElapsedSeconds));
        // DEBUG_V (String ("         ElapsedPlayTimeMS: ") + String (p_Parent->FrameControl.ElapsedPlayTimeMS));

        uint32_t TargetElapsedMS = uint32_t (ElapsedSeconds * 1000);
        uint32_t CurrentFrame = p_Parent->CalculateFrameId (p_Parent->FrameControl.ElapsedPlayTimeMS, 0);
        uint32_t TargetFrameId = p_Parent->CalculateFrameId (TargetElapsedMS, 0);
        int32_t  FrameDiff = TargetFrameId - CurrentFrame;

        if (2 > abs (FrameDiff))
        {
            // DEBUG_V ("No Need to adjust the time");
            break;
        }

        // DEBUG_V ("Need to adjust the start time");
        // DEBUG_V (String ("ElapsedPlayTimeMS: ") + String (p_Parent->FrameControl.ElapsedPlayTimeMS));

        // DEBUG_V (String ("     CurrentFrame: ") + String (CurrentFrame));
        // DEBUG_V (String ("    TargetFrameId: ") + String (TargetFrameId));
        // DEBUG_V (String ("        FrameDiff: ") + String (FrameDiff));

        // Adjust the start of the file time to align with the master FPP
        noInterrupts ();
        if (20 < abs (FrameDiff))
        {
            // DEBUG_V ("Large Setp Adjustment");
            p_Parent->FrameControl.ElapsedPlayTimeMS = TargetElapsedMS;
        }
        else if(CurrentFrame > TargetFrameId)
        {
            // DEBUG_V("go back a frame");
            p_Parent->FrameControl.ElapsedPlayTimeMS -= p_Parent->FrameControl.FrameStepTimeMS;
        }
        else
        {
            // DEBUG_V("go forward a frame");
            p_Parent->FrameControl.ElapsedPlayTimeMS += p_Parent->FrameControl.FrameStepTimeMS;
        }
        interrupts ();

        response = true;
        // DEBUG_V (String ("ElapsedPlayTimeMS: ") + String (p_Parent->FrameControl.ElapsedPlayTimeMS));

    } while (false);

    // DEBUG_END;
    return response;

} // fsm_PlayFile_state_PlayingFile::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Stopping::Poll ()
{
    // DEBUG_START;

    // DEBUG_V("fsm_PlayFile_state_Stopping::Poll");
    // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (p_Parent->FileHandleForFileBeingPlayed));

    FileMgr.CloseSdFile (p_Parent->FileHandleForFileBeingPlayed);
    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

    StartingElapsedTime = 0.0;
    PlayCount = 0;

    p_Parent->SyncControl.LastRcvdElapsedSeconds = 0;
    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;

    if(p_Parent->SendFppSync)
    {
        FPPDiscovery.GenerateFppSyncMsg(SYNC_PKT_STOP, emptyString, 0, float(0.0));
    }

    if (!FileName.equals(emptyString))
    {
        // DEBUG_V ("Restarting File");
        p_Parent->Start (FileName, StartingElapsedTime, PlayCount);
    }

    // DEBUG_END;
    return true;

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
    // DEBUG_V("fsm_PlayFile_state_Stopping::Init");

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Stopping_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Start (String& _FileName, float ElapsedTime, uint32_t _PlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Stopping::Start");

    FileName = _FileName;
    StartingElapsedTime = ElapsedTime;
    PlayCount = _PlayCount;

    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Stopping::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Stopping::Stop");
    // DEBUG_V("Not actually doing anything");
    // DEBUG_END;

} // fsm_PlayFile_state_Stopping::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Stopping::Sync (String&, float)
{
    // DEBUG_START;
    // DEBUG_V("State:Stopping");

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Stopping::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Error::Poll ()
{
    // xDEBUG_START;

    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
    p_Parent->FrameControl.FrameStepTimeMS = 0;
    p_Parent->FrameControl.TotalNumberOfFramesInSequence = 0;

    // xDEBUG_END;
    return false;

} // fsm_PlayFile_state_Error::Poll

//-----------------------------------------------------------------------------
IRAM_ATTR void fsm_PlayFile_state_Error::TimerPoll ()
{
    // xDEBUG_START;

    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
    p_Parent->FrameControl.FrameStepTimeMS = 0;
    p_Parent->FrameControl.TotalNumberOfFramesInSequence = 0;

    // xDEBUG_END;

} // fsm_PlayFile_state_Error::TimerPoll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Error::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Error::Init");

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Error_imp);

    p_Parent->FrameControl.ElapsedPlayTimeMS = 0;
    p_Parent->FrameControl.FrameStepTimeMS = 0;
    p_Parent->FrameControl.TotalNumberOfFramesInSequence = 0;

    if(c_FileMgr::INVALID_FILE_HANDLE != p_Parent->FileHandleForFileBeingPlayed)
    {
        // DEBUG_V("Unexpected File Handle at error init.");
        FileMgr.CloseSdFile(p_Parent->FileHandleForFileBeingPlayed);
    }

    // DEBUG_END;

} // fsm_PlayFile_state_Error::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Error::Start (String& FileName, float ElapsedSeconds, uint32_t PlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Error::Start");

    if (!FileName.equals(p_Parent->GetFileName ()))
    {
        p_Parent->fsm_PlayFile_state_Idle_imp.Start (FileName, ElapsedSeconds, PlayCount);
    }

    // DEBUG_END

} // fsm_PlayFile_state_Error::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Error::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Error::Stop");

    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_Error::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Error::Sync (String& FileName, float ElapsedSeconds)
{
    // DEBUG_START;
    // DEBUG_V("State:Error");

    if (!FileName.equals(p_Parent->GetFileName ()))
    {
        p_Parent->fsm_PlayFile_state_Idle_imp.Start (FileName, ElapsedSeconds, 1);
    }

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Error::Sync
