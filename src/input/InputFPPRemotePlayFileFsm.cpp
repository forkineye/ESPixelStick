/*
* InputFPPRemotePlayFileFsm.cpp
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
*   Fsm object used to parse and play an File
*/

#include "input/InputFPPRemotePlayFile.hpp"
#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Idle::Poll ()
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Poll");

    // is there a new file to play?
    if(!String(p_Parent->FileControl[NextFile].FileName).isEmpty())
    {
        // DEBUG_V(String("Start configured file: ") + p_Parent->FileControl[NextFile].FileName);
        p_Parent->fsm_PlayFile_state_Starting_imp.Init (p_Parent);
    }
    else if (!String(p_Parent->BackgroundFileName).isEmpty())
    {
        String temp = String(p_Parent->BackgroundFileName);
        // DEBUG_V(String("Start background file: ") + temp);
        Start(temp, 0.0, 1);
        p_Parent->fsm_PlayFile_state_Starting_imp.Init (p_Parent);
    }

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Idle::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Init");

    // DEBUG_V (String ("Parent: 0x") + String ((uint32_t)Parent, HEX));
    p_Parent = Parent;
    p_Parent->pCurrentFsmState = &(p_Parent->fsm_PlayFile_state_Idle_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Idle::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Idle::Start");
    // DEBUG_V (String ("FileName: ") + FileName);
    // DEBUG_V (String ("NextFile: ") + String(NextFile));

    strcpy(&p_Parent->FileControl[NextFile].FileName[0], FileName.c_str());
    p_Parent->FileControl[NextFile].ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000.0);
    p_Parent->FileControl[NextFile].LastPollTimeMS = millis();
    p_Parent->FileControl[NextFile].StartingTimeMS = p_Parent->FileControl[NextFile].LastPollTimeMS - p_Parent->FileControl[NextFile].ElapsedPlayTimeMS;
    p_Parent->FileControl[NextFile].RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("          Backgroud: ") + p_Parent->BackgroundFileName);
    // DEBUG_V (String ("           FileName: ") + p_Parent->FileControl[NextFile].FileName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FileControl[NextFile].ElapsedPlayTimeMS);
    // DEBUG_V (String ("     LastPollTimeMS: ") + p_Parent->FileControl[NextFile].LastPollTimeMS);
    // DEBUG_V (String ("     StartingTimeMS: ") + p_Parent->FileControl[NextFile].StartingTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->FileControl[NextFile].RemainingPlayCount);

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
    // DEBUG_V("fsm_PlayFile_state_Idle::Sync");

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
    // DEBUG_V("fsm_PlayFile_state_Starting::Poll");

    do // once
    {
        p_Parent->FileControl[CurrentFile] = p_Parent->FileControl[NextFile];
        p_Parent->ClearControlFileInfo();
        p_Parent->SyncControl.LastRcvdElapsedSeconds = 0.0;

        if (!p_Parent->ParseFseqFile ())
        {
            // DEBUG_V("fsm_PlayFile_state_PlayingFile::Poll FSEQ Parse Error");
            p_Parent->fsm_PlayFile_state_Error_imp.Init(p_Parent);
            break;
        }

        p_Parent->FileControl[CurrentFile].RemainingPlayCount --;
        p_Parent->FileControl[CurrentFile].LastPollTimeMS = millis();
        p_Parent->FileControl[CurrentFile].StartingTimeMS = p_Parent->FileControl[CurrentFile].LastPollTimeMS - p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS;
        p_Parent->FileControl[CurrentFile].LastPlayedFrameId = p_Parent->CalculateFrameId (p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS, p_Parent->GetSyncOffsetMS ());

        // DEBUG_V (String ("                FileName: ") + p_Parent->FileControl[CurrentFile].FileName);
        // DEBUG_V (String ("       ElapsedPlayTimeMS: ") + p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS);
        // DEBUG_V (String ("          LastPollTimeMS: ") + p_Parent->FileControl[CurrentFile].LastPollTimeMS);
        // DEBUG_V (String ("          StartingTimeMS: ") + p_Parent->FileControl[CurrentFile].StartingTimeMS);
        // DEBUG_V (String ("      RemainingPlayCount: ") + p_Parent->FileControl[CurrentFile].RemainingPlayCount);
        // DEBUG_V (String ("       LastPlayedFrameId: ") + p_Parent->FileControl[CurrentFile].LastPlayedFrameId);
        // DEBUG_V (String ("NumberOfFramesInSequence: ") + p_Parent->FileControl[CurrentFile].TotalNumberOfFramesInSequence);

        p_Parent->fsm_PlayFile_state_PlayingFile_imp.Init (p_Parent);

    } while (false);

    // DEBUG_END;
    return true;

} // fsm_PlayFile_state_Starting::Poll

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

    strcpy(p_Parent->FileControl[NextFile].FileName, FileName.c_str());
    p_Parent->FileControl[NextFile].ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000.0);
    p_Parent->FileControl[NextFile].LastPollTimeMS = millis();
    p_Parent->FileControl[NextFile].StartingTimeMS = p_Parent->FileControl[NextFile].LastPollTimeMS - p_Parent->FileControl[NextFile].ElapsedPlayTimeMS;
    p_Parent->FileControl[NextFile].RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->FileControl[NextFile].FileName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FileControl[NextFile].ElapsedPlayTimeMS);
    // DEBUG_V (String ("     LastPollTimeMS: ") + p_Parent->FileControl[NextFile].LastPollTimeMS);
    // DEBUG_V (String ("     StartingTimeMS: ") + p_Parent->FileControl[NextFile].StartingTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->FileControl[NextFile].RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Stopping_imp.Init(p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Starting::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Starting::Stop");

    p_Parent->FileControl[CurrentFile].RemainingPlayCount = 0;
    p_Parent->fsm_PlayFile_state_Stopping_imp.Init (p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_Starting::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Starting::Sync (String& FileName, float ElapsedSeconds)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Starting::Sync");

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
    ///DEBUG_START;
    ///DEBUG_V("fsm_PlayFile_state_PlayingFile::Poll");

    bool Response = false;
    p_Parent->FileControl[CurrentFile].NumPollsSinceSync++;

    do // once
    {
        if(c_FileMgr::INVALID_FILE_HANDLE == p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed)
        {
            // DEBUG_V("Unexpected missing file handle");
            Stop();
            break;
        }

        p_Parent->UpdateElapsedPlayTimeMS();
        uint32_t CurrentFrame = p_Parent->CalculateFrameId (p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS, p_Parent->GetSyncOffsetMS ());

        ///DEBUG_V (String ("LastPlayedFrameId: ") + String (CurrentFrame));
        // have we reached the end of the file?
        if (p_Parent->FileControl[CurrentFile].TotalNumberOfFramesInSequence <= CurrentFrame)
        {
            // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->FileControl[CurrentFile].RemainingPlayCount);
            if (0 != p_Parent->FileControl[CurrentFile].RemainingPlayCount)
            {
                // DEBUG_V (String ("   Restart Playing:: FileName: '") + p_Parent->GetFileName () + "'");
                // DEBUG_V (String ("RemainingPlayCount: ") + String (p_Parent->FileControl[CurrentFile].RemainingPlayCount));
                // DEBUG_V (String ("Replaying:: FileName:      '") + p_Parent->GetFileName () + "'");
                --p_Parent->FileControl[CurrentFile].RemainingPlayCount;
                // DEBUG_V (String ("RemainingPlayCount: ") + p_Parent->FileControl[CurrentFile].RemainingPlayCount);

                p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS = 0;
                p_Parent->FileControl[CurrentFile].RemainingPlayCount --;
                p_Parent->FileControl[CurrentFile].LastPollTimeMS = millis();
                p_Parent->FileControl[CurrentFile].StartingTimeMS = p_Parent->FileControl[CurrentFile].LastPollTimeMS;
                p_Parent->FileControl[CurrentFile].LastPlayedFrameId = p_Parent->CalculateFrameId (p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS, p_Parent->GetSyncOffsetMS ());

                // DEBUG_V (String ("                FileName: ") + p_Parent->FileControl[CurrentFile].FileName);
                // DEBUG_V (String ("       ElapsedPlayTimeMS: ") + p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS);
                // DEBUG_V (String ("          LastPollTimeMS: ") + p_Parent->FileControl[CurrentFile].LastPollTimeMS);
                // DEBUG_V (String ("          StartingTimeMS: ") + p_Parent->FileControl[CurrentFile].StartingTimeMS);
                // DEBUG_V (String ("      RemainingPlayCount: ") + p_Parent->FileControl[CurrentFile].RemainingPlayCount);
                // DEBUG_V (String ("       LastPlayedFrameId: ") + p_Parent->FileControl[CurrentFile].LastPlayedFrameId);
                // DEBUG_V (String ("NumberOfFramesInSequence: ") + p_Parent->FileControl[CurrentFile].TotalNumberOfFramesInSequence);
            }
            else
            {
                // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->FileControl[CurrentFile].TotalNumberOfFramesInSequence));
                // DEBUG_V (String ("      Done Playing:: FileName: '") + p_Parent->FileControl[CurrentFile].FileName + "'");
                Stop ();
                Response = true;
                break;
            }
            break;
        }

        // Do we need to do an update?
        if (CurrentFrame == p_Parent->FileControl[CurrentFile].LastPlayedFrameId)
        {
            ///DEBUG_V (String ("keep waiting"));
            break;
        }
        p_Parent->FileControl[CurrentFile].LastPlayedFrameId = CurrentFrame;

        uint32_t FilePosition = p_Parent->FileControl[CurrentFile].DataOffset + (p_Parent->FileControl[CurrentFile].ChannelsPerFrame * CurrentFrame);
        uint32_t BufferSize = OutputMgr.GetBufferUsedSize();
        uint32_t MaxBytesToRead = (p_Parent->FileControl[CurrentFile].ChannelsPerFrame > BufferSize) ? BufferSize : p_Parent->FileControl[CurrentFile].ChannelsPerFrame;

        uint32_t CurrentOutputBufferOffset = 0;
        ///DEBUG_V (String ("               MaxBytesToRead: ") + String (MaxBytesToRead));

        InputMgr.RestartBlankTimer (p_Parent->GetInputChannelId ());
        ///DEBUG_V();

        if(p_Parent->SendFppSync)
        {
            FPPDiscovery.GenerateFppSyncMsg(SYNC_PKT_SYNC, p_Parent->GetFileName(), CurrentFrame, float(p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS) / 1000.0);
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

            ///DEBUG_V (String ("                 FilePosition: ") + String (FilePosition));
            ///DEBUG_V (String ("         AdjustedFilePosition: ") + String (uint32_t(AdjustedFilePosition), HEX));
            ///DEBUG_V (String ("           CurrentDestination: ") + String (uint32_t(CurrentDestination), HEX));
            ///DEBUG_V (String ("            ActualBytesToRead: ") + String (ActualBytesToRead));
            // DEBUG_FILE_HANDLE(p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);
            uint32_t ActualBytesRead = p_Parent->ReadFile(CurrentOutputBufferOffset, ActualBytesToRead, AdjustedFilePosition);
            // DEBUG_FILE_HANDLE(p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);

            MaxBytesToRead -= ActualBytesRead;
            CurrentOutputBufferOffset += ActualBytesRead;

            if (ActualBytesRead != ActualBytesToRead)
            {
                // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (p_Parent->FileControl[CurrentFile].TotalNumberOfFramesInSequence));
                // DEBUG_V (String ("                 CurrentFrame: ") + String (CurrentFrame));
                // DEBUG_V (String ("            ActualBytesToRead: ") + String (ActualBytesToRead));
                // DEBUG_V (String ("              ActualBytesRead: ") + String (ActualBytesRead));
                logcon (F ("File Playback Failed to read enough data"));
                Stop ();
            }
        }

    } while (false);

    ///DEBUG_END;
    return Response;

} // fsm_PlayFile_state_PlayingFile::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Init");

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_PlayingFile_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Start");

    // DEBUG_V("Set up the next file");
    strcpy(p_Parent->FileControl[NextFile].FileName, FileName.c_str());
    p_Parent->FileControl[NextFile].ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000.0);
    p_Parent->FileControl[NextFile].LastPollTimeMS = millis();
    p_Parent->FileControl[NextFile].StartingTimeMS = p_Parent->FileControl[NextFile].LastPollTimeMS - p_Parent->FileControl[NextFile].ElapsedPlayTimeMS;
    p_Parent->FileControl[NextFile].RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->FileControl[NextFile].FileName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FileControl[NextFile].ElapsedPlayTimeMS);
    // DEBUG_V (String ("     LastPollTimeMS: ") + p_Parent->FileControl[NextFile].LastPollTimeMS);
    // DEBUG_V (String ("     StartingTimeMS: ") + p_Parent->FileControl[NextFile].StartingTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->FileControl[NextFile].RemainingPlayCount);

    // DEBUG_V("Stop the current file");
    Stop ();

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Start

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_PlayingFile::Stop (void)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_PlayingFile::Stop");
    // DEBUG_V (String (F ("Stop Playing::  FileName: '")) + p_Parent->FileControl[CurrentFile].FileName + "'");
    // DEBUG_V (String ("           RemainingPlayCount: ") + p_Parent->FileControl[CurrentFile].RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Stopping_imp.Init (p_Parent);

    // DEBUG_END;

} // fsm_PlayFile_state_PlayingFile::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_PlayingFile::Sync (String& FileName, float ElapsedSeconds)
{
    /// DEBUG_START;
    /// DEBUG_V("fsm_PlayFile_state_PlayingFile::Sync");

    bool response = false;

    do // once
    {
        // are we on the correct file?
        if (!FileName.equals(p_Parent->GetFileName ()))
        {
            /// DEBUG_V ("Sync: Filename change");
            /// DEBUG_V (String("New FileName: ") + FileName);
            /// DEBUG_V (String("Old FileName: ") + p_Parent->GetFileName ());
            Start (FileName, ElapsedSeconds, 1);
            break;
        }

        if (p_Parent->SyncControl.LastRcvdElapsedSeconds >= ElapsedSeconds)
        {
            /// DEBUG_V ("Duplicate or older sync msg");
            break;
        }

        /// DEBUG_V (String ("        ESPS millis: ") + String (millis()));
        /// DEBUG_V (String ("ESPS StartingTimeMS: ") + String (p_Parent->FileControl[CurrentFile].StartingTimeMS));
        /// DEBUG_V (String (" old  FPP ElapsedMS: ") + String (uint(p_Parent->SyncControl.LastRcvdElapsedSeconds) * 1000));
        /// DEBUG_V (String ("      FPP ElapsedMS: ") + String (uint(ElapsedSeconds) * 1000));
        /// DEBUG_V (String ("     ESPS ElapsedMS: ") + String ((millis() - p_Parent->FileControl[CurrentFile].StartingTimeMS)));
        /// DEBUG_V (String ("  NumPollsSinceSync: ") + String (p_Parent->FileControl[CurrentFile].NumPollsSinceSync));
        p_Parent->FileControl[CurrentFile].NumPollsSinceSync = 0;

        p_Parent->SyncControl.LastRcvdElapsedSeconds = ElapsedSeconds;

        /// DEBUG_V (String ("new LastRcvdElapsedSeconds: ") + String (p_Parent->SyncControl.LastRcvdElapsedSeconds));
        /// DEBUG_V (String ("         ElapsedPlayTimeMS: ") + String (p_Parent->FileControl.ElapsedPlayTimeMS));

        uint32_t TargetElapsedMS = uint32_t (ElapsedSeconds * 1000);
        uint32_t CurrentFrame = p_Parent->FileControl[CurrentFile].LastPlayedFrameId;
        uint32_t TargetFrameId = p_Parent->CalculateFrameId (TargetElapsedMS, 0);
        int32_t  FrameDiff = TargetFrameId - CurrentFrame;

        // DEBUG_V (String ("     CurrentFrameId: ") + String (CurrentFrame));
        // DEBUG_V (String ("      TargetFrameId: ") + String (TargetFrameId));
        // DEBUG_V (String ("          FrameDiff: ") + String (FrameDiff));

        if (2 > abs (FrameDiff))
        {
            /// DEBUG_V ("No Need to adjust the time");
            break;
        }

        ///DEBUG_V ("Need to adjust the start time");
        ///DEBUG_V (String ("ElapsedPlayTimeMS: ") + String (p_Parent->FileControl.ElapsedPlayTimeMS));

        // Adjust the start of the file time to align with the master FPP
        if (20 < abs (FrameDiff))
        {
            /// DEBUG_V ("Large Setp Adjustment");
            p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS = TargetElapsedMS;
            p_Parent->FileControl[CurrentFile].StartingTimeMS = millis() - TargetElapsedMS;
        }
        else if(CurrentFrame > TargetFrameId)
        {
            /// DEBUG_V("go back a frame");
            p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS -= p_Parent->FileControl[CurrentFile].FrameStepTimeMS;
            p_Parent->FileControl[CurrentFile].StartingTimeMS += p_Parent->FileControl[CurrentFile].FrameStepTimeMS;
        }
        else
        {
            /// DEBUG_V("go forward a frame");
            p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS += p_Parent->FileControl[CurrentFile].FrameStepTimeMS;
            p_Parent->FileControl[CurrentFile].StartingTimeMS -= p_Parent->FileControl[CurrentFile].FrameStepTimeMS;
        }

        response = true;
        /// DEBUG_V (String ("ESPS StartingTimeMS: ") + String (p_Parent->FileControl[CurrentFile].StartingTimeMS));
        /// DEBUG_V (String ("ElapsedPlayTimeMS: ") + String (p_Parent->FileControl[CurrentFile].ElapsedPlayTimeMS));

    } while (false);

    /// DEBUG_END;
    return response;

} // fsm_PlayFile_state_PlayingFile::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Stopping::Poll ()
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Stopping::Poll");

    // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed));

    if(c_FileMgr::INVALID_FILE_HANDLE != p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed)
    {
        // DEBUG_FILE_HANDLE (p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);
        FileMgr.CloseSdFile (p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);
    }
    else
    {
        // DEBUG_V("Unexpected missing file handle");
    }

    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

    if(p_Parent->SendFppSync)
    {
        FPPDiscovery.GenerateFppSyncMsg(SYNC_PKT_STOP, emptyString, 0, float(0.0));
    }

    // DEBUG_END;
    return true;

} // fsm_PlayFile_state_Stopping::Poll

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
void fsm_PlayFile_state_Stopping::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Stopping::Start");

    // DEBUG_V("Set up a next file");

    strcpy(p_Parent->FileControl[NextFile].FileName, FileName.c_str());
    p_Parent->FileControl[NextFile].ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000.0);
    p_Parent->FileControl[NextFile].LastPollTimeMS = millis();
    p_Parent->FileControl[NextFile].StartingTimeMS = p_Parent->FileControl[NextFile].LastPollTimeMS - p_Parent->FileControl[NextFile].ElapsedPlayTimeMS;
    p_Parent->FileControl[NextFile].RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->FileControl[NextFile].FileName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FileControl[NextFile].ElapsedPlayTimeMS);
    // DEBUG_V (String ("     LastPollTimeMS: ") + p_Parent->FileControl[NextFile].LastPollTimeMS);
    // DEBUG_V (String ("     StartingTimeMS: ") + p_Parent->FileControl[NextFile].StartingTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->FileControl[NextFile].RemainingPlayCount);

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
    // DEBUG_V("fsm_PlayFile_state_Stopping::Sync");

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Stopping::Sync

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool fsm_PlayFile_state_Error::Poll ()
{
    ///DEBUG_START;
    ///DEBUG_V("fsm_PlayFile_state_Error::Poll");

    if(c_FileMgr::INVALID_FILE_HANDLE != p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed)
    {
        ///DEBUG_V("Unexpected file handle in Error handler.");
        // DEBUG_FILE_HANDLE (p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);
        FileMgr.CloseSdFile (p_Parent->FileControl[CurrentFile].FileHandleForFileBeingPlayed);
    }
    else
    {
        ///DEBUG_V("Unexpected missing file handle");
    }

    memset(p_Parent->FileControl[CurrentFile].FileName, 0x0, sizeof(p_Parent->FileControl[CurrentFile].FileName));

    ///DEBUG_END;
    return false;

} // fsm_PlayFile_state_Error::Poll

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Error::Init (c_InputFPPRemotePlayFile* Parent)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Error::Init");

    p_Parent = Parent;
    Parent->pCurrentFsmState = &(Parent->fsm_PlayFile_state_Error_imp);

    // DEBUG_END;

} // fsm_PlayFile_state_Error::Init

//-----------------------------------------------------------------------------
void fsm_PlayFile_state_Error::Start (String& FileName, float ElapsedSeconds, uint32_t RemainingPlayCount)
{
    // DEBUG_START;
    // DEBUG_V("fsm_PlayFile_state_Error::Start");

    // DEBUG_V("Set up a next file");

    strcpy(p_Parent->FileControl[NextFile].FileName, FileName.c_str());
    p_Parent->FileControl[NextFile].ElapsedPlayTimeMS = uint32_t (ElapsedSeconds * 1000.0);
    p_Parent->FileControl[NextFile].LastPollTimeMS = millis();
    p_Parent->FileControl[NextFile].StartingTimeMS = p_Parent->FileControl[NextFile].LastPollTimeMS - p_Parent->FileControl[NextFile].ElapsedPlayTimeMS;
    p_Parent->FileControl[NextFile].RemainingPlayCount = RemainingPlayCount;

    // DEBUG_V (String ("           FileName: ") + p_Parent->FileControl[NextFile].FileName);
    // DEBUG_V (String ("  ElapsedPlayTimeMS: ") + p_Parent->FileControl[NextFile].ElapsedPlayTimeMS);
    // DEBUG_V (String ("     LastPollTimeMS: ") + p_Parent->FileControl[NextFile].LastPollTimeMS);
    // DEBUG_V (String ("     StartingTimeMS: ") + p_Parent->FileControl[NextFile].StartingTimeMS);
    // DEBUG_V (String (" RemainingPlayCount: ") + p_Parent->FileControl[NextFile].RemainingPlayCount);

    p_Parent->fsm_PlayFile_state_Idle_imp.Init (p_Parent);

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
    // DEBUG_V("fsm_PlayFile_state_Error::Sync");

    Start(FileName, ElapsedSeconds, 1);

    // DEBUG_END;
    return false;

} // fsm_PlayFile_state_Error::Sync
