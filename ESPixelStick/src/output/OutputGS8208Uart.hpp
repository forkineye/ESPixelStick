#pragma once
/*
* OutputGS8208Uart.h - GS8208 driver code for ESPixelStick UART
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

#include "OutputCommon.hpp"

#if defined(SUPPORT_OutputType_GS8208) && defined(SUPPORT_UART_OUTPUT)

#include "OutputGS8208.hpp"

class c_OutputGS8208Uart : public c_OutputGS8208
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputGS8208Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGS8208Uart ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    void    Render ();                                        ///< Call from loop(),  renders output data
    void    SetOutputBufferSize (uint16_t NumChannelsAvailable);
    void    PauseOutput ();
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< UART ISR

#define GS8208_NUM_DATA_BYTES_PER_INTENSITY_BYTE    4

private:
    bool validate ();        ///< confirm that the current configuration is valid

}; // c_OutputGS8208Uart

#endif // defined(SUPPORT_OutputType_GS8208) && defined(SUPPORT_UART_OUTPUT)
