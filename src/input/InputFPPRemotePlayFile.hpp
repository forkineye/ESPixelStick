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

class c_InputFPPRemotePlayFile : public c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayFile ();
    ~c_InputFPPRemotePlayFile ();

    virtual void Start (String & FileName, uint32_t FrameId);
    virtual void Stop ();
    virtual bool Sync (uint32_t FrameId);
    virtual void Poll (uint8_t* Buffer, size_t BufferSize);
    virtual void GetStatus (JsonObject & jsonStatus);
    virtual bool IsIdle () { return (pCurrentFsmState == &fsm_PlayFile_state_Idle_imp); }

private:
    friend class fsm_PlayFile_state_Idle;
    friend class fsm_PlayFile_state_PlayingFile;
    friend class fsm_PlayFile_state;

    fsm_PlayFile_state_Idle          fsm_PlayFile_state_Idle_imp;
    fsm_PlayFile_state_PlayingFile   fsm_PlayFile_state_PlayingFile_imp;

    fsm_PlayFile_state * pCurrentFsmState = nullptr;
    c_FileMgr::FileId FileBeingPlayed = 0;
    uint32_t          CurrentFrameId = 0;
    size_t            DataOffset = 0;
    uint32_t          ChannelsPerFrame = 0;
    uint32_t          FrameStepTime = 0;
    uint32_t          TotalNumberOfFramesInSequence = 0;
    uint32_t          StartTimeInMillis = 0;


}; // c_InputFPPRemotePlayFile
