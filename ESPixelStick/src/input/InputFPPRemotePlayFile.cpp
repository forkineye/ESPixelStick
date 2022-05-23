/*
* InputFPPRemotePlayFile.cpp
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
*   PlayFile object used to parse and play an FSEQ File
*/

#include "InputFPPRemotePlayFile.hpp"
#include "../service/FPPDiscovery.h"
#include "../service/fseq.h"

//-----------------------------------------------------------------------------
static void TimerPollHandler (void * p)
{
#ifdef ARDUINO_ARCH_ESP32
    TaskHandle_t Handle = reinterpret_cast <c_InputFPPRemotePlayFile*> (p)->GetTaskHandle ();
    if (Handle)
    {
        vTaskResume (Handle);
    }
#else
    reinterpret_cast<c_InputFPPRemotePlayFile*>(p)->TimerPoll ();
#endif // def ARDUINO_ARCH_ESP32

} // TimerPollHandler

#ifdef ARDUINO_ARCH_ESP32
//----------------------------------------------------------------------------
static void TimerPollHandlerTask (void* pvParameters)
{
    // DEBUG_START; // Need extra stack space to run this
    c_InputFPPRemotePlayFile* InputFpp = reinterpret_cast <c_InputFPPRemotePlayFile*> (pvParameters);

    do
    {
        // we start suspended
        vTaskSuspend (NULL); //Suspend Own Task
        // DEBUG_V ("");
        InputFpp->TimerPoll ();

    } while (true);
    // DEBUG_END;

} // TimerPollHandlerTask
#endif // def ARDUINO_ARCH_ESP32

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::c_InputFPPRemotePlayFile (c_InputMgr::e_InputChannelIds InputChannelId) :
    c_InputFPPRemotePlayItem (InputChannelId)
{
    // DEBUG_START;

    fsm_PlayFile_state_Idle_imp.Init (this);

    LastIsrTimeStampMS = millis ();
    MsTicker.attach_ms (uint32_t (FPP_TICKER_PERIOD_MS), &TimerPollHandler, (void*)this); // Add ISR Function

#ifdef ARDUINO_ARCH_ESP32
    xTaskCreate (TimerPollHandlerTask, "FPPTask", TimerPollHandlerTaskStack, this, ESP_TASK_PRIO_MIN + 4, &TimerPollTaskHandle);
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::~c_InputFPPRemotePlayFile ()
{
    // DEBUG_START;
    MsTicker.detach ();

#ifdef ARDUINO_ARCH_ESP32
    if (NULL != TimerPollTaskHandle)
    {
        vTaskDelete (TimerPollTaskHandle);
        TimerPollTaskHandle = NULL;
    }
#endif // def ARDUINO_ARCH_ESP32

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
    if (FileMgr.SdCardIsInstalled ())
    {
        pCurrentFsmState->Start (FileName, SecondsElapsed, PlayCount);
    }
    else
    {
        // DEBUG_V ("No SD Card installed. Ignore Start request");
        fsm_PlayFile_state_Idle_imp.Init (this);
    }

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
void c_InputFPPRemotePlayFile::Sync (String & FileName, float SecondsElapsed)
{
    // DEBUG_START;

    SyncControl.SyncCount++;
    UpdateElapsedPlayTimeMS ();
    if (pCurrentFsmState->Sync (FileName, SecondsElapsed))
    {
        SyncControl.SyncAdjustmentCount++;
    }

    // DEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Poll ()
{
    // xDEBUG_START;

    // TimerPoll ();
    pCurrentFsmState->Poll ();

    // Show that we have received a poll
    PollDetectionCounter = 0;

    // xDEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::TimerPoll ()
{
    // xDEBUG_START;

    // Are polls still coming in?
    if (PollDetectionCounter < PollDetectionCounterLimit)
    {
        PollDetectionCounter++;
        UpdateElapsedPlayTimeMS ();
        pCurrentFsmState->TimerPoll ();
    }
    // xDEBUG_END;

} // TimerPoll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::GetStatus (JsonObject& JsonStatus)
{
    // xDEBUG_START;

    // NOTE:
    // variables ending in "Total" reflect total     play time
    // variables ending in "Rem"   reflect remaining play time

    // File uses milliseconds to store elapsed and total play time
    uint32_t mseconds = FrameControl.ElapsedPlayTimeMS;
    uint32_t msecondsTotal;
    if (__builtin_mul_overflow(FrameControl.FrameStepTimeMS, FrameControl.TotalNumberOfFramesInSequence, &msecondsTotal)) {
        // returned non-zero: there has been an overflow
        msecondsTotal = MilliSecondsInASecond; // set to one second total when overflow occurs
    }

    // JsonStatus uses seconds to report elapsed, played, and remaining time
    uint32_t secs = mseconds / MilliSecondsInASecond;
    uint32_t secsTot = msecondsTotal / MilliSecondsInASecond;
    uint32_t secsRem;
    if (__builtin_sub_overflow(secsTot, secs, &secsRem)) {
        // returned non-zero: there has been an overflow
        secsRem = 0; // set to zero remaining seconds when overflow occurs
    }

    JsonStatus[F ("SyncCount")]           = SyncControl.SyncCount;
    JsonStatus[F ("SyncAdjustmentCount")] = SyncControl.SyncAdjustmentCount;

    String temp = GetFileName ();

    JsonStatus[CN_current_sequence]  = temp;
    JsonStatus[CN_playlist]          = temp;
    JsonStatus[CN_seconds_elapsed]   = String (secs);
    JsonStatus[CN_seconds_played]    = String (secs);
    JsonStatus[CN_seconds_remaining] = String (secsRem);
    JsonStatus[CN_sequence_filename] = temp;
    JsonStatus[F("PlayedFileCount")] = PlayedFileCount;

    // After inserting the total seconds and total seconds remaining,
    // JsonStatus also includes formatted "minutes + seconds" for both
    // timeElapsed and timeRemaining
    char buf[12];
    ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(buf, secs));
    JsonStatus[CN_time_elapsed] = buf;
    ESP_ERROR_CHECK(saferSecondsToFormattedMinutesAndSecondsString(buf, secsRem));
    JsonStatus[CN_time_remaining] = buf;

    JsonStatus[CN_errors] = LastFailedPlayStatusMsg;

    // xDEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::UpdateElapsedPlayTimeMS ()
{
    noInterrupts ();

    uint32_t now = millis ();
    uint32_t elapsedMS = now - LastIsrTimeStampMS;
    if (now < LastIsrTimeStampMS)
    {
        // handle wrap
        elapsedMS = (0 - LastIsrTimeStampMS) + now;
    }

    LastIsrTimeStampMS = now;
    FrameControl.ElapsedPlayTimeMS += elapsedMS;

    interrupts ();
} // UpdateElapsedPlayTimeMS

//-----------------------------------------------------------------------------
uint32_t c_InputFPPRemotePlayFile::CalculateFrameId (uint32_t ElapsedMS, int32_t SyncOffsetMS)
{
    // DEBUG_START;

    uint32_t CurrentFrameId = 0;

    do // once
    {
        uint32_t AdjustedPlayTime = ElapsedMS + SyncOffsetMS;
        // DEBUG_V (String ("AdjustedPlayTime: ") + String (AdjustedPlayTime));
        if ((0 > SyncOffsetMS) && (ElapsedMS < abs(SyncOffsetMS)))
        {
            break;
        }

        CurrentFrameId = (AdjustedPlayTime / FrameControl.FrameStepTimeMS);
        // DEBUG_V (String ("  CurrentFrameId: ") + String (CurrentFrameId));

    } while (false);

    // DEBUG_END;

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

        FileHandleForFileBeingPlayed = -1;
        if (false == FileMgr.OpenSdFile (PlayItemName,
                                         c_FileMgr::FileMode::FileRead,
                                         FileHandleForFileBeingPlayed))
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not open file: filename: '")) + PlayItemName + "'");
            logcon (LastFailedPlayStatusMsg);
            break;
        }

        // DEBUG_V (String ("FileHandleForFileBeingPlayed: ") + String (FileHandleForFileBeingPlayed));
        size_t BytesRead = FileMgr.ReadSdFile (FileHandleForFileBeingPlayed,
                                               (uint8_t*)&fsqRawHeader,
                                               sizeof (fsqRawHeader), 0);
        // DEBUG_V (String ("                    BytesRead: ") + String (BytesRead));
        // DEBUG_V (String ("        sizeof (fsqRawHeader): ") + String (sizeof (fsqRawHeader)));

        if (BytesRead != sizeof (fsqRawHeader))
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not read FSEQ header: filename: '")) + PlayItemName + "'");
            logcon (LastFailedPlayStatusMsg);
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
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not start. ")) + PlayItemName + F (" is not a v2 uncompressed sequence"));
            logcon (LastFailedPlayStatusMsg);
            break;
        }
        // DEBUG_V ("");

        if ((fsqParsedHeader.TotalNumberOfFramesInSequence * fsqParsedHeader.channelCount) > FileMgr.GetSdFileSize (FileHandleForFileBeingPlayed))
        {
            LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Could not start. ")) + PlayItemName + F (" File does not contain enough data to meet the Stated Channel Count * Number of Frames value."));
            logcon (LastFailedPlayStatusMsg);
            break;
        }

        FrameControl.FrameStepTimeMS = max ((uint8_t)25, fsqParsedHeader.stepTime);
        FrameControl.TotalNumberOfFramesInSequence = fsqParsedHeader.TotalNumberOfFramesInSequence;

        FrameControl.DataOffset = fsqParsedHeader.dataOffset;
        FrameControl.ChannelsPerFrame = fsqParsedHeader.channelCount;

        memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
        if (fsqParsedHeader.numSparseRanges)
        {
            if (MAX_NUM_SPARSE_RANGES < fsqParsedHeader.numSparseRanges)
            {
                LastFailedPlayStatusMsg =  (String (F ("ParseFseqFile:: Could not start. ")) + PlayItemName + F (" Too many sparse ranges defined in file header."));
                logcon (LastFailedPlayStatusMsg);
                break;
            }

            FSEQRawRangeEntry FseqRawRanges[MAX_NUM_SPARSE_RANGES];

            FileMgr.ReadSdFile (FileHandleForFileBeingPlayed,
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
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + PlayItemName + F (" No channels defined in Sparse Ranges."));
                logcon (LastFailedPlayStatusMsg);
                memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
                SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
            }

            else if (TotalChannels > fsqParsedHeader.channelCount)
            {
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + PlayItemName + F (" Too many channels defined in Sparse Ranges."));
                logcon (LastFailedPlayStatusMsg);
                memset ((void*)&SparseRanges, 0x00, sizeof (SparseRanges));
                SparseRanges[0].ChannelCount = fsqParsedHeader.channelCount;
            }

            else if (LargestBlock > fsqParsedHeader.channelCount)
            {
                LastFailedPlayStatusMsg = (String (F ("ParseFseqFile:: Ignoring Range Info. ")) + PlayItemName + F (" Sparse Range Frame offset + Num channels is larger than frame size."));
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
    PlayItemName                               = String ("");
    RemainingPlayCount                         = 0;
    SyncControl.LastRcvdElapsedSeconds         = 0.0;
    FrameControl.ElapsedPlayTimeMS             = 0;
    FrameControl.DataOffset                    = 0;
    FrameControl.ChannelsPerFrame              = 0;
    FrameControl.FrameStepTimeMS               = 25;
    FrameControl.TotalNumberOfFramesInSequence = 0;

} // ClearFileInfo

size_t c_InputFPPRemotePlayFile::ReadFile(size_t DestinationIntensityId, size_t NumBytesToRead, size_t FileOffset)
{
    // DEBUG_START;
#define WRITE_DIRECT_TO_OUTPUT_BUFFER
#ifdef WRITE_DIRECT_TO_OUTPUT_BUFFER
    size_t NumBytesRead = FileMgr.ReadSdFile(FileHandleForFileBeingPlayed,
                                             OutputMgr.GetBufferAddress(),
                                             min((NumBytesToRead), OutputMgr.GetBufferUsedSize()),
                                             FileOffset);
#else
    uint8_t LocalIntensityBuffer[200];

    size_t NumBytesRead = 0;

    while (NumBytesRead < NumBytesToRead)
    {
        size_t NumBytesReadThisPass = FileMgr.ReadSdFile(FileHandleForFileBeingPlayed,
                                                         LocalIntensityBuffer,
                                                         min((NumBytesToRead - NumBytesRead), sizeof(LocalIntensityBuffer)),
                                                         FileOffset);

        OutputMgr.WriteChannelData(DestinationIntensityId, NumBytesReadThisPass, LocalIntensityBuffer);

        FileOffset += NumBytesReadThisPass;
        NumBytesRead += NumBytesReadThisPass;
        DestinationIntensityId += NumBytesReadThisPass;
    }
#endif // !def WRITE_DIRECT_TO_OUTPUT_BUFFER

    // DEBUG_END;
    return NumBytesRead;
} // ReadFile
