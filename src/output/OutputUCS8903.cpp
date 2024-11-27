/*
* OutputUCS8903.cpp - UCS8903 driver code for ESPixelStick UART
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

#include "ESPixelStick.h"
#include "output/OutputUCS8903.hpp"
#ifdef SUPPORT_OutputType_UCS8903

//----------------------------------------------------------------------------
c_OutputUCS8903::c_OutputUCS8903 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = UCS8903_PIXEL_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputUCS8903

//----------------------------------------------------------------------------
c_OutputUCS8903::~c_OutputUCS8903 ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputUCS8903

//----------------------------------------------------------------------------
void c_OutputUCS8903::Begin ()
{
    // DEBUG_START;

    c_OutputPixel::Begin ();
    SetIntensityDataWidth(UCS8903_INTENSITY_DATA_WIDTH);
    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputUCS8903::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputUCS8903::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // DEBUG_START;
    c_OutputPixel::GetStatus(jsonStatus);
    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputUCS8903::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    // Stop current output operation
    c_OutputPixel::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    SetFrameDurration(float(UCS8903_PIXEL_NS_BIT_0_HIGH + UCS8903_PIXEL_NS_BIT_0_LOW) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
bool c_OutputUCS8903::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputPixel::SetConfig (jsonConfig);
    // DEBUG_V();

    // Calculate our refresh time
    SetFrameDurration(float(UCS8903_PIXEL_NS_BIT_0_HIGH + UCS8903_PIXEL_NS_BIT_0_LOW) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;
    return response;

} // SetConfig

#endif // def SUPPORT_OutputType_UCS8903
