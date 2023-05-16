/*
 * Manage a single input line
 */
#include "externalInput.h"
#include "../FileMgr.hpp"

/*****************************************************************************/
/*	Global Data                                                              */
/*****************************************************************************/

/*****************************************************************************/
/*	fsm                                                                      */
/*****************************************************************************/

fsm_ExternalInput_boot                fsm_ExternalInput_boot_imp;
fsm_ExternalInput_off_state           fsm_ExternalInput_off_state_imp;
fsm_ExternalInput_on_wait_short_state fsm_ExternalInput_on_wait_short_state_imp;
fsm_ExternalInput_on_wait_long_state  fsm_ExternalInput_on_wait_long_state_imp;
fsm_ExternalInput_wait_for_off_state  fsm_ExternalInput_wait_for_off_state_imp;

/*****************************************************************************/
/* Code                                                                      */
/*****************************************************************************/

c_ExternalInput::c_ExternalInput(void) :
	m_CurrentFsmState(fsm_ExternalInput_boot_imp)
{
	// DEBUG_START;
	fsm_ExternalInput_boot_imp.Init(*this); // currently redundant, but might Init() might do more ... so important to leave this
	// DEBUG_END;

} // c_ExternalInput

void c_ExternalInput::Init(uint32_t iInputId, uint32_t iPinId, Polarity_t Polarity, String & sName)
{
	// DEBUG_START;

	// Remember the pin number for future calls
	m_iPinId   = iPinId;
	m_name     = sName;
	m_polarity = Polarity;

	// set the pin direction to input
	pinMode(m_iPinId, INPUT);

	// DEBUG_END;

} // Init

/*****************************************************************************/
/*
 *	Get the input value.
 *
 *	needs
 *		nothing
 *	returns
 *		input value
 */
c_ExternalInput::InputValue_t c_ExternalInput::Get()
{
	// DEBUG_START;

	return m_CurrentFsmState.Get();

	// DEBUG_END;

} // Get

/*****************************************************************************/
/*
 *	Process the current configuration
 *
 *	needs
 *		reference to a json config object
 *	returns
 *		nothing
 */
void c_ExternalInput::GetConfig (JsonObject JsonData)
{
	// DEBUG_START;

	JsonData[M_IO_ENABLED] = m_bIsEnabled;
	JsonData[M_NAME]       = m_name;
	JsonData[M_ID]         = m_iPinId;
	JsonData[M_POLARITY]   = (ActiveHigh == m_polarity) ? CN_ActiveHigh : CN_ActiveLow;
	// DEBUG_V (String ("m_iPinId: ") + String (m_iPinId));

	// DEBUG_END;

} // GetConfig

/*****************************************************************************/
/*
 *	Process the current configuration
 *
 *	needs
 *		reference to a json config object
 *	returns
 *		nothing
 */
void c_ExternalInput::GetStatistics (JsonObject JsonData)
{
	// DEBUG_START;

	JsonData[M_ID]    = m_iPinId;
	JsonData[M_STATE] = (InputValue_t::on == Get ()) ? "on" : "off";

	// DEBUG_END;

} // GetStatistics

/*****************************************************************************/
/*
 *	Process the current configuration
 *
 *	needs
 *		reference to a json config object
 *	returns
 *		nothing
 */
void c_ExternalInput::ProcessConfig (JsonObject JsonData)
{
	// DEBUG_START;

	String sPolarity = (ActiveHigh == m_polarity) ? CN_ActiveHigh : CN_ActiveLow;

	uint32_t oldInputId = m_iPinId;
	
	setFromJSON (m_bIsEnabled, JsonData, M_IO_ENABLED);
	setFromJSON (m_name,       JsonData, M_NAME);
	setFromJSON (m_iPinId,     JsonData, M_ID);
	setFromJSON (sPolarity,    JsonData, M_POLARITY);

	m_polarity = (String(CN_ActiveHigh) == sPolarity) ? ActiveHigh : ActiveLow;

	if ((oldInputId != m_iPinId) || (false == m_bIsEnabled))
	{
		pinMode (m_iPinId, INPUT);
		m_bHadLongPush  = false;
		m_bHadShortPush = false;
		fsm_ExternalInput_boot_imp.Init (*this);
	}

	// DEBUG_V (String ("m_iPinId: ") + String (m_iPinId));

	// DEBUG_END;

} // ProcessConfig

/*****************************************************************************/
/*
 *	Poll the state machine
 *
 *	needs
 *		Nothing
 *	returns
 *		nothing
 */
void c_ExternalInput::Poll (void)
{
	// DEBUG_START;
	m_CurrentFsmState.Poll (*this);

	// DEBUG_END;

} // Poll

/*****************************************************************************/
/*
 * read the adjusted value of the input pin
 *
 *	needs
 *		Nothing
 *	returns
 *		true - input is "on"
 */
bool c_ExternalInput::ReadInput (void)
{
	// read the input
	bool bInputValue = digitalRead (m_iPinId);

	// do we need to invert the input?
	if (Polarity_t::ActiveLow == m_polarity)
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
	pExternalInput.m_CurrentFsmState = fsm_ExternalInput_boot_imp;

	// dont do anything

} // fsm_ExternalInput_boot::Init

/*****************************************************************************/
// waiting for the system to come up
void fsm_ExternalInput_boot::Poll (c_ExternalInput& pExternalInput)
{
	// start normal operation
	if (pExternalInput.m_bIsEnabled)
	{
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}

} // fsm_ExternalInput_boot::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is off and is stable
void fsm_ExternalInput_off_state::Init(c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	pExternalInput.m_iInputDebounceCount = MIN_INPUT_STABLE_VALUE;
	pExternalInput.m_CurrentFsmState    = fsm_ExternalInput_off_state_imp;
	// DEBUG_V ("Entring OFF State");

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
		if (0 == --pExternalInput.m_iInputDebounceCount)
		{
			// we really are on
			fsm_ExternalInput_on_wait_short_state_imp.Init(pExternalInput);
		}
	}
	else // still off
	{
		// DEBUG_V ("");
		// reset the debounce counter
		pExternalInput.m_iInputDebounceCount = MIN_INPUT_STABLE_VALUE;
	}

	// DEBUG_END;

} // fsm_ExternalInput_off_state::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is on and is stable
void fsm_ExternalInput_on_wait_short_state::Init(c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	pExternalInput.m_InputHoldTimer.StartTimer(INPUT_SHORT_VALUE_MS);
	pExternalInput.m_CurrentFsmState = fsm_ExternalInput_on_wait_short_state_imp;
	// DEBUG_V ("Entring Wait Short State");

} // fsm_ExternalInput_on_wait_short_state::Init

/*****************************************************************************/
// Input is on and is stable
void fsm_ExternalInput_on_wait_short_state::Poll(c_ExternalInput& pExternalInput)
{
	// DEBUG_START;
	// read the input
	bool bInputValue = pExternalInput.ReadInput ();

	// If the input is "on"
	if (true == bInputValue)
	{
		// DEBUG_V("");
		// decrement the counter
		if (pExternalInput.m_InputHoldTimer.IsExpired())
		{
			// we really are on
			fsm_ExternalInput_on_wait_long_state_imp.Init(pExternalInput);
		}
	}
	else // Turned off
	{
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}
	// DEBUG_END;

} // fsm_ExternalInput_on_wait_short_state::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is always on
void fsm_ExternalInput_on_wait_long_state::Init (c_ExternalInput& pExternalInput)
{
	pExternalInput.m_InputHoldTimer.StartTimer(INPUT_LONG_VALUE_MS);
	pExternalInput.m_CurrentFsmState = fsm_ExternalInput_on_wait_long_state_imp;
	// DEBUG_V ("Entring Wait Long State");

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
		if (pExternalInput.m_InputHoldTimer.IsExpired())
		{
			// we really are on
			fsm_ExternalInput_wait_for_off_state_imp.Init (pExternalInput);
			pExternalInput.m_bHadLongPush = true;
		}
	}
	else // Turned off
	{
		pExternalInput.m_bHadShortPush = true;
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}

	// DEBUG_END;

} // fsm_ExternalInput_on_wait_long_state::Poll

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Input is always on
void fsm_ExternalInput_wait_for_off_state::Init (c_ExternalInput& pExternalInput)
{
	pExternalInput.m_CurrentFsmState = fsm_ExternalInput_wait_for_off_state_imp;
	// DEBUG_V ("Entring Wait OFF State");

} // fsm_ExternalInput_wait_for_off_state::Init

/*****************************************************************************/
// Input is on and is stable
void fsm_ExternalInput_wait_for_off_state::Poll (c_ExternalInput& pExternalInput)
{
	// DEBUG_START;

	// read the input
	bool bInputValue = pExternalInput.ReadInput ();

	// If the input is "on"
	if (false == bInputValue)
	{
		fsm_ExternalInput_off_state_imp.Init (pExternalInput);
	}

	// DEBUG_END;

} // fsm_ExternalInput_wait_for_off_state::Poll
/*****************************************************************************/
