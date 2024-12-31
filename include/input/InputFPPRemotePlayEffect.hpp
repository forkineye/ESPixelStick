#pragma once
/*
* InputFPPRemotePlayEffect.hpp
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
*   PlayFile object used to parse and play an effect
*/

#include "ESPixelStick.h"
#include "InputFPPRemotePlayItem.hpp"
#include "InputFPPRemotePlayEffectFsm.hpp"
#include "InputEffectEngine.hpp"

class c_InputFPPRemotePlayEffect : public c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayEffect (c_InputMgr::e_InputChannelIds InputChannelId);
    virtual ~c_InputFPPRemotePlayEffect ();

    virtual void Start     (String & FileName, float duration, uint32_t PlayCount);
    virtual void Stop      ();
    virtual void Sync      (String & FileName, float SecondsElapsed);
    virtual bool Poll      ();
    virtual void GetStatus (JsonObject & jsonStatus);
    virtual bool IsIdle    () { return (pCurrentFsmState == &fsm_PlayEffect_state_Idle_imp); }

protected:

    friend class fsm_PlayEffect_state_Idle;
    friend class fsm_PlayEffect_state_PlayingEffect;
    friend class fsm_PlayEffect_state;

    fsm_PlayEffect_state_Idle          fsm_PlayEffect_state_Idle_imp;
    fsm_PlayEffect_state_PlayingEffect fsm_PlayEffect_state_PlayingEffect_imp;

    fsm_PlayEffect_state* pCurrentFsmState = nullptr;
    FastTimer PlayEffectTimer;

    c_InputEffectEngine EffectsEngine;

}; // c_InputFPPRemotePlayEffect
