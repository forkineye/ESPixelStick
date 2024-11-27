#pragma once
/*
* OutputTM1814Uart.h - TM1814 driver code for ESPixelStick UART
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
#if defined(SUPPORT_OutputType_TM1814)

#include "OutputTM1814.hpp"
#include "OutputUart.hpp"

class c_OutputTM1814Uart : public c_OutputTM1814
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTM1814Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputTM1814Uart ();

    // functions to be provided by the derived class
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
    c_OutputUart Uart;

#define TM1814_DATA_RATE                            (800000.0)

#define TM1814_NUM_DATA_BITS_PER_INTENSITY_BIT      11
#define TM1814_NUM_DATA_BYTES_PER_INTENSITY_BYTE    8
#define TM1814_BAUD_RATE                            int(TM1814_DATA_RATE * TM1814_NUM_DATA_BITS_PER_INTENSITY_BIT)

}; // c_OutputTM1814Uart

#endif // defined(SUPPORT_OutputType_TM1814)
