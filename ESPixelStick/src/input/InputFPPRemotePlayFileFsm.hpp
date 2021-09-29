#pragma once
/*
* InputFPPRemotePlayFileFsm.hpp
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
*   PlayFile object use to parse and play a File
*/

#include "../ESPixelStick.h"
#include "../FileMgr.hpp"

class c_InputFPPRemotePlayFile;

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
class fsm_PlayFile_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize) = 0;
    virtual void Init (c_InputFPPRemotePlayFile * Parent) = 0;
    virtual void GetStateName (String & sName) = 0;
    virtual void Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount) = 0;
    virtual void Stop (void) = 0;
    virtual bool Sync (String& FileName, uint32_t FrameId) = 0;
    void GetDriverName (String& Name) { Name = "InputMgr"; }

protected:
    c_InputFPPRemotePlayFile * p_InputFPPRemotePlayFile = nullptr;

}; // fsm_PlayFile_state

/*****************************************************************************/
class fsm_PlayFile_state_Idle : public fsm_PlayFile_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayFile* Parent);
    virtual void GetStateName (String & sName) { sName = CN_Idle; }
    virtual void Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount);
    virtual void Stop (void);
    virtual bool Sync (String& FileName, uint32_t FrameId);

}; // fsm_PlayFile_state_Idle

/*****************************************************************************/
class fsm_PlayFile_state_Starting : public fsm_PlayFile_state
{
public:
    virtual void Poll (uint8_t* Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayFile* Parent);
    virtual void GetStateName (String& sName) { sName = F ("Starting"); }
    virtual void Start (String& FileName, uint32_t FrameId, uint32_t RemainingPlayCount);
    virtual void Stop (void);
    virtual bool Sync (String& FileName, uint32_t FrameId);

}; // fsm_PlayFile_state_Starting

/*****************************************************************************/
class fsm_PlayFile_state_PlayingFile : public fsm_PlayFile_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayFile* Parent);
    virtual void GetStateName (String & sName) { sName = CN_File; }
    virtual void Start (String & FileName, uint32_t FrameId, uint32_t RemainingPlayCount);
    virtual void Stop (void);
    virtual bool Sync (String & FileName, uint32_t FrameId);
private:
    struct SparseRange
    {
        uint32_t DataOffset;
        uint32_t ChannelCount;
    };

}; // fsm_PlayFile_state_PlayingFile

/*****************************************************************************/
class fsm_PlayFile_state_Stopping : public fsm_PlayFile_state
{
public:
    virtual void Poll (uint8_t* Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayFile* Parent);
    virtual void GetStateName (String& sName) { sName = F("Stopping"); }
    virtual void Start (String& FileName, uint32_t FrameId, uint32_t RemainingPlayCount);
    virtual void Stop (void);
    virtual bool Sync (String& FileName, uint32_t FrameId);

private:
    String   FileName = "";
    uint32_t FrameId = 0;
    uint32_t PlayCount = 0;

}; // fsm_PlayFile_state_Stopping
