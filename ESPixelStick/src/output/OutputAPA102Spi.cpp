#ifdef ARDUINO_ARCH_ESP32
/*
* OutputAPA102Spi.cpp - APA102 driver code for ESPixelStick SPI Channel
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
*/

#include "../ESPixelStick.h"
#include "OutputAPA102Spi.hpp"

//----------------------------------------------------------------------------
c_OutputAPA102Spi::c_OutputAPA102Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputAPA102 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // update frame calculation 
    BlockSize = SPI_NUM_INTENSITY_PER_TRANSACTION;
    BlockDelay = 20.0; // measured between 16 and 21 us

    // DEBUG_END;
} // c_OutputAPA102Spi

//----------------------------------------------------------------------------
c_OutputAPA102Spi::~c_OutputAPA102Spi ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_OutputAPA102Spi

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputAPA102Spi::Begin ()
{
    // DEBUG_START;
    Spi.Begin (this);

    SetFramePrependInformation ( (uint8_t*)&FrameStartData, sizeof (FrameStartData));
    SetFrameAppendInformation  ( (uint8_t*)&FrameEndData,   sizeof (FrameEndData));
    SetPixelPrependInformation ( (uint8_t*)&PixelStartData, sizeof (PixelStartData));

    HasBeenInitialized = true;

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputAPA102Spi::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputAPA102::GetConfig (jsonConfig);

    jsonConfig[CN_clock_pin] = DEFAULT_SPI_CLOCK_GPIO;

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputAPA102Spi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputAPA102::SetConfig (jsonConfig);

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputAPA102Spi::Render ()
{
    // DEBUG_START;

    if (canRefresh ())
    {
        if (Spi.Render ())
        {
            ReportNewFrame ();
        }
    }

    // DEBUG_END;

} // render

#endif // def ARDUINO_ARCH_ESP32
