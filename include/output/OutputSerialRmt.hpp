#pragma once
/*
* OutputSerialRmt.h - WS2811 driver code for ESPixelStick RMT Channel
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
#if (defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(ARDUINO_ARCH_ESP32)

#include "OutputSerial.hpp"
#include "OutputRmt.hpp"

class c_OutputSerialRmt : public c_OutputSerial
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSerialRmt (OM_OutputPortDefinition_t & OutputPortDefinition,
        c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputSerialRmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    uint32_t Poll ();                                        ///< Call from loop (),  renders output data
    bool    RmtPoll ();
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);
    void    StartNewDataFrame();
    bool    ISR_GetNextBitToSend(rmt_item32_t & DataToSend);

private:
    void    SetUpRmtBitTimes();
    rmt_idle_level_t idle_level = rmt_idle_level_t::RMT_IDLE_LEVEL_HIGH;

    c_OutputRmt Rmt;

    rmt_item32_t    BreakBit;
    uint32_t        BreakBitCount = 0;
    rmt_item32_t    MabBit;
    uint32_t        MabBitCount = 0;
    rmt_item32_t    StartBit;
    uint32_t        StartBitCount = 0;
    rmt_item32_t    DataBitArray[4]; // 00 / 01 / 10 / 11
    uint32_t        CurrentDataPairId = 0;
    rmt_item32_t    StopBit;
    uint32_t        StopBitCount = 0;
    uint32_t        DataPattern;

    // #define SERIAL_RMT_DEBUG_COUNTERS
    #ifdef SERIAL_RMT_DEBUG_COUNTERS
    #define INC_SERIAL_RMT_DEBUG_COUNTERS(c) (++SerialRmtDebugCounters.c)
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
    } SerialRmtDebugCounters;
    #else
    #define INC_SERIAL_RMT_DEBUG_COUNTERS(c)
    #endif // def SERIAL_RMT_DEBUG_COUNTERS

}; // c_OutputSerialRmt

#endif // defined(SUPPORT_OutputProtocol_FireGod) || (defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(ARDUINO_ARCH_ESP32)
