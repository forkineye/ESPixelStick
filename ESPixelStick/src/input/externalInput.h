
// only allow the file to be included once.
#pragma once

#include "../ESPixelStick.h"

/*****************************************************************************/
class fsm_ExternalInput_state;
class c_ExternalInput final
{
public:
	c_ExternalInput (void);
	~c_ExternalInput(void) {}

	enum InputValue_t
	{
		off = 0,		// input is off
		on,				// input is on
	};

	enum Polarity_t
	{
		ActiveHigh = 0,
		ActiveLow,
	};

	void         Init              (uint32_t iInputId, uint32_t iPinId, Polarity_t Poliarity, String & sName);
	InputValue_t Get               ();
	inline bool  InputHadLongPush  (bool bClearFlag) { bool tmp = m_bHadLongPush;  if (true == bClearFlag) { m_bHadLongPush  = false; } return tmp; }
	inline bool  InputHadShortPush (bool bClearFlag) { bool tmp = m_bHadShortPush; if (true == bClearFlag) { m_bHadShortPush = false; } return tmp; }
	void         Poll              (void);
	void         GetConfig         (JsonObject JsonData);
	void         GetStatistics     (JsonObject JsonData);
	void         ProcessConfig     (JsonObject JsonData);
	bool		 IsEnabled ()      { return m_bIsEnabled; }
	void         GetDriverName     (String & Name) { Name = "ExtInput"; }

protected:

	// read the adjusted value of the input pin
	bool ReadInput (void);

#	define M_NAME       CN_name
#	define M_IO_ENABLED CN_enabled
#	define M_STATE      CN_state
#	define M_POLARITY   CN_polarity
#   define M_ID         CN_id

	String                    m_name;
	uint32_t                  m_iPinId;
	Polarity_t                m_polarity;
	time_t                    m_ExpirationTime;
	bool                      m_bIsEnabled;
	uint32_t                  m_iInputDebounceCount;
	uint32_t                  m_iInputHoldTimeMS;
	bool                      m_bHadLongPush;
	bool                      m_bHadShortPush;
	fsm_ExternalInput_state&  m_CurrentFsmState;

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
	virtual void Poll(c_ExternalInput& pExternalInput) = 0;
	virtual void Init(c_ExternalInput& pExternalInput) = 0;
	virtual c_ExternalInput::InputValue_t Get(void)    = 0;
	virtual ~fsm_ExternalInput_state() {};
private:
#define MIN_INPUT_STABLE_VALUE	5
#define INPUT_SHORT_VALUE_MS    500
#define INPUT_LONG_VALUE_MS     1500

}; // fsm_ExternalInput_state

/*****************************************************************************/
// input is unknown and unreachable
//
class fsm_ExternalInput_boot final : public fsm_ExternalInput_state
{
public:
	void Poll (c_ExternalInput& pExternalInput) override;
	void Init (c_ExternalInput& pExternalInput) override;
	c_ExternalInput::InputValue_t Get(void) override { return c_ExternalInput::InputValue_t::off; }
	~fsm_ExternalInput_boot() override {};
}; // fsm_ExternalInput_boot

/*****************************************************************************/
// input is off and stable
//
class fsm_ExternalInput_off_state final : public fsm_ExternalInput_state
{
public:
	void Poll(c_ExternalInput& pExternalInput) override;
	void Init(c_ExternalInput& pExternalInput) override;
	c_ExternalInput::InputValue_t Get(void) override { return c_ExternalInput::InputValue_t::off; }
	~fsm_ExternalInput_off_state() override {};

}; // fsm_ExternalInput_off_state

/*****************************************************************************/
// input is on and is stable
//
class fsm_ExternalInput_on_wait_short_state final : public fsm_ExternalInput_state
{
public:
	void Poll(c_ExternalInput& pExternalInput) override;
	void Init(c_ExternalInput& pExternalInput) override;
	c_ExternalInput::InputValue_t Get(void) override { return c_ExternalInput::InputValue_t::on; }
	~fsm_ExternalInput_on_wait_short_state() override {};

}; // fsm_ExternalInput_on_wait_short_state

/*****************************************************************************/
// input is always reported as on
//
class fsm_ExternalInput_on_wait_long_state final : public fsm_ExternalInput_state
{
public:
	void Poll (c_ExternalInput& pExternalInput) override;
	void Init (c_ExternalInput& pExternalInput) override;
	c_ExternalInput::InputValue_t Get (void) override { return c_ExternalInput::InputValue_t::on; }
	~fsm_ExternalInput_on_wait_long_state() override {};

}; // fsm_ExternalInput_on_wait_long_state

/*****************************************************************************/
// input is always reported as on
//
class fsm_ExternalInput_wait_for_off_state final : public fsm_ExternalInput_state
{
public:
	void Poll (c_ExternalInput& pExternalInput) override;
	void Init (c_ExternalInput& pExternalInput) override;
	c_ExternalInput::InputValue_t Get (void) override { return c_ExternalInput::InputValue_t::on; }
	~fsm_ExternalInput_wait_for_off_state() override {};

}; // fsm_ExternalInput_wait_for_off_state

