#pragma once
/*
* OutputGECEUart.h - GECE driver code for ESPixelStick UART
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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
#if defined(SUPPORT_OutputType_GECE)

#include "OutputGECE.hpp"
#include "OutputUart.hpp"

class c_OutputGECEUart : public c_OutputGECE
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputGECEUart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGECEUart ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    uint32_t Poll ();                                        ///< Call from loop(),  renders output data
#if defined(ARDUINO_ARCH_ESP32)
    bool     RmtPoll () {return false;}
#endif // defined(ARDUINO_ARCH_ESP32)
    void    PauseOutput (bool State);
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);

private:

#define GECE_UART_BITS_PER_GECE_BIT 5.0
#define GECE_UART_USEC_PER_BIT      (float(GECE_USEC_PER_GECE_BIT) / float(GECE_UART_BITS_PER_GECE_BIT))
#define GECE_BAUDRATE               uint32_t((1.0 / (GECE_UART_USEC_PER_BIT / float(MicroSecondsInASecond))))
#define GECE_UART_BREAK_BITS        uint32_t(((GECE_PIXEL_STOP_TIME_NS / NanoSecondsInAMicroSecond) / GECE_UART_USEC_PER_BIT) + 1)

        c_OutputUart Uart;

// #define GECE_UART_DEBUG_COUNTERS
#ifdef GECE_UART_DEBUG_COUNTERS
    uint32_t NewFrameCounter = 0;
    uint32_t TimeSinceLastFrameMS = 0;
    uint32_t TimeLastFrameStartedMS = 0;
#endif // def GECE_UART_DEBUG_COUNTERS

}; // c_OutputGECEUart

#endif // defined(SUPPORT_OutputType_GECE)
