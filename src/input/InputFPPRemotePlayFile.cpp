/*
* InputFPPRemotePlayFile.cpp
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
*   PlayFile object used to parse and play an FSEQ File
*/

#include "input/InputFPPRemotePlayFile.hpp"
#include "service/FPPDiscovery.h"
#include "service/fseq.h"
#include "utility/SaferStringConversion.hpp"

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
        Poll ();
    }

    // DEBUG_END;

} // ~c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Start (String & FileName, float SecondsElapsed, uint32_t PlayCount)
{
    // DEBUG_START;

    if (!InputIsPaused() && FileMgr.SdCardIsInstalled ())
    {
        // DEBUG_V("Ask FSM to start the sequence.");
        pCurrentFsmState->Start (FileName, SecondsElapsed, PlayCount);
    }
    else
    {
        // DEBUG_V ("Paused or No SD Card installed. Ignore Start request");
        fsm_PlayFile_state_Idle_imp.Init (this);
    }

    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Stop ()
{
    // DEBUG_START;

    if(!InputIsPaused())
    {
        pCurrentFsmState->Stop ();
    }

    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Sync (String & FileName, float SecondsElapsed)
{
    // xDEBUG_START;

    if(!InputIsPaused())
    {
        SyncControl.SyncCount++;
        UpdateElapsedPlayTimeMS ();
        if (pCurrentFsmState->Sync (FileName, SecondsElapsed))
        {
            SyncControl.SyncAdjustmentCount++;
        }
    }

    // xDEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayFile::Poll ()
{
    // xDEBUG_START;
    bool Response = false;

    if(!InputIsPaused())
    {
        // xDEBUG_V("Poll the FSM: Start");
        Response = pCurrentFsmState->Poll ();
        // xDEBUG_V("Poll the FSM: Done");
    }

    // xDEBUG_END;
    return Response;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::GetStatus (JsonObject& JsonStatus)
{
    // xDEBUG_START;

    // NOTE:
    // variables ending in "Total" reflect total     play time
    // variables ending in "Rem"   reflect remaining play time

    // File uses milliseconds to store elapsed and total play time
    uint32_t mseconds = FileControl[CurrentFile].ElapsedPlayTimeMS;
    uint32_t msecondsTotal;
    if (__builtin_mul_overflow(FileControl[CurrentFile].FrameStepTimeMS, FileControl[CurrentFile].TotalNumberOfFramesInSequence, &msecondsTotal)) {
        // DEBUG_V("returned non-zero: there has been an overflow");
        msecondsTotal = MilliSecondsInASecond; // set to one second total when overflow occurs
    }

    // JsonStatus uses seconds to report elapsed, played, and remaining time
    uint32_t secs = mseconds / MilliSecondsInASecond;
    uint32_t secsTot = msecondsTotal / MilliSecondsInASecond;
    uint32_t secsRem;
    if (__builtin_sub_overflow(secsTot, secs, &secsRem)) {
        // DEBUG_V("returned non-zero: there has been an overflow");
        secsRem = 0; // set to zero remaining seconds when overflow occurs
    }

    JsonWrite(JsonStatus, F ("SyncCount"),           SyncControl.SyncCount);
    JsonWrite(JsonStatus, F ("SyncAdjustmentCount"), SyncControl.SyncAdjustmentCount);

    String temp = GetFileName ();

    JsonWrite(JsonStatus, CN_current_sequence,  temp);
    JsonWrite(JsonStatus, CN_playlist,          temp);
    JsonWrite(JsonStatus, CN_seconds_elapsed,   String (secs));
    JsonWrite(JsonStatus, CN_seconds_played,    String (secs));
    JsonWrite(JsonStatus, CN_seconds_remaining, String (secsRem));
    JsonWrite(JsonStatus, CN_sequence_filename, temp);
    JsonWrite(JsonStatus, F("PlayedFileCount"), PlayedFileCount);

    // After inserting the total seconds and total seconds remaining,
    // JsonStatus also includes formatted "minutes + seconds" for both
    // time Elapsed and time Remaining
    char buf[12];
    ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(buf, secs));
    JsonWrite(JsonStatus, CN_time_elapsed,   buf);
    ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(buf, secsRem));
    JsonWrite(JsonStatus, CN_time_remaining, buf);
    JsonWrite(JsonStatus, CN_errors,         LastFailedPlayStatusMsg);

    // xDEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::UpdateElapsedPlayTimeMS ()
{
    // noInterrupts ();

    uint32_t now = millis ();
    uint32_t elapsedMS = now - FileControl[CurrentFile].LastPollTimeMS;
    if (now < FileControl[CurrentFile].LastPollTimeMS)
    {
        // handle wrap
        elapsedMS = (0 - FileControl[CurrentFile].LastPollTimeMS) + now;
    }

    FileControl[CurrentFile].LastPollTimeMS = now;
    FileControl[CurrentFile].ElapsedPlayTimeMS += elapsedMS;

    // interrupts ();
} // UpdateElapsedPlayTimeMS

//-----------------------------------------------------------------------------
uint32_t c_InputFPPRemotePlayFile::CalculateFrameId (uint32_t ElapsedMS, int32_t SyncOffsetMS)
{
    // xDEBUG_START;

    uint32_t CurrentFrameId = FileControl[CurrentFile].LastPlayedFrameId;

    do // once
    {
        uint32_t AdjustedPlayTime = ElapsedMS + SyncOffsetMS;
        // xDEBUG_V (String ("AdjustedPlayTime: ") + String (AdjustedPlayTime));
        if ((0 > SyncOffsetMS) && (ElapsedMS < abs(SyncOffsetMS)))
        {
            CurrentFrameId = 0;
            // DEBUG_V (String ("SyncOffsetMS: ") + String (SyncOffsetMS));
            // DEBUG_V (String ("   ElapsedMS: ") + String (ElapsedMS));
            break;
        }

        CurrentFrameId = (AdjustedPlayTime / FileControl[CurrentFile].FrameStepTimeMS);
        // xDEBUG_V (String ("  CurrentFrameId: ") + String (CurrentFrameId));
#ifdef DEBUG_FSEQ
        if(2 < abs(int(CurrentFrameId) - int(FileControl[CurrentFile].LastPlayedFrameId)))
        {
            // DEBUG_V (String ("  CurrentFrameId: ") + String (CurrentFrameId));
            // DEBUG_V (String (" PreviousFrameId: ") + String (FileControl[CurrentFile].LastPlayedFrameId));
            // DEBUG_V (String ("     Frame Delta: ") + String(abs(int(CurrentFrameId) - int(FileControl[CurrentFile].LastPlayedFrameId))));
        }
        if(CurrentFrameId < FileControl[CurrentFile].LastPlayedFrameId)
        {
            // DEBUG_V (String ("  CurrentFrameId: ") + String (CurrentFrameId));
            // DEBUG_V (String (" PreviousFrameId: ") + String (FileControl[CurrentFile].LastPlayedFrameId));
        }
#endif // def DEBUG_FSEQ

    } while (false);

    // xDEBUG_END;

    return CurrentFrameId;

} // CalculateFrameId

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayFile::ParseFseqFile ()
{
    // DEBUG_START;
    bool Response = false;

    do // once
    {
        FSEQRawHeader    fsqRawHeader;
        FSEQParsedHeader fsqParsedHeader;

        if(c_FileMgr::INVALID_FILE_HANDLE != FileControl[CurrentFile].FileHandleForFileBeingPlayed)
        {
            FileMgr.CloseSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed);
        }

        if(FileControl[CurrentFile].FileName.isEmpty() ||
           LastFailedFilename.equals(FileControl[CurrentFile].FileName))
        {
            // just go away. Not a valid filename
            break;
        }

        if (false == FileMgr.OpenSdFile (FileControl[CurrentFile].FileName,
                                         c_FileMgr::FileMode::FileRead,
                                         FileControl[CurrentFile].FileHandleForFileBeingPlayed, -1))
        {
            if(!LastFailedFilename.equals(FileControl[CurrentFile].FileName))
            {
                // only output the message once
                LastFailedPlayStatusMsg = String (F ("ParseFseqFile:: Could not open file: filename: '")) + FileControl[CurrentFile].FileName + "'";
                LastFailedFilename = FileControl[CurrentFile].FileName;
                logcon (LastFailedPlayStatusMsg);
            }
            break;
        }

        // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (FileControl[CurrentFile].FileHandleForFileBeingPlayed));
        uint32_t BytesRead = FileMgr.ReadSdFile (FileControl[CurrentFile].FileHandleForFileBeingPlayed,
                                               (uint8_t*)&fsqRawHeader,
                                               sizeof (fsqRawHeader), size_t(0));
        // DEBUG_V (String ("                    BytesRead: ") + String (BytesRead));
        // DEBUG_V (String ("        sizeof (fsqRawHeader): ") + String (sizeof (fsqRawHeader)));

        if (BytesRead != sizeof (fsqRawHeader))
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not read FSEQ header: filename: '")) + FileControl[CurrentFile].FileName + "'");
            logcon (LastFailedPlayStatusMsg);
            FileMgr.CloseSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed);
            break;
        }

        // DEBUG_V ("Convert Raw Header into a processed header");
        memcpy (fsqParsedHeader.header, fsqRawHeader.header, sizeof (fsqParsedHeader.header));
        fsqParsedHeader.dataOffset                    = read16 (fsqRawHeader.dataOffset);
        fsqParsedHeader.minorVersion                  = fsqRawHeader.minorVersion;
        fsqParsedHeader.majorVersion                  = fsqRawHeader.majorVersion;
        fsqParsedHeader.VariableHdrOffset             = read16 (fsqRawHeader.VariableHdrOffset);
        fsqParsedHeader.channelCount                  = read32 (fsqRawHeader.channelCount, 0);
        fsqParsedHeader.TotalNumberOfFramesInSequence = read32 (fsqRawHeader.TotalNumberOfFramesInSequence, 0);
        fsqParsedHeader.stepTime                      = fsqRawHeader.stepTime;
        fsqParsedHeader.flags                         = fsqRawHeader.flags;
        fsqParsedHeader.compressionType               = fsqRawHeader.compressionType;
        fsqParsedHeader.numCompressedBlocks           = fsqRawHeader.numCompressedBlocks;
        fsqParsedHeader.numSparseRanges               = fsqRawHeader.numSparseRanges;
        fsqParsedHeader.flags2                        = fsqRawHeader.flags2;
        fsqParsedHeader.id                            = read64 (fsqRawHeader.id, 0);

// #define DUMP_FSEQ_HEADER
#ifdef DUMP_FSEQ_HEADER
        // DEBUG_V (String ("                   dataOffset: ") + String (fsqParsedHeader.dataOffset));
        // DEBUG_V (String ("                 minorVersion: ") + String (fsqParsedHeader.minorVersion));
        // DEBUG_V (String ("                 majorVersion: ") + String (fsqParsedHeader.majorVersion));
        // DEBUG_V (String ("            VariableHdrOffset: ") + String (fsqParsedHeader.VariableHdrOffset));
        // DEBUG_V (String ("                 channelCount: ") + String (fsqParsedHeader.channelCount));
        // DEBUG_V (String ("TotalNumberOfFramesInSequence: ") + String (fsqParsedHeader.TotalNumberOfFramesInSequence));
        // DEBUG_V (String ("                     stepTime: ") + String (fsqParsedHeader.stepTime));
        // DEBUG_V (String ("                        flags: ") + String (fsqParsedHeader.flags));
        // DEBUG_V (String ("              compressionType: 0x") + String (fsqParsedHeader.compressionType, HEX));
        // DEBUG_V (String ("          numCompressedBlocks: ") + String (fsqParsedHeader.numCompressedBlocks));
        // DEBUG_V (String ("              numSparseRanges: ") + String (fsqParsedHeader.numSparseRanges));
        // DEBUG_V (String ("                       flags2: ") + String (fsqParsedHeader.flags2));
        // DEBUG_V (String ("                           id: 0x") + String ((unsigned long)fsqParsedHeader.id, HEX));
#endif // def DUMP_FSEQ_HEADER

        if (fsqParsedHeader.majorVersion != 2 || fsqParsedHeader.compressionType != 0)
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not start. ")) + FileControl[CurrentFile].FileName + F (" is not a v2 uncompressed sequence"));
            logcon (LastFailedPlayStatusMsg);
            FileMgr.CloseSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed);
            break;
        }
        // DEBUG_V ("");
        size_t ActualDataSize = FileMgr.GetSdFileSize (FileControl[CurrentFile].FileHandleForFileBeingPlayed) - sizeof(fsqParsedHeader);
        size_t NeededDataSize = fsqParsedHeader.TotalNumberOfFramesInSequence * fsqParsedHeader.channelCount;
        // DEBUG_V("NeededDataSize: " + String(NeededDataSize));
        // DEBUG_V("ActualDataSize: " + String(ActualDataSize));
        if (NeededDataSize > ActualDataSize)
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not start: ")) + FileControl[CurrentFile].FileName +
                                      F (" File does not contain enough data to meet the Stated Channel Count * Number of Frames value. Need: ") +
                                      String (NeededDataSize) + F (", SD File Size: ") + String (ActualDataSize));
            logcon (LastFailedPlayStatusMsg);
            FileMgr.CloseSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed);
            break;
        }

        FileControl[CurrentFile].FrameStepTimeMS = max ((uint8_t)25, fsqParsedHeader.stepTime);
        FileControl[CurrentFile].TotalNumberOfFramesInSequence = fsqParsedHeader.TotalNumberOfFramesInSequence;

        FileControl[CurrentFile].DataOffset = fsqParsedHeader.dataOffset;
        FileControl[CurrentFile].ChannelsPerFrame = fsqParsedHeader.channelCount;

        memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
        if (fsqParsedHeader.numSparseRanges)
        {
            if (MAX_NUM_SPARSE_RANGES < fsqParsedHeader.numSparseRanges)
            {
                LastFailedPlayStatusMsg =  (String (F ("ParseFseqFile:: Could not start. ")) + FileControl[CurrentFile].FileName + F (" Too many sparse ranges defined in file header."));
                logcon (LastFailedPlayStatusMsg);
                FileMgr.CloseSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed);
                break;
            }

            FSEQRawRangeEntry FseqRawRanges[MAX_NUM_SPARSE_RANGES];

            FileMgr.ReadSdFile (FileControl[CurrentFile].FileHandleForFileBeingPlayed,
                                (uint8_t*)&FseqRawRanges[0],
                                sizeof (FseqRawRanges),
                                sizeof (FSEQRawHeader) + fsqParsedHeader.numCompressedBlocks * 8);

            uint32_t SparseRangeIndex = 0;
            uint32_t TotalChannels = 0;
            uint32_t LargestBlock = 0;
            for (auto & CurrentSparseRange : SparseRanges)
            {
                // DEBUG_V (String ("           Sparse Range Index: ") + String (SparseRangeIndex));
                if (SparseRangeIndex >= fsqParsedHeader.numSparseRanges)
                {
                    // DEBUG_V (String ("No Sparse Range Data for this entry "));
                    ++SparseRangeIndex;
                    continue;
                }

                // CurrentSparseRange.DataOffset = read24 (FseqRawRanges[SparseRangeIndex].Start);
                CurrentSparseRange.DataOffset = TotalChannels;
                CurrentSparseRange.ChannelCount = read24 (FseqRawRanges[SparseRangeIndex].Length);
                TotalChannels += CurrentSparseRange.ChannelCount;
                LargestBlock  = max (LargestBlock, CurrentSparseRange.DataOffset + CurrentSparseRange.ChannelCount);

#ifdef DUMP_FSEQ_HEADER
                // DEBUG_V (String ("            RangeChannelCount: ") + String (CurrentSparseRange.ChannelCount));
                // DEBUG_V (String ("              RangeDataOffset: 0x") + String (CurrentSparseRange.DataOffset, HEX));
#endif // def DUMP_FSEQ_HEADER

                ++SparseRangeIndex;
            }

#ifdef DUMP_FSEQ_HEADER
            // DEBUG_V (String ("                TotalChannels: ") + String (TotalChannels));
            // DEBUG_V (String ("                 LargestBlock: ") + String (LargestBlock));
#endif // def DUMP_FSEQ_HEADER
            if (0 == TotalChannels)
            {
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + FileControl[CurrentFile].FileName + F (" No channels defined in Sparse Ranges."));
                logcon (LastFailedPlayStatusMsg);
                memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
                SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
            }

            else if (TotalChannels > fsqParsedHeader.channelCount)
            {
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + FileControl[CurrentFile].FileName + F (" Too many channels defined in Sparse Ranges."));
                logcon (LastFailedPlayStatusMsg);
                memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
                SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
            }

            else if (LargestBlock > fsqParsedHeader.channelCount)
            {
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + FileControl[CurrentFile].FileName + F (" Sparse Range Frame offset + Num channels is larger than frame size."));
                logcon (LastFailedPlayStatusMsg);
                memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
                SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
            }
        }
        else
        {
            memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
            SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
        }

        PlayedFileCount++;
        Response = true;

    } while (false);

    // Caller must close the file since it is used to play the channel data.

    // DEBUG_END;

    return Response;

} // ParseFseqFile

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::ClearFileInfo()
{
    // DEBUG_START;
    FileControl[NextFile].FileName                      = emptyString;
    FileControl[NextFile].FileHandleForFileBeingPlayed  = c_FileMgr::INVALID_FILE_HANDLE;
    FileControl[NextFile].RemainingPlayCount            = 0;
    SyncControl.LastRcvdElapsedSeconds                  = 0.0;
    FileControl[NextFile].ElapsedPlayTimeMS             = 0;
    FileControl[NextFile].DataOffset                    = 0;
    FileControl[NextFile].ChannelsPerFrame              = 0;
    FileControl[NextFile].FrameStepTimeMS               = 0;
    FileControl[NextFile].TotalNumberOfFramesInSequence = 0;
    // DEBUG_END;
} // ClearFileInfo

#ifdef DEBUG_FSEQ
uint32_t NextFileOffset = 0;
uint32_t NextChannelOffset = 0;
size_t LastStartingPosition = 0;
size_t NextExpectedStartingPosition = 0;
uint32_t LocalIntensityBufferIndex = 0;
uint32_t PreviousOnPosition = 0;
uint32_t PreviousFileOffset = 0;
#endif // def DEBUG_FSEQ

//-----------------------------------------------------------------------------
uint64_t c_InputFPPRemotePlayFile::ReadFile(uint64_t DestinationIntensityId, uint64_t NumBytesToRead, uint64_t FileOffset)
{
    // xDEBUG_START;

    uint64_t NumBytesRead = 0;

    do // once
    {
        if(c_FileMgr::INVALID_FILE_HANDLE == FileControl[CurrentFile].FileHandleForFileBeingPlayed)
        {
            // DEBUG_V("No Valid File Handle");
            break;
        }

        if(nullptr == LocalIntensityBuffer)
        {
            LocalIntensityBuffer = (byte*)malloc(LocalIntensityBufferSize);
            // DEBUG_V("Allocating local buffer.");
            if(nullptr == LocalIntensityBuffer)
            {
                logcon(String(CN_stars) + F("Could not allocate a buffer to read SD files. Rebooting. ") + CN_stars);
                RequestReboot(1, true);
                break;
            }
        }
        while (NumBytesRead < NumBytesToRead)
        {
            // xDEBUG_V();
            uint64_t NumBytesReadThisPass = FileMgr.ReadSdFile(FileControl[CurrentFile].FileHandleForFileBeingPlayed,
                                                               LocalIntensityBuffer,
                                                               min((NumBytesToRead - NumBytesRead), LocalIntensityBufferSize),
                                                               FileOffset);
            // xDEBUG_V();
#ifdef DEBUG_FSEQ
/*
            uint32_t nonZeroCount = 0;
            for(uint32_t index = 0; index < NumBytesReadThisPass; ++index)
            {
                if(LocalIntensityBuffer[index] != 0)
                {
                    ++ nonZeroCount;
                }
                if(LocalIntensityBuffer[index+1] != 0)
                {
                    ++ index;
                }
            }
            if(1 < nonZeroCount)
            {
                // DEBUG_V(String("Found too many non zero bytes: ") + String(nonZeroCount));
            }
            if(0 != DestinationIntensityId)
            {
                // DEBUG_V(String("Unexpected DestinationIntensityId: ") + String(DestinationIntensityId));
            }
            if(130 != NumBytesReadThisPass)
            {
                // DEBUG_V(String("Unexpected NumBytesReadThisPass: ") + String(NumBytesReadThisPass));
            }
            if(0 != abs(int(FileOffset) - int(NextFileOffset)))
            {
                // DEBUG_V(String("Unexpected FileOffset: ") + String(FileOffset));
                // DEBUG_V(String("       NextFileOffset: ") + String(NextFileOffset));
                // DEBUG_V(String("                delta: ") + String(abs(int(FileOffset) - int(NextFileOffset))));
            }
            NextFileOffset = FileOffset + NumBytesReadThisPass;
            if(FileOffset < NextExpectedStartingPosition)
            {
                // DEBUG_V("Unexpected Start Position");
                // DEBUG_V (String ("NextExpectedStartingPosition: ") + String (NextExpectedStartingPosition));
                // DEBUG_V (String ("     Actual StartingPosition: ") + String (FileOffset));
            }
            NextExpectedStartingPosition = FileOffset + NumBytesToRead;
            if(FileOffset <= LastStartingPosition)
            {
                // DEBUG_V("Unexpected Start Position");
                // DEBUG_V (String ("   LastStartingPosition: ") + String (LastStartingPosition));
                // DEBUG_V (String ("Actual StartingPosition: ") + String (FileOffset));
            }
            LastStartingPosition = FileOffset;
            memset(LocalIntensityBuffer, 0x00, LocalIntensityBufferSize);
            LocalIntensityBuffer[LocalIntensityBufferIndex >> 2] = 255;
            ++LocalIntensityBufferIndex;
            if((LocalIntensityBufferSize - 1) < (LocalIntensityBufferIndex >> 2))
            {
                LocalIntensityBufferIndex = 0;
            }
*/
            uint32_t CurrentOnPosition = 0;
            for(uint32_t index = 0; index < LocalIntensityBufferSize; ++index)
            {
                if(0 != LocalIntensityBuffer[index])
                {
                    CurrentOnPosition = index;
                    break;
                }
            }
            if((CurrentOnPosition == PreviousOnPosition) ||
               (CurrentOnPosition == PreviousOnPosition + 1))
            {
                // found what we are looking for
            }
            else
            {
                // DEBUG_V(String("PreviousOnPosition: ")   + String(PreviousOnPosition));
                // DEBUG_V(String(" CurrentOnPosition: ")   + String(CurrentOnPosition));
                // DEBUG_V(String("             delta: ")   + String(abs(int(CurrentOnPosition) - int(PreviousOnPosition))));
                // DEBUG_V(String("PreviousFileOffset: 0x") + String(PreviousFileOffset, HEX));
                // DEBUG_V(String("        FileOffset: 0x") + String(FileOffset, HEX));
                // DEBUG_V(String("       deltaOffset: ")   + String(abs(int(FileOffset) - int(PreviousFileOffset))));
            }
            PreviousOnPosition = CurrentOnPosition;
            PreviousFileOffset = FileOffset;
#endif // def DEBUG_FSEQ

            OutputMgr.WriteChannelData(DestinationIntensityId, NumBytesReadThisPass, LocalIntensityBuffer);

            FileOffset += NumBytesReadThisPass;
            NumBytesRead += NumBytesReadThisPass;
            DestinationIntensityId += NumBytesReadThisPass;
        }
    } while (false);
    // xDEBUG_V(String("NumBytesToRead: ") + String(NumBytesToRead));
    // xDEBUG_V(String("  NumBytesRead: ") + String(NumBytesRead));

    // xDEBUG_END;
    return NumBytesRead;
} // ReadFile
