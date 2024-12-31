#pragma once
/*
* InputFPPRemotePlayFile.hpp
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

#include "ESPixelStick.h"
#include "InputFPPRemotePlayItem.hpp"
#include "InputFPPRemotePlayFileFsm.hpp"
#include "service/fseq.h"
#include <Ticker.h>

#ifdef ARDUINO_ARCH_ESP32
#include <esp_task.h>
#endif // def ARDUINO_ARCH_ESP32


class c_InputFPPRemotePlayFile : public c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayFile (c_InputMgr::e_InputChannelIds InputChannelId);
    virtual ~c_InputFPPRemotePlayFile ();

    virtual void Start (String & FileName, float SecondsElapsed, uint32_t RemainingPlayCount);
    virtual void Stop ();
    virtual void Sync (String& FileName, float SecondsElapsed);
    virtual bool Poll ();
    virtual void GetStatus (JsonObject & jsonStatus);
    virtual bool IsIdle () { return (pCurrentFsmState == &fsm_PlayFile_state_Idle_imp); }

    void TimerPoll ();
#ifdef ARDUINO_ARCH_ESP32
    TaskHandle_t GetTaskHandle () { return TimerPollTaskHandle; }
    volatile bool TimerPollInProgress = false;
#endif // def ARDUINO_ARCH_ESP32

private:
#define ELAPSED_PLAY_TIMER_INTERVAL_MS  10

    void ClearFileInfo            ();

    friend class fsm_PlayFile_state_Idle;
    friend class fsm_PlayFile_state_Starting;
    friend class fsm_PlayFile_state_PlayingFile;
    friend class fsm_PlayFile_state_Stopping;
    friend class fsm_PlayFile_state_Error;
    friend class fsm_PlayFile_state;

    fsm_PlayFile_state_Idle        fsm_PlayFile_state_Idle_imp;
    fsm_PlayFile_state_Starting    fsm_PlayFile_state_Starting_imp;
    fsm_PlayFile_state_PlayingFile fsm_PlayFile_state_PlayingFile_imp;
    fsm_PlayFile_state_Stopping    fsm_PlayFile_state_Stopping_imp;
    fsm_PlayFile_state_Error       fsm_PlayFile_state_Error_imp;

    fsm_PlayFile_state * pCurrentFsmState = &fsm_PlayFile_state_Idle_imp;

    c_FileMgr::FileId FileHandleForFileBeingPlayed = c_FileMgr::INVALID_FILE_HANDLE;

    struct FrameControl_t
    {
        uint32_t    DataOffset = 0;
        uint32_t    ChannelsPerFrame = 0;
        uint32_t    FrameStepTimeMS = 1;
        uint32_t    TotalNumberOfFramesInSequence = 0;
        uint32_t    ElapsedPlayTimeMS = 0;

    } FrameControl;

    struct SyncControl_t
    {
        uint32_t          SyncCount = 0;
        uint32_t          SyncAdjustmentCount = 0;
        float             LastRcvdElapsedSeconds = 0.0;
    } SyncControl;

#   define    FPP_TICKER_PERIOD_MS 25
// #   define    FPP_TICKER_PERIOD_MS 1000
    Ticker    MsTicker;
    uint32_t  LastIsrTimeStampMS = 0;
    uint32_t  PlayedFileCount = 0;

    // Logic to detect if polls have stopped coming in.
    // This is part of the blanking logic.
    int       PollDetectionCounter = 0;
    static const int PollDetectionCounterLimit = 5;

#define MAX_NUM_SPARSE_RANGES 5
    FSEQParsedRangeEntry SparseRanges[MAX_NUM_SPARSE_RANGES];

    void        UpdateElapsedPlayTimeMS ();
    uint32_t    CalculateFrameId (uint32_t ElapsedMS, int32_t SyncOffsetMS);
    bool        ParseFseqFile ();
    uint32_t    ReadFile(uint32_t DestinationIntensityId, uint32_t NumBytesToRead, uint32_t FileOffset);

    String      LastFailedPlayStatusMsg;

#ifdef ARDUINO_ARCH_ESP32
    TaskHandle_t TimerPollTaskHandle = NULL;
#   define TimerPollHandlerTaskStack 3000
// #   define TimerPollHandlerTaskStack 6000
#endif // def ARDUINO_ARCH_ESP32

}; // c_InputFPPRemotePlayFile
