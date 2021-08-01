#pragma once
/*
* OutputWS2811Rmt.h - WS2811 driver code for ESPixelStick RMT Channel
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
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "OutputCommon.hpp"
#include "OutputWS2811.hpp"

#ifdef ARDUINO_ARCH_ESP32
// #   include <driver/uart.h>
#endif

class c_OutputWS2811Rmt : public c_OutputWS2811
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2811Rmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    void    Render ();                                        ///< Call from loop(),  renders output data
    void    SetOutputBufferSize (uint16_t NumChannelsAvailable);
    void    PauseOutput ();

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< ISR

#define WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE    4
#define WS2812_NUM_DATA_BYTES_PER_PIXEL             16

}; // c_OutputWS2811Rmt

