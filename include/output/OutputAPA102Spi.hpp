#pragma once
/*
* OutputAPA102Spi.h - APA102 driver code for ESPixelStick Spi Channel
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
#if defined(SUPPORT_OutputType_APA102) && defined(SUPPORT_SPI_OUTPUT)

#include "OutputAPA102.hpp"
#include "OutputSpi.hpp"

class c_OutputAPA102Spi : public c_OutputAPA102
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputAPA102Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputAPA102Spi ();

    // functions to be provided by the derived class
    void    Begin ();
    void    GetConfig (ArduinoJson::JsonObject& jsonConfig);
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    uint32_t Poll ();                                        ///< Call from loop (),  renders output data
#if defined(ARDUINO_ARCH_ESP32)
    bool     RmtPoll () {return false;}
#endif // defined(ARDUINO_ARCH_ESP32)

private:

    c_OutputSpi Spi;

}; // c_OutputAPA102Spi

#endif // defined(SUPPORT_OutputType_APA102) && defined(SUPPORT_SPI_OUTPUT)
