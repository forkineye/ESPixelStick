
#pragma once
/* externalInput.h
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
*/

#include "ESPixelStick.h"

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
		shortOn,		// input was on for <N MS
		longOn,         // input was on for <N MS
	};

	enum Polarity_t
	{
		ActiveHigh = 0,
		ActiveLow,
	};

	void         Init              (uint32_t iInputId, uint32_t iPinId, Polarity_t Poliarity, String & sName);
	void         Poll              (void);
	void         GetConfig         (JsonObject JsonData);
	void         GetStatistics     (JsonObject JsonData);
	void         ProcessConfig     (JsonObject JsonData);
	bool		 IsEnabled ()      { return Enabled; }
	uint32_t	 GetTriggerChannel() { return TriggerChannel; }
	void         GetDriverName     (String & Name) { Name = "ExtInput"; }

protected:

	// read the adjusted value of the input pin
	bool ReadInput (void);
/*
#	define M_NAME       CN_name
#	define M_IO_ENABLED CN_enabled
#	define M_STATE      CN_state
#	define M_POLARITY   CN_polarity
#   define M_ID         CN_id
*/
	String                    name;
    uint32_t                  GpioId              = 0;
	uint32_t			      TriggerChannel      = uint32_t(32);
	Polarity_t                polarity            = Polarity_t::ActiveLow;
	bool                      Enabled             = false;
	uint32_t                  InputDebounceCount  = 0;
	FastTimer                 InputHoldTimer;
	uint32_t				  LongPushDelayMS     = 2000;
	fsm_ExternalInput_state * CurrentFsmState     = nullptr;    // initialized in constructor

	friend class fsm_ExternalInput_boot;
	friend class fsm_ExternalInput_off_state;
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
	virtual ~fsm_ExternalInput_state() {};
private:
#define MIN_INPUT_STABLE_VALUE	50

}; // fsm_ExternalInput_state

/*****************************************************************************/
// input is unknown and unreachable
//
class fsm_ExternalInput_boot final : public fsm_ExternalInput_state
{
public:
	void Poll (c_ExternalInput& pExternalInput) override;
	void Init (c_ExternalInput& pExternalInput) override;
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
	~fsm_ExternalInput_off_state() override {};

}; // fsm_ExternalInput_off_state

/*****************************************************************************/
// input is always reported as on
//
class fsm_ExternalInput_on_wait_long_state final : public fsm_ExternalInput_state
{
public:
	void Poll (c_ExternalInput& pExternalInput) override;
	void Init (c_ExternalInput& pExternalInput) override;
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
	~fsm_ExternalInput_wait_for_off_state() override {};

}; // fsm_ExternalInput_wait_for_off_state

