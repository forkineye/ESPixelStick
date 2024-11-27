/*
* OutputWS2801Spi.cpp - WS2801 driver code for ESPixelStick SPI Channel
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
*/

#include "ESPixelStick.h"
#if defined (SUPPORT_OutputType_WS2801) && defined (SUPPORT_SPI_OUTPUT)

#include "output/OutputWS2801Spi.hpp"

//----------------------------------------------------------------------------
c_OutputWS2801Spi::c_OutputWS2801Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputWS2801 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // update frame calculation
    BlockSize = SPI_NUM_INTENSITY_PER_TRANSACTION;
    BlockDelay = 20.0; // measured between 16 and 21 us

    // DEBUG_END;
} // c_OutputWS2801Spi

//----------------------------------------------------------------------------
c_OutputWS2801Spi::~c_OutputWS2801Spi ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_OutputWS2801Spi

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2801Spi::Begin ()
{
    // DEBUG_START;
    Spi.Begin (this);

    HasBeenInitialized = true;

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputWS2801Spi::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputWS2801::GetConfig (jsonConfig);
    Spi.GetConfig(jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputWS2801Spi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputWS2801::SetConfig (jsonConfig);
    response |= Spi.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputWS2801Spi::Poll ()
{
    // DEBUG_START;

    uint32_t FrameLen = ActualFrameDurationMicroSec;

    if (canRefresh ())
    {
        if (Spi.Poll ())
            {
            ReportNewFrame ();
            }
    }
    else
    {
        FrameLen = 0;
    }

    // DEBUG_END;
    return FrameLen;

} // render

#endif // defined (SUPPORT_OutputType_WS2801) && defined (SUPPORT_SPI_OUTPUT)
