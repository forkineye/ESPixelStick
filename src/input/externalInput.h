
// only allow the file to be included once.
#pragma once

#include "../ESPixelStick.h"

/*****************************************************************************/
class fsm_ExternalInput_state;
class c_ExternalInput
{
public:
	c_ExternalInput (void);
	~c_ExternalInput(void) {}

	typedef enum InputValue_e
	{
		off = 0,		// input is off
		shortOn,		// input was on for 0.5 sec
		longOn,         // input was on for 2.0 sec
		on,				// input is on
	} InputValue_t;

	typedef enum Polarity_e
	{
		ActiveHigh = 0,
		ActiveLow,

	} Polarity_t;

	void         Init              (uint32_t iInputId, uint32_t iPinId, Polarity_t Poliarity, String & sName);
	InputValue_t Get               ();
	inline bool  InputHadLongPush  (bool bClearFlag) { bool tmp = m_bHadLongPush;  if (true == bClearFlag) { m_bHadLongPush  = false; } return tmp; }
	inline bool  InputHadShortPush (bool bClearFlag) { bool tmp = m_bHadShortPush; if (true == bClearFlag) { m_bHadShortPush = false; } return tmp; }
	void         Poll              (void);
	void         GetConfig         (JsonObject JsonData);
	void         GetStatistics     (JsonObject JsonData);
	void         ProcessConfig     (JsonObject JsonData);
	bool		 IsEnabled ()      { return m_bIsEnabled; }

protected:

	// read the adjusted value of the input pin
	bool ReadInput (void);

#	define M_NAME       CN_name
#	define M_IO_ENABLED CN_enabled
#	define M_STATE      CN_state
#	define M_POLARITY   CN_polarity
#   define M_ID         CN_id

	String                    m_name;
	uint32_t                  m_iPinId              = 0;
	Polarity_t                m_polarity            = Polarity_t::ActiveLow;
	time_t                    m_ExpirationTime      = 0;
	bool                      m_bIsEnabled          = false;
	uint32_t                  m_iInputDebounceCount = 0;
	uint32_t                  m_iInputHoldTimeMS    = 0;
	bool                      m_bHadLongPush        = false;
	bool                      m_bHadShortPush       = false;
	fsm_ExternalInput_state * m_pCurrentFsmState    = nullptr;

	friend class fsm_ExternalInput_boot;
	friend class fsm_ExternalInput_off_state;
	friend class fsm_ExternalInput_on_wait_short_state;
	friend class fsm_ExternalInput_on_wait_long_state;
	friend class fsm_ExternalInput_wait_for_off_state;

}; // c_ExternalInput

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
class fsm_ExternalInput_state
{
public:
	virtual void Poll(c_ExternalInput * pExternalInput) = 0;
	virtual void Init(c_ExternalInput * pExternalInput) = 0;
	virtual c_ExternalInput::InputValue_t Get(void)     = 0;
private:
#define MIN_INPUT_STABLE_VALUE	5
#define INPUT_SHORT_VALUE_MS    500
#define INPUT_LONG_VALUE_MS     1500

}; // fsm_ExternalInput_state

/*****************************************************************************/
// input is unknown and unreachable
//
class fsm_ExternalInput_boot : public fsm_ExternalInput_state
{
public:
	virtual void Poll (c_ExternalInput * pExternalInput);
	virtual void Init (c_ExternalInput * pExternalInput);
	virtual c_ExternalInput::InputValue_t Get(void) { return c_ExternalInput::InputValue_t::off; }

}; // fsm_ExternalInput_boot

/*****************************************************************************/
// input is off and stable
//
class fsm_ExternalInput_off_state : public fsm_ExternalInput_state
{
public:
	virtual void Poll(c_ExternalInput * pExternalInput);
	virtual void Init(c_ExternalInput * pExternalInput);
	virtual c_ExternalInput::InputValue_t Get(void) { return c_ExternalInput::InputValue_t::off; }

}; // fsm_ExternalInput_off_state

/*****************************************************************************/
// input is on and is stable
//
class fsm_ExternalInput_on_wait_short_state : public fsm_ExternalInput_state
{
public:
	virtual void Poll(c_ExternalInput * pExternalInput);
	virtual void Init(c_ExternalInput * pExternalInput);
	virtual c_ExternalInput::InputValue_t Get(void) { return c_ExternalInput::InputValue_t::on; }

}; // fsm_ExternalInput_on_wait_short_state

/*****************************************************************************/
// input is always reported as on
//
class fsm_ExternalInput_on_wait_long_state : public fsm_ExternalInput_state
{
public:
	virtual void Poll (c_ExternalInput * pExternalInput);
	virtual void Init (c_ExternalInput * pExternalInput);
	virtual c_ExternalInput::InputValue_t Get (void) { return c_ExternalInput::InputValue_t::on; }

}; // fsm_ExternalInput_on_wait_long_state

/*****************************************************************************/
// input is always reported as on
//
class fsm_ExternalInput_wait_for_off_state : public fsm_ExternalInput_state
{
public:
	virtual void Poll (c_ExternalInput* pExternalInput);
	virtual void Init (c_ExternalInput* pExternalInput);
	virtual c_ExternalInput::InputValue_t Get (void) { return c_ExternalInput::InputValue_t::on; }

}; // fsm_ExternalInput_wait_for_off_state

