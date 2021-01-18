#pragma once
/*
* InputFPPRemotePlayEffect.hpp
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
*   PlayFile object used to parse and play an effect
*/

#include "../ESPixelStick.h"
#include "InputFPPRemotePlayItem.hpp"
#include "InputFPPRemotePlayEffectFsm.hpp"

class c_InputFPPRemotePlayEffect : c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayEffect (String & NameOfPlayFile);
    ~c_InputFPPRemotePlayEffect ();

    virtual void Start ();
    virtual void Stop  ();
    virtual void Sync  (time_t syncTime);
    virtual void Poll  ();
    virtual void GetStatus (JsonObject & jsonStatus);

private:

    friend class fsm_PlayEffect_state_Idle;
    friend class fsm_PlayEffect_state_PlayingEffect;
    friend class fsm_PlayEffect_state_Paused;
    friend class fsm_PlayEffect_state;

    fsm_PlayEffect_state_Idle          fsm_PlayEffect_state_Idle_imp;
    fsm_PlayEffect_state_PlayingEffect fsm_PlayEffect_state_PlayingEffect_imp;
    fsm_PlayEffect_state_Paused        fsm_PlayEffect_state_Paused_imp;

    fsm_PlayEffect_state* pCurrentFsmState = nullptr;

}; // c_InputFPPRemotePlayEffect
