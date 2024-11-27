#pragma once
/*
* OutputWS2811Uart.h - WS2811 driver code for ESPixelStick UART
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
#if defined(SUPPORT_OutputType_WS2811)

#include "OutputWS2811.hpp"
#include "OutputUart.hpp"

class c_OutputWS2811Uart : public c_OutputWS2811
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                        gpio_num_t outputGpio,
                        uart_port_t uart,
                        c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2811Uart ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    uint32_t Poll ();
#if defined(ARDUINO_ARCH_ESP32)
    bool     RmtPoll () {return false;}
#endif // defined(ARDUINO_ARCH_ESP32)
    void    PauseOutput (bool State);
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetConfig (ArduinoJson::JsonObject& jsonConfig);
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);

#define WS2811_NUM_DATA_BYTES_PER_INTENSITY_BYTE    4

private:
    c_OutputUart Uart;
#ifdef WS2811_UART_DEBUG_COUNTERS
    uint32_t NewFrameCounter = 0;
    uint32_t TimeSinceLastFrameMS = 0;
    uint32_t TimeLastFrameStartedMS = 0;
#endif // def WS2811_UART_DEBUG_COUNTERS

}; // c_OutputWS2811Uart

#endif // defined(SUPPORT_OutputType_WS2811)
