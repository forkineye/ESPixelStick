#pragma once
/*
* OutputTLS3001Rmt.h - TLS3001 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2026 Shelby Merrick
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
#if defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "OutputTLS3001.hpp"
#include "OutputRmt.hpp"
#include "OutputTLS3001RmtFsm.hpp"

class c_OutputTLS3001Rmt : public c_OutputTLS3001
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTLS3001Rmt(OM_OutputPortDefinition_t & OutputPortDefinition,
                       c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputTLS3001Rmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    uint32_t Poll ();                                         ///< Call from loop (),  renders output data
    bool    RmtPoll ();
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);
    bool    ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    void    StartNewDataFrame();

private:
    void    SetBitTimes ();

protected:
    friend class fsm_RMT_state_SendSync;
    friend class fsm_RMT_state_SendReset;
    friend class fsm_RMT_state_SendDataStart;
    friend class fsm_RMT_state_SendData;
    friend class fsm_RMT_state_SendDataIdle;
    friend class fsm_RMT_state;
    OutputTLS3001RmtFsmStates_t CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataIdle;

    fsm_RMT_state_SendSync      fsm_RMT_state_SendSync_imp;
    fsm_RMT_state_SendReset     fsm_RMT_state_SendReset_imp;
    fsm_RMT_state_SendDataStart fsm_RMT_state_SendDataStart_imp;
    fsm_RMT_state_SendData      fsm_RMT_state_SendData_imp;
    fsm_RMT_state_SendDataIdle  fsm_RMT_state_SendDataIdle_imp;

    rmt_item32_t                RmtResetOneMsDelay;
    uint32_t                    NumResetDelayBits = 1;

    rmt_item32_t                RmtSyncIdle;
    uint32_t                    NumSyncIdleBits = 1;

    rmt_item32_t                RmtIfgBit;
    uint32_t                    NumIfgBitsPerFrame = 1;

    #define TLS3001_PIXEL_RMT_TICKS_BIT  uint16_t(TLS3001_PIXEL_NS_BIT / RMT_TickLengthNS)
    const rmt_item32_t          RmtOneBit  = {TLS3001_PIXEL_RMT_TICKS_BIT / 2, 1, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0};
    const rmt_item32_t          RmtZeroBit = {TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 1};

    bool                        SendingData = false;

    c_OutputRmt Rmt;

    // #define USE_TLS3001RMT_COUNTERS
    #ifdef USE_TLS3001RMT_COUNTERS
        #define INCREMENT_TLS3001_COUNTER(c) (++TLS3001RMTCounters.c)
        #define INCREMENT_TLS3001_FSM_COUNTER(c) (++Parent->TLS3001RMTCounters.c)
    struct TLS3001RMTCounters_t
    {
        uint32_t FrameStarted  = 0;
        uint32_t FrameErrorNotFinished = 0;
        uint32_t MinFrameLenMs = uint32_t(-1);
        uint32_t MaxFrameLenMs = 0;
        uint32_t MinPollTimeMs = uint32_t(-1);
        uint32_t MaxPollTimeMs = 0;
        uint32_t PollCounter   = 0;
        uint32_t TooSoon       = 0;
        uint32_t NoGpio        = 0;
        uint32_t SendReset_Init = 0;
        uint32_t SendReset_GetNextBitToSend = 0;
        uint32_t SendSync_Init = 0;
        uint32_t SendSync_GetNextBitToSend = 0;
        uint32_t SendDataStart_Init = 0;
        uint32_t SendDataStart_GetNextBitToSend = 0;
        uint32_t SendData_Init = 0;
        uint32_t SendData_GetNextBitToSend = 0;
        uint32_t SendData_RefreshData = 0;
        uint32_t SendDataIdle_Init = 0;
        uint32_t SendDataIdle_GetNextBitToSend = 0;
    } TLS3001RMTCounters;

    #else
        #define INCREMENT_TLS3001_COUNTER(c)
        #define INCREMENT_TLS3001_FSM_COUNTER(c)
    #endif // def USE_TLS3001RMT_COUNTERS

}; // c_OutputTLS3001Rmt

#endif // defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)
