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
#include "../service/fseq.h"

static void TimerPollHandler (void * p)
{
    reinterpret_cast<c_InputFPPRemotePlayFile*>(p)->TimerPoll ();

}// TimerPollHandler

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::c_InputFPPRemotePlayFile (c_InputMgr::e_InputChannelIds InputChannelId) :
    c_InputFPPRemotePlayItem (InputChannelId)
{
    // DEBUG_START;

    fsm_PlayFile_state_Idle_imp.Init (this);

    LastIsrTimeStampMS = millis ();
    MsTicker.attach_ms (uint32_t (25), &TimerPollHandler, (void*)this); // Add ISR Function

    // DEBUG_END;
} // c_InputFPPRemotePlayFile

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayFile::~c_InputFPPRemotePlayFile ()
{
    // DEBUG_START;
    MsTicker.detach ();
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

    SyncControl.SyncCount++;
    if (pCurrentFsmState->Sync (FileName, FrameId))
    {
        SyncControl.SyncAdjustmentCount++;
    }

    // DEBUG_END;

} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::Poll (uint8_t * _Buffer, size_t _BufferSize)
{
    // xDEBUG_START;

    Buffer = _Buffer;
    BufferSize = _BufferSize;

    InitTimeCorrectionFactor ();
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

        uint32_t now = millis ();
        uint32_t elapsedMS = now - LastIsrTimeStampMS;
        if (now < LastIsrTimeStampMS)
        {
            // handle wrap
            elapsedMS = (0 - LastIsrTimeStampMS) + now;
        }

        LastIsrTimeStampMS = now;
        FrameControl.ElapsedPlayTimeMS += elapsedMS;
        pCurrentFsmState->TimerPoll ();
    }
    // xDEBUG_END;

} // TimerPoll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::GetStatus (JsonObject& JsonStatus)
{
    // xDEBUG_START;

    // uint32_t mseconds = millis () - StartTimeMS;
    time_t   AdjustedFrameStepTimeMS = time_t (float (FrameControl.FrameStepTimeMS) * SyncControl.TimeCorrectionFactor);
    uint32_t mseconds = AdjustedFrameStepTimeMS * FrameControl.LastPlayedFrameId;
    uint32_t msecondsTotal = FrameControl.FrameStepTimeMS * FrameControl.TotalNumberOfFramesInSequence;

    uint32_t secs = mseconds / 1000;
    uint32_t secsTot = msecondsTotal / 1000;

    JsonStatus[F ("SyncCount")]           = SyncControl.SyncCount;
    JsonStatus[F ("SyncAdjustmentCount")] = SyncControl.SyncAdjustmentCount;
    JsonStatus[F ("TimeOffset")]          = SyncControl.TimeCorrectionFactor;

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

    JsonStatus[CN_errors] = LastFailedPlayStatusMsg;

    // xDEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
uint32_t c_InputFPPRemotePlayFile::CalculateFrameId (int32_t SyncOffsetMS)
{
    // DEBUG_START;

    uint32_t CurrentFrameId = 0;

    do // once
    {
        if (FrameControl.ElapsedPlayTimeMS < FrameControl.FrameStepTimeMS)
        {
            break;
        }

        uint32_t AdjustedPlayTime = FrameControl.ElapsedPlayTimeMS + SyncOffsetMS;
        // DEBUG_V (String ("AdjustedPlayTime: ") + String (AdjustedPlayTime));
        if ((0 > SyncOffsetMS) && (FrameControl.ElapsedPlayTimeMS < abs(SyncOffsetMS)))
        {
            break;
        }

        CurrentFrameId = (AdjustedPlayTime / SyncControl.AdjustedFrameStepTimeMS);
        // DEBUG_V (String ("  CurrentFrameId: ") + String (CurrentFrameId));

    } while (false);

    // DEBUG_END;

    return CurrentFrameId;

} // CalculateFrameId

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::CalculatePlayStartTime (uint32_t StartingFrameId)
{
    // DEBUG_START;

    FrameControl.ElapsedPlayTimeMS = uint32_t ((float (FrameControl.FrameStepTimeMS) * SyncControl.TimeCorrectionFactor) * float (StartingFrameId));
    
    // DEBUG_V (String ("     FrameStepTimeMS: ") + String (FrameControl.FrameStepTimeMS));
    // DEBUG_V (String ("TimeCorrectionFactor: ") + String (SyncControl.TimeCorrectionFactor));
    // DEBUG_V (String ("     StartingFrameId: ") + String (StartingFrameId));
    // DEBUG_V (String ("     ElapsedPlayTIme: ") + String (FrameControl.ElapsedPlayTimeMS));

    // DEBUG_END;

} // CalculatePlayStartTime

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
        SyncControl.AdjustedFrameStepTimeMS = uint32_t (float (FrameControl.FrameStepTimeMS) * SyncControl.TimeCorrectionFactor);

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
            // DEBUG_V (String ("                LargestOffset: ") + String (LargestOffset));
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

        Response = true;

    } while (false);

    // Caller must close the file since it is used to play the channel data.

    // DEBUG_END;

    return Response;

} // ParseFseqFile

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::InitTimeCorrectionFactor ()
{
    // DEBUG_START;

    do // once
    {
        if (INVALID_TIME_CORRECTION_FACTOR != SyncControl.SavedTimeCorrectionFactor)
        {
            break;
        }

        // only do this once after boot.
        SyncControl.TimeCorrectionFactor = SyncControl.SavedTimeCorrectionFactor = INITIAL_TIME_CORRECTION_FACTOR;

        String FileData;
        FileMgr.ReadSdFile (String (CN_time), FileData);

        if (0 == FileData.length ())
        {
            // DEBUG_V ("No data in file");
            break;
        }

        // DEBUG_V (String ("FileData: ") + FileData);

        DynamicJsonDocument jsonDoc (64);
        DeserializationError error = deserializeJson (jsonDoc, FileData);
        if (error)
        {
            logcon (CN_Heap_colon + String (ESP.getFreeHeap ()));
            logcon (String (F ("Time Factor Deserialzation Error. Error code = ")) + error.c_str ());
        }
        // DEBUG_V ("");

        JsonObject JsonData = jsonDoc.as<JsonObject> ();

        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
        // PrettyPrint (JsonData, String ("InitTimeCorrectionFactor"));

        setFromJSON (SyncControl.TimeCorrectionFactor, JsonData, CN_time);
        SyncControl.SavedTimeCorrectionFactor = SyncControl.TimeCorrectionFactor;
        // DEBUG_V (String ("TimeCorrectionFactor: ") + String (TimeCorrectionFactor, 10));
    
    } while (false);

    // DEBUG_END;

} // InitTimeCorrectionFactor

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::SaveTimeCorrectionFactor ()
{
    // DEBUG_START;

    do // once
    {
        if (fabs (SyncControl.SavedTimeCorrectionFactor - SyncControl.TimeCorrectionFactor) < 0.000005F )
        {
            // DEBUG_V ("Nothing to save");
            break;
        }

        DynamicJsonDocument jsonDoc (64);
        JsonObject JsonData = jsonDoc.createNestedObject ("x");
        // JsonObject JsonData = jsonDoc.as<JsonObject> ();

        JsonData[CN_time] = SyncControl.TimeCorrectionFactor;
        SyncControl.SavedTimeCorrectionFactor = SyncControl.TimeCorrectionFactor;

        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
        // PrettyPrint (JsonData, String ("SaveTimeCorrectionFactor"));

        String JsonFileData;
        serializeJson (JsonData, JsonFileData);
        // DEBUG_V (String ("JsonFileData: ") + JsonFileData);
        FileMgr.SaveSdFile (String(CN_time), JsonFileData);

    } while (false);
    
    // DEBUG_END;

} // SaveTimeCorrectionFactor

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayFile::ClearFileInfo()
{
    PlayItemName                               = String ("");
    FrameControl.StartingFrameId               = 0;
    FrameControl.LastPlayedFrameId             = 0;
    RemainingPlayCount                         = 0;
    SyncControl.LastRcvdSyncFrameId            = 0;
    FrameControl.DataOffset                    = 0;
    FrameControl.ChannelsPerFrame              = 0;
    FrameControl.FrameStepTimeMS               = 25;
    FrameControl.TotalNumberOfFramesInSequence = 0;
    FrameControl.ElapsedPlayTimeMS             = 0;
} // ClearFileInfo
