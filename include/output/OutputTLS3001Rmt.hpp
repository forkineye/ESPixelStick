#pragma once
/*
* OutputTLS3001Rmt.h - TLS3001 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2025 Shelby Merrick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#include "ESPixelStick.h"
#if defined(SUPPORT_OutputType_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "OutputTLS3001.hpp"
#include "OutputRmt.hpp"

class fsm_RMT_state
{
public:
    fsm_RMT_state() {}
    virtual ~fsm_RMT_state() {}
    virtual void Init(c_OutputTLS3001Rmt *Parent) = 0;
    virtual void Poll (c_OutputTLS3001Rmt * Parent) = 0;
    uint32_t     FsmTimerStartTime = 0;
    void GetDriverName (String& Name) { Name = "TLS3001"; }

}; // fsm_RMT_state

class c_OutputTLS3001Rmt : public c_OutputTLS3001
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTLS3001Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
        gpio_num_t outputGpio,
        uart_port_t uart,
        c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputTLS3001Rmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    void    Poll ();                                        ///< Call from loop (),  renders output data
    bool    RmtPoll ();
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);

private:


protected:
    friend class fsm_RMT_state_SendReset;
    friend class fsm_RMT_state_SendStart;
    friend class fsm_RMT_state_SendData;
    friend class fsm_RMT_state;
    fsm_RMT_state * pCurrentFsmState = nullptr;

    c_OutputRmt Rmt;

}; // c_OutputTLS3001Rmt

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
class fsm_RMT_state_SendReset : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendReset() {}
    virtual ~fsm_RMT_state_SendReset() {}
    virtual void Init(c_OutputTLS3001Rmt *Parent);
    virtual void Poll (c_OutputTLS3001Rmt * Parent);

}; // fsm_RMT_state_SendReset

/*****************************************************************************/
class fsm_RMT_state_SendStart : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendStart() {}
    virtual ~fsm_RMT_state_SendStart() {}
    virtual void Init(c_OutputTLS3001Rmt *Parent);
    virtual void Poll (c_OutputTLS3001Rmt * Parent);

}; // fsm_RMT_state_SendStart

/*****************************************************************************/
class fsm_RMT_state_SendData : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendData() {}
    virtual ~fsm_RMT_state_SendData() {}
    virtual void Init(c_OutputTLS3001Rmt *Parent);
    virtual void Poll (c_OutputTLS3001Rmt * Parent);
private:
    uint32_t FrameCount = 0;

}; // fsm_RMT_state_SendData

#endif // defined(SUPPORT_OutputType_TLS3001) && defined (ARDUINO_ARCH_ESP32)
