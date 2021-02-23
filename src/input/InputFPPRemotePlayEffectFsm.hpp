#pragma once
/*
* InputFPPRemotePlayEffectFsm.hpp
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
*   PlayEffect object use to parse and play a PlayEffect
*/

#include "../ESPixelStick.h"

class c_InputFPPRemotePlayEffect;

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
class fsm_PlayEffect_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize) = 0;
    virtual void Init (c_InputFPPRemotePlayEffect * Parent) = 0;
    virtual void GetStateName (String & sName) = 0;
    virtual void Start (String & FileName, uint32_t FrameId) = 0;
    virtual void Stop (void) = 0;
    virtual bool Sync (uint32_t FrameId) = 0;
    virtual void GetStatus (JsonObject& jsonStatus) = 0;

protected:
    c_InputFPPRemotePlayEffect* p_InputFPPRemotePlayEffect = nullptr;

}; // fsm_PlayEffect_state

/*****************************************************************************/
class fsm_PlayEffect_state_Idle : public fsm_PlayEffect_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayEffect* Parent);
    virtual void GetStateName (String & sName) { sName = CN_Idle; }
    virtual void Start (String & FileName, uint32_t FrameId);
    virtual void Stop (void);
    virtual bool Sync (uint32_t FrameId);
    virtual void GetStatus (JsonObject& jsonStatus);

}; // fsm_PlayEffect_state_Idle

/*****************************************************************************/
class fsm_PlayEffect_state_PlayingEffect : public fsm_PlayEffect_state
{
public:
    virtual void Poll (uint8_t * Buffer, size_t BufferSize);
    virtual void Init (c_InputFPPRemotePlayEffect* Parent);
    virtual void GetStateName (String & sName) { sName = CN_Effect; }
    virtual void Start (String & FileName, uint32_t FrameId);
    virtual void Stop (void);
    virtual bool Sync (uint32_t FrameId);
    virtual void GetStatus (JsonObject& jsonStatus);

}; // fsm_PlayEffect_state_PlayingEffect
