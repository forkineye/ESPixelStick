#pragma once
/*
* InputFPPRemotePlayFile.hpp
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

#include "../ESPixelStick.h"
#include "InputFPPRemotePlayItem.hpp"
#include "InputFPPRemotePlayFileFsm.hpp"
#include "../service/fseq.h"

class c_InputFPPRemotePlayFile : public c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayFile (c_InputMgr::e_InputChannelIds InputChannelId);
    ~c_InputFPPRemotePlayFile ();

    virtual void Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount);
    virtual void Stop ();
    virtual void Sync (String& FileName, uint32_t FrameId);
    virtual void Poll (uint8_t* Buffer, size_t BufferSize);
    virtual void GetStatus (JsonObject & jsonStatus);
    virtual bool IsIdle () { return (pCurrentFsmState == &fsm_PlayFile_state_Idle_imp); }
    
    uint32_t GetLastFrameId ()  { return LastPlayedFrameId; }
    float    GetTimeCorrectionFactor () { return TimeCorrectionFactor; }

private:
    friend class fsm_PlayFile_state_Idle;
    friend class fsm_PlayFile_state_Starting;
    friend class fsm_PlayFile_state_PlayingFile;
    friend class fsm_PlayFile_state_Stopping;
    friend class fsm_PlayFile_state;

    fsm_PlayFile_state_Idle        fsm_PlayFile_state_Idle_imp;
    fsm_PlayFile_state_Starting    fsm_PlayFile_state_Starting_imp;
    fsm_PlayFile_state_PlayingFile fsm_PlayFile_state_PlayingFile_imp;
    fsm_PlayFile_state_Stopping    fsm_PlayFile_state_Stopping_imp;

    fsm_PlayFile_state * pCurrentFsmState = &fsm_PlayFile_state_Idle_imp;
    
    c_FileMgr::FileId FileHandleForFileBeingPlayed = 0;
    uint32_t          LastPlayedFrameId = 0;
    uint32_t          LastRcvdSyncFrameId = 0;
    size_t            DataOffset = 0;
    uint32_t          ChannelsPerFrame = 0;
    time_t            FrameStepTimeMS = 1;
    uint32_t          TotalNumberOfFramesInSequence = 0;
    uint32_t          StartTimeMS = 0;
    float             TimeCorrectionFactor = 1.0;
    uint32_t          SyncCount = 0;
    uint32_t          SyncAdjustmentCount = 0;

#define MAX_NUM_SPARSE_RANGES 5
    FSEQParsedRangeEntry SparseRanges[MAX_NUM_SPARSE_RANGES];

    uint32_t          CalculateFrameId (time_t now, int32_t SyncOffsetMS = 0);
    void              CalculatePlayStartTime ();
    bool              ParseFseqFile ();

    String            LastFailedPlayStatusMsg;

#define TimeOffsetStep 0.00001

}; // c_InputFPPRemotePlayFile
