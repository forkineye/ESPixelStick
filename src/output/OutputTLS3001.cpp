/*
* OutputTLS3001.cpp - TLS3001 driver code for ESPixelStick UART
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
#ifdef SUPPORT_OutputType_TLS3001

#include "output/OutputTLS3001.hpp"

//----------------------------------------------------------------------------
c_OutputTLS3001::c_OutputTLS3001 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = TLS3001_MIN_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputTLS3001

//----------------------------------------------------------------------------
c_OutputTLS3001::~c_OutputTLS3001 ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTLS3001

//----------------------------------------------------------------------------
void c_OutputTLS3001::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputTLS3001::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputPixel::GetStatus (jsonStatus);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputTLS3001::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

        // Stop current output operation
    c_OutputPixel::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    SetFrameDurration (((1.0 / float (TLS3001_PIXEL_NS_BIT)) * MicroSecondsInASecond), 0, 0);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
bool c_OutputTLS3001::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputPixel::SetConfig (jsonConfig);

    // Calculate our refresh time
    SetFrameDurration (((1.0 / float (TLS3001_PIXEL_NS_BIT)) * MicroSecondsInASecond), 0, 0);

    // DEBUG_END;
    return response;

} // SetConfig

#endif // def SUPPORT_OutputType_TLS3001
