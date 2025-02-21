#pragma once
/*
* InputFPPRemotePlayItem.hpp
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
*   Common Play object use to parse and play a play list or file
*/

#include "ESPixelStick.h"
#include "InputMgr.hpp"

class c_InputFPPRemotePlayItem
{
protected:
    time_t  PlayDurationSec = 0;
    bool    SendFppSync = false;
    String  BackgroundFileName = emptyString;

#if defined(ARDUINO_ARCH_ESP8266)
const uint64_t  LocalIntensityBufferSize = 512;
#else
const uint64_t  LocalIntensityBufferSize = 2048;
#endif // defined(ARDUINO_ARCH_ESP8266)

private:
    bool    InputPaused = false;
    int32_t SyncOffsetMS = 0;
    c_InputMgr::e_InputChannelIds InputChannelId = c_InputMgr::e_InputChannelIds::InputChannelId_ALL;

public:
    c_InputFPPRemotePlayItem (c_InputMgr::e_InputChannelIds InputChannelId);
    virtual ~c_InputFPPRemotePlayItem ();

    struct FileControl_t
    {
        String      FileName;
        c_FileMgr::FileId FileHandleForFileBeingPlayed = c_FileMgr::INVALID_FILE_HANDLE;
        uint32_t    RemainingPlayCount = 0;
        uint32_t    DataOffset = 0;
        uint32_t    ChannelsPerFrame = 0;
        uint32_t    FrameStepTimeMS = 25;
        uint32_t    TotalNumberOfFramesInSequence = 0;
        uint32_t    ElapsedPlayTimeMS = 0;
        uint32_t    StartingTimeMS = 0;
        uint32_t    LastPollTimeMS = 0;
        uint32_t    LastPlayedFrameId = 0;
    } FileControl[2];
    #define CurrentFile 0
    #define NextFile 1

    virtual bool     Poll           () = 0;
    virtual void     Start          (String & FileName, float SecondsElapsed, uint32_t RemainingPlayCount) = 0;
    virtual void     Stop           () = 0;
    virtual void     SetPauseState  (bool _PauseInput) {InputPaused = _PauseInput;}
    virtual void     Sync           (String & FileName, float SecondsElapsed) = 0;
    virtual void     GetStatus      (JsonObject & jsonStatus) = 0;
    virtual bool     IsIdle         () = 0;
            String   GetFileName    () { return FileControl[CurrentFile].FileName; }
            uint32_t GetRepeatCount () { return FileControl[CurrentFile].RemainingPlayCount; }
            void     SetDuration    (time_t value) { PlayDurationSec = value; }
            void     GetDriverName  (String& Name) { Name = "InputMgr"; }
            int32_t  GetSyncOffsetMS () { return SyncOffsetMS; }
            void     SetSyncOffsetMS (int32_t value) { SyncOffsetMS = value; }
            void     SetSendFppSync  (bool value) { SendFppSync = value; }
            c_InputMgr::e_InputChannelIds GetInputChannelId () { return InputChannelId; }
            bool     InputIsPaused   () { return InputPaused; }
            void     SetOperationalState (bool ActiveFlag) {InputPaused = !ActiveFlag;}
            void     SetBackgroundFileName(String & FileName) {BackgroundFileName = FileName;}
            void     ClearFileNames  ();

}; // c_InputFPPRemotePlayItem
extern byte *LocalIntensityBuffer;
