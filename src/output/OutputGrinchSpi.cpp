/*
* OutputGrinchSpi.cpp - GRINCH driver code for ESPixelStick SPI Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2024 Shelby Merrick
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
#if defined (SUPPORT_OutputProtocol_GRINCH)

#include "output/OutputGrinchSpi.hpp"

//----------------------------------------------------------------------------
c_OutputGrinchSpi::c_OutputGrinchSpi (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputGrinch (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // update frame calculation

    // DEBUG_END;
} // c_OutputGrinchSpi

//----------------------------------------------------------------------------
c_OutputGrinchSpi::~c_OutputGrinchSpi ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_OutputGrinchSpi

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputGrinchSpi::Begin ()
{
    // DEBUG_START;
    Spi.Begin (OutputPortDefinition, this);
    HasBeenInitialized = true;

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputGrinchSpi::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputGrinch::GetConfig (jsonConfig);
    Spi.GetConfig (jsonConfig);

    // PrettyPrint(jsonConfig, "Grinch");

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputGrinchSpi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGrinch::SetConfig (jsonConfig);
    response |= Spi.SetConfig (jsonConfig);

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputGrinchSpi::Poll ()
{
    // DEBUG_START;

    uint32_t FrameLen = ActualFrameDurationMicroSec;

    if (canRefresh ())
    {
        if (Spi.Poll ())
        {
            ISR_ReportNewFrame ();
        }
    }
    else
    {
        FrameLen = 0;
    }

    // DEBUG_END;
    return FrameLen;

} // render

#endif // defined (SUPPORT_OutputProtocol_GRINCH)
