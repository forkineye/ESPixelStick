#pragma once
/*
* OutputUCS1903Rmt.h - UCS1903 driver code for ESPixelStick RMT Channel
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

#if defined(SUPPORT_OutputProtocol_UCS1903) && defined(ARDUINO_ARCH_ESP32)

#include "OutputUCS1903.hpp"
#include "OutputRmt.hpp"

class c_OutputUCS1903Rmt : public c_OutputUCS1903
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputUCS1903Rmt (OM_OutputPortDefinition_t & OutputPortDefinition,
                        c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputUCS1903Rmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    uint32_t Poll ();                                        ///< Call from loop (),  renders output data
    bool    RmtPoll ();
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);
    bool    ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    void    StartNewDataFrame();

private:

// The adjustments compensate for rounding errors in the calculations
#define UCS1903_PIXEL_RMT_TICKS_BIT_0_HIGH    uint16_t ( (UCS1903_PIXEL_NS_BIT_0_HIGH / RMT_TickLengthNS) + 0.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_0_LOW     uint16_t ( (UCS1903_PIXEL_NS_BIT_0_LOW  / RMT_TickLengthNS) + 0.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_1_HIGH    uint16_t ( (UCS1903_PIXEL_NS_BIT_1_HIGH / RMT_TickLengthNS) - 1.0)
#define UCS1903_PIXEL_RMT_TICKS_BIT_1_LOW     uint16_t ( (UCS1903_PIXEL_NS_BIT_1_LOW  / RMT_TickLengthNS) + 1.0)
#define UCS1903_PIXEL_RMT_TICKS_IDLE          uint16_t ( (UCS1903_PIXEL_IDLE_TIME_NS  / RMT_TickLengthNS) + 1.0)

//     {{UCS1903_PIXEL_RMT_TICKS_IDLE / 10,  0, UCS1903_PIXEL_RMT_TICKS_IDLE / 10, 1}, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID},

    rmt_item32_t    ZeroBit = {UCS1903_PIXEL_RMT_TICKS_BIT_0_HIGH, 0, UCS1903_PIXEL_RMT_TICKS_BIT_0_LOW, 1};
    rmt_item32_t    OneBit  = {UCS1903_PIXEL_RMT_TICKS_BIT_1_HIGH, 0, UCS1903_PIXEL_RMT_TICKS_BIT_1_LOW, 1};
    rmt_item32_t    IfgBit;
    uint32_t        IfgBitCount;
    uint32_t        IfgBitCurrentCount;
    uint32_t        DataPattern;
    uint32_t        DataPatternMask;

    // #define UCS1903_RMT_DEBUG_COUNTERS
    #ifdef UCS1903_RMT_DEBUG_COUNTERS
    #define INC_UCS1903_RMT_DEBUG_COUNTERS(c) (++RmtDebugCounters.c)
    struct
    {
        uint32_t GetNextBit = 0;
        uint32_t FrameStarts = 0;
        uint32_t FrameEnds = 0;
        uint32_t BreakBits = 0;
        uint32_t MabBits = 0;
        uint32_t StartBits = 0;
        uint32_t DataBits = 0;
        uint32_t StopBits = 0;
        uint32_t Underrun = 0;
    } RmtDebugCounters;
    #else
    #define INC_UCS1903_RMT_DEBUG_COUNTERS(c)
    #endif // def UCS1903_RMT_DEBUG_COUNTERS

    c_OutputRmt Rmt;

}; // c_OutputUCS1903Rmt

#endif // defined(SUPPORT_OutputProtocol_UCS1903) && defined(ARDUINO_ARCH_ESP32)
