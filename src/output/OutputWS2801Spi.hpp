#pragma once
/*
* OutputWS2801Spi.h - WS2801 driver code for ESPixelStick Spi Channel
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
#ifdef USE_WS2801

#include "OutputWS2801.hpp"

#include <driver/spi_master.h>

class c_OutputWS2801Spi : public c_OutputWS2801
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2801Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2801Spi ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    void    Render ();                                        ///< Call from loop(),  renders output data
    void    PauseOutput () {};

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< ISR
    void IRAM_ATTR ISR_Handler_SendIntensityData (); ///< ISR
    void IRAM_ATTR ISR_Handler_StartNewFrame (); ///< ISR

private:

#define SPI_MASTER_FREQ_1M      (APB_CLK_FREQ/80) // 1Mhz

    uint8_t NumIntensityValuesPerInterrupt = 0;
    uint8_t NumIntensityBitsPerInterrupt = 0;

    uint32_t FrameStartCounter = 0;
    // uint32_t DataISRcounter = 0;
    // uint32_t FrameDoneCounter = 0;
    // uint32_t FrameEndISRcounter = 0;

}; // c_OutputWS2801Spi

#endif // def USE_WS2801
