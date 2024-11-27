/*
* OutputAPA102Spi.cpp - APA102 driver code for ESPixelStick SPI Channel
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
#if defined (SUPPORT_OutputType_APA102) && defined (SUPPORT_SPI_OUTPUT)

#include "output/OutputAPA102Spi.hpp"

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
    Spi.GetConfig(jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputAPA102Spi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputAPA102::SetConfig (jsonConfig);
    response |= Spi.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputAPA102Spi::Poll ()
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

#endif // defined(SUPPORT_OutputType_APA102) && defined(SUPPORT_SPI_OUTPUT)
