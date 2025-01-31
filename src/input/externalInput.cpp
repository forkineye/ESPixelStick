/*
* externalInput.cpp
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
#include "input/externalInput.h"
#include "input/InputMgr.hpp"
#include "FileMgr.hpp"

/*****************************************************************************/
/*	Global Data                                                              */
/*****************************************************************************/

/*****************************************************************************/
/*	fsm                                                                      */
/*****************************************************************************/

fsm_ExternalInput_boot                fsm_ExternalInput_boot_imp;
fsm_ExternalInput_off_state           fsm_ExternalInput_off_state_imp;
fsm_ExternalInput_on_wait_long_state  fsm_ExternalInput_on_wait_long_state_imp;
fsm_ExternalInput_wait_for_off_state  fsm_ExternalInput_wait_for_off_state_imp;

/*****************************************************************************/
/* Code                                                                      */
/*****************************************************************************/

c_ExternalInput::c_ExternalInput(void)
{
	// DEBUG_START;
	fsm_ExternalInput_boot_imp.Init(*this); // currently redundant, but might Init() might do more ... so important to leave this
	// DEBUG_END;

} // c_ExternalInput

/*****************************************************************************/
void c_ExternalInput::Init(uint32_t iInputId, uint32_t iPinId, Polarity_t Polarity, String & sName)
{
	// DEBUG_START;

	// Remember the pin number for future calls
	GpioId   = iPinId;
	name     = sName;
	polarity = Polarity;

	// set the pin direction to input
    ResetGpio(gpio_num_t(GpioId));
	pinMode(GpioId, INPUT);

	// DEBUG_END;

} // Init

/*****************************************************************************/
void c_ExternalInput::GetConfig (JsonObject JsonData)
{
    // DEBUG_START;

	JsonWrite(JsonData, CN_enabled,  Enabled);
	JsonWrite(JsonData, CN_name,     name);
	JsonWrite(JsonData, CN_id,       GpioId);
	JsonWrite(JsonData, CN_polarity, String((ActiveHigh == polarity) ? CN_ActiveHigh : CN_ActiveLow));
	JsonWrite(JsonData, CN_channels, TriggerChannel);
	JsonWrite(JsonData, CN_long,     LongPushDelayMS);

	// DEBUG_V (String ("m_iPinId: ") + String (m_iPinId));

    // DEBUG_END;

} // GetConfig

/*****************************************************************************/
void c_ExternalInput::GetStatistics (JsonObject JsonData)
{
	// DEBUG_START;

	JsonWrite(JsonData, CN_id,    GpioId);
	JsonWrite(JsonData, CN_state, String((ReadInput()) ? CN_on : CN_off));

	// DEBUG_END;

} // GetStatistics

/*****************************************************************************/
void c_ExternalInput::ProcessConfig (JsonObject JsonData)
{
	// DEBUG_START;

	String Polarity = (ActiveHigh == polarity) ? CN_ActiveHigh : CN_ActiveLow;

	uint32_t oldInputId = GpioId;
	
	setFromJSON (Enabled,         JsonData, CN_enabled);
	setFromJSON (name,            JsonData, CN_name);
	setFromJSON (GpioId,          JsonData, CN_id);
	setFromJSON (Polarity,        JsonData, CN_polarity);
	setFromJSON (TriggerChannel,  JsonData, CN_channels);
	setFromJSON (LongPushDelayMS, JsonData, CN_long);

	polarity = (String(CN_ActiveHigh) == Polarity) ? ActiveHigh : ActiveLow;

	if ((oldInputId != GpioId) || (false == Enabled))
	{
        ResetGpio(gpio_num_t(oldInputId));
        ResetGpio(gpio_num_t(GpioId));
		pinMode (oldInputId, INPUT);
		pinMode (GpioId, INPUT_PULLUP);
		fsm_ExternalInput_boot_imp.Init (*this);
	}

	// DEBUG_V (String ("m_iPinId: ") + String (m_iPinId));

	// DEBUG_END;

} // ProcessConfig

/*****************************************************************************/
void c_ExternalInput::Poll ()
{
	// DEBUG_START;

	CurrentFsmState->Poll (*this);

	// DEBUG_END;

} // Poll

/*****************************************************************************/
bool c_ExternalInput::ReadInput (void)
{
	// read the input
	bool bInputValue = digitalRead (GpioId);

	// do we need to invert the input?
	if (Polarity_t::ActiveLow == polarity)
	{
		// invert the input value
		bInputValue = !bInputValue;
	}

	// DEBUG_V (String ("m_iPinId:    ") + String (m_iPinId));
	// DEBUG_V (String ("bInputValue: ") + String (bInputValue));
	return bInputValue;

} // ReadInput

/*****************************************************************************/
/*	FSM                                                                      */
/*****************************************************************************/

/*****************************************************************************/
// waiting for the system to come up
void fsm_ExternalInput_boot::Init (c_ExternalInput& pExternalInput)
{
    // DEBUG_START;

    // DEBUG_V ("Entring BOOT State");
	pExternalInput.CurrentFsmState = &fsm_ExternalInput_boot_imp;
	// dont do anything

    // DEBUG_END;

} // fsm_ExternalInput_boot::Init

/*****************************************************************************/
// waiting for the system to come up
void fsm_ExternalInput_boot::Poll (c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	// start normal operation
	if (pExternalInput.Enabled)
	{
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}

	// DEBUG_END;

} // fsm_ExternalInput_boot::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is off and is stable
void fsm_ExternalInput_off_state::Init(c_ExternalInput& pExternalInput)
{
    // DEBUG_START;

    // DEBUG_V ("Entring OFF State");
	pExternalInput.InputDebounceCount = MIN_INPUT_STABLE_VALUE;
	pExternalInput.CurrentFsmState    = &fsm_ExternalInput_off_state_imp;
	InputMgr.ProcessButtonActions(c_ExternalInput::InputValue_t::off);

    // DEBUG_END;

} // fsm_ExternalInput_off_state::Init

/*****************************************************************************/
// Input was off
void fsm_ExternalInput_off_state::Poll(c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	// read the input
	bool bInputValue = pExternalInput.ReadInput();
	
	// If the input is "on"
	if (true == bInputValue)
	{
		// decrement the counter
		if (0 == --pExternalInput.InputDebounceCount)
		{
			// we really are on
			fsm_ExternalInput_on_wait_long_state_imp.Init(pExternalInput);
		}
	}
	else // still off
	{
		//  DEBUG_V ("reset the debounce counter");
		pExternalInput.InputDebounceCount = MIN_INPUT_STABLE_VALUE;
	}

	//  DEBUG_END;

} // fsm_ExternalInput_off_state::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is always on
void fsm_ExternalInput_on_wait_long_state::Init (c_ExternalInput& pExternalInput)
{
    // DEBUG_START;

    // DEBUG_V ("Entring Wait Long State");
	pExternalInput.InputHoldTimer.StartTimer(pExternalInput.LongPushDelayMS, false);
	pExternalInput.CurrentFsmState = &fsm_ExternalInput_on_wait_long_state_imp;

    // DEBUG_END;

} // fsm_ExternalInput_on_wait_long_state::Init

/*****************************************************************************/
// Input is on and is stable
void fsm_ExternalInput_on_wait_long_state::Poll (c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	// read the input
	bool bInputValue = pExternalInput.ReadInput ();

	// If the input is "on"
	if (true == bInputValue)
	{
		// DEBUG_V("");
		// decrement the counter
		if (pExternalInput.InputHoldTimer.IsExpired())
		{
			// we really are on
			fsm_ExternalInput_wait_for_off_state_imp.Init (pExternalInput);
			InputMgr.ProcessButtonActions(c_ExternalInput::InputValue_t::longOn);
		    // DEBUG_V("HadLongPush = true")
		}
	}
	else // Turned off
	{
	    // DEBUG_V("HadShortPush = true")
		InputMgr.ProcessButtonActions(c_ExternalInput::InputValue_t::shortOn);
		fsm_ExternalInput_wait_for_off_state_imp.Init (pExternalInput);
	}

	// DEBUG_END;

} // fsm_ExternalInput_on_wait_long_state::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is always on
void fsm_ExternalInput_wait_for_off_state::Init (c_ExternalInput& pExternalInput)
{
    // DEBUG_START;

    // DEBUG_V ("Entring Wait OFF State");
	pExternalInput.CurrentFsmState = &fsm_ExternalInput_wait_for_off_state_imp;

    // DEBUG_END;

} // fsm_ExternalInput_wait_for_off_state::Init

/*****************************************************************************/
// Input is on and is stable
void fsm_ExternalInput_wait_for_off_state::Poll (c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	// read the input
	bool bInputValue = pExternalInput.ReadInput ();

	// If the input is "on" then we wait for it to go off
	if (false == bInputValue)
	{
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}

	// DEBUG_END;

} // fsm_ExternalInput_wait_for_off_state::Poll
/*****************************************************************************/
