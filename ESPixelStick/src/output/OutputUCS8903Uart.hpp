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

#if defined(SUPPORT_OutputType_UCS8903) && defined(SUPPORT_UART_OUTPUT)

#include "OutputUCS8903.hpp"

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
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    void    Render ();                                        ///< Call from loop(),  renders output data
    void    SetOutputBufferSize (uint16_t NumChannelsAvailable);
    void    PauseOutput ();
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< UART ISR
    uint32_t IntensityMultiplier = 65535 / 255;

#define UCS8903_NUM_UART_DATA_BYTES_PER_INTENSITY_VALUE 16

private:
    bool validate ();        ///< confirm that the current configuration is valid

}; // c_OutputUCS8903Uart

#endif // defined(SUPPORT_OutputType_UCS8903) && defined(SUPPORT_UART_OUTPUT)