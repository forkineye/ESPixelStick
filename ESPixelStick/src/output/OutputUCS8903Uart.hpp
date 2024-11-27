#pragma once
/*
* OutputUCS8903Uart.h - UCS8903 driver code for ESPixelStick UART
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, bor arising out of, the furnishing, performance
*  or use of these programs.
*
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "OutputCommon.hpp"

#if defined(SUPPORT_OutputType_UCS8903)

#include "OutputUCS8903.hpp"
#include "OutputUart.hpp"

class c_OutputUCS8903Uart : public c_OutputUCS8903
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputUCS8903Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputUCS8903Uart ();

    // functions to be provided by the derived class
    void    Begin ();
    uint32_t Poll ();
#if defined(ARDUINO_ARCH_ESP32)
    bool     RmtPoll () {return false;}
#endif // defined(ARDUINO_ARCH_ESP32)
    void    PauseOutput (bool State);
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);

private:
    c_OutputUart Uart;
#ifdef UCS8903_UART_DEBUG_COUNTERS
    uint32_t NewFrameCounter        = 0;
    uint32_t TimeSinceLastFrameMS   = 0;
    uint32_t TimeLastFrameStartedMS = 0;
#endif // def UCS8903_UART_DEBUG_COUNTERS

}; // c_OutputUCS8903Uart

#endif // defined(SUPPORT_OutputType_UCS8903)
