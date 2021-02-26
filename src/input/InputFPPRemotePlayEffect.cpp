/*
* InputFPPRemotePlayEffect.cpp
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

#include "InputFPPRemotePlayEffect.hpp"
#include "InputEffectEngine.hpp"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayEffect::c_InputFPPRemotePlayEffect () :
    c_InputFPPRemotePlayItem ()
{
    // DEBUG_START;

    // Tell input manager to not put any data into the input buffer
    InputMgr.SetOperationalState (false);

    fsm_PlayEffect_state_Idle_imp.Init (this);

    EffectsEngine.Begin ();
    EffectsEngine.SetOperationalState (false);

    // DEBUG_END;
} // c_InputFPPRemotePlayEffect

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayEffect::~c_InputFPPRemotePlayEffect ()
{
    // DEBUG_START;

    // allow the other input channels to run
    InputMgr.SetOperationalState (true);
    EffectsEngine.SetOperationalState (false);

    // DEBUG_END;

} // ~c_InputFPPRemotePlayEffect

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Start (String & FileName, uint32_t duration, uint32_t )
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, duration);

    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Stop ()
{
    // DEBUG_START;

    pCurrentFsmState->Stop ();

    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Sync (String& FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Sync (FrameId);

    // DEBUG_END;
} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    pCurrentFsmState->Poll (Buffer, BufferSize);

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    pCurrentFsmState->GetStatus (jsonStatus);

    // DEBUG_END;

} // GetStatus
