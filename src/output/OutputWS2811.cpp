/*
* OutputWS2811.cpp - WS2811 driver code for ESPixelStick UART
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
#include "output/OutputWS2811.hpp"
#if defined(SUPPORT_OutputType_WS2811) 

//----------------------------------------------------------------------------
c_OutputWS2811::c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = WS2811_PIXEL_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputWS2811

//----------------------------------------------------------------------------
c_OutputWS2811::~c_OutputWS2811 ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputWS2811

//----------------------------------------------------------------------------
void c_OutputWS2811::Begin ()
{
    // DEBUG_START;

    c_OutputPixel::Begin ();
    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputWS2811::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputPixel::GetStatus (jsonStatus);

    // jsonStatus["canRefresh"] = canRefresh();
    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputWS2811::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputPixel::SetConfig (jsonConfig);

    // Calculate our refresh time
    
    // DEBUG_V(String("   WS2811_PIXEL_DATA_RATE: ") + String(WS2811_PIXEL_DATA_RATE));
    // DEBUG_V(String("WS2811_PIXEL_NS_BIT_TOTAL: ") + String(WS2811_PIXEL_NS_BIT_TOTAL));
    SetFrameDurration(float(WS2811_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;
    return response;

} // SetConfig
#endif // defined(SUPPORT_OutputType_WS2811)
