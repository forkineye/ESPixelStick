/*
* InputFPPRemoteFsm.cpp
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
*   Fsm object used to parse and play an effect
*/

#include "InputFPPRemotePlayEffect.hpp"
#include "../service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_Idle::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::Poll

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_Idle::Init (c_InputFPPRemotePlayEffect* Parent)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect = Parent;
    p_InputFPPRemotePlayEffect->pCurrentFsmState = &(Parent->fsm_PlayEffect_state_Idle_imp);

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::Init

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_Idle::Start (String & ConfigString, uint32_t )
{
    // DEBUG_START;

    // DEBUG_V (String ("ConfigString: '") + ConfigString + "'");
    p_InputFPPRemotePlayEffect->PLayEffectEndTime = millis () + (1000 * p_InputFPPRemotePlayEffect->PlayDurationSec);
    
    // tell the effect engine what it is supposed to be doing
    DynamicJsonDocument EffectConfig (512);
    DeserializationError error = deserializeJson ((EffectConfig), (const String)ConfigString);

    // DEBUG_V ("Error Check");
    if (error)
    {
        String CfgFileMessagePrefix = String (F ("Effect Config: '")) + ConfigString + "' ";
        LOG_PORT.println (String (F ("Heap:")) + String (ESP.getFreeHeap ()));
        LOG_PORT.println (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
        LOG_PORT.println (String (F ("++++")) + ConfigString + String (F ("----")));
    }

    JsonObject ConfigObject = EffectConfig.as<JsonObject> ();

    String EffectName;
    setFromJSON (EffectName, ConfigObject, F("currenteffect"));
    LOG_PORT.println (String (F ("Playing Effect: '")) + EffectName + "'");

    p_InputFPPRemotePlayEffect->pEffectsEngine->SetConfig (ConfigObject);
    p_InputFPPRemotePlayEffect->fsm_PlayEffect_state_PlayingEffect_imp.Init (p_InputFPPRemotePlayEffect);

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::Start

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_Idle::Stop (void)
{
    // DEBUG_START;

    // Do nothing

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayEffect_state_Idle::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    // Do Nothing

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::Sync

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_Idle::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    // DEBUG_END;

} // fsm_PlayEffect_state_Idle::GetStatus

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_PlayingEffect::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect->pEffectsEngine->SetBufferInfo (Buffer, BufferSize);
    p_InputFPPRemotePlayEffect->pEffectsEngine->Process ();

    if (p_InputFPPRemotePlayEffect->PLayEffectEndTime <= millis ())
    {
        // DEBUG_V ("");
        Stop ();
    }

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::Poll

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_PlayingEffect::Init (c_InputFPPRemotePlayEffect* Parent)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect = Parent;
    p_InputFPPRemotePlayEffect->pCurrentFsmState = &(Parent->fsm_PlayEffect_state_PlayingEffect_imp);

    p_InputFPPRemotePlayEffect->pEffectsEngine->SetOperationalState (true);

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::Init

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_PlayingEffect::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect->pEffectsEngine->SetOperationalState (true);
    InputMgr.SetOperationalState (false);

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::Start

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_PlayingEffect::Stop (void)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect->pEffectsEngine->SetOperationalState (false);
    InputMgr.SetOperationalState (true);

    p_InputFPPRemotePlayEffect->fsm_PlayEffect_state_Idle_imp.Init (p_InputFPPRemotePlayEffect);

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::Stop

//-----------------------------------------------------------------------------
bool fsm_PlayEffect_state_PlayingEffect::Sync (uint32_t FrameId)
{
    // DEBUG_START;

    // do nothing

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::Sync

//-----------------------------------------------------------------------------
void fsm_PlayEffect_state_PlayingEffect::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    p_InputFPPRemotePlayEffect->pEffectsEngine->GetStatus (jsonStatus);

    // DEBUG_END;

} // fsm_PlayEffect_state_PlayingEffect::GetStatus

