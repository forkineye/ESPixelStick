/*
* OutputTM1814.cpp - TM1814 driver code for ESPixelStick UART
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
#include "OutputTM1814.hpp"

#define TM1814_COMMAND_DATA_VALUE               63 // 0x3f ~3F = C1 = 193
static uint8_t PreambleData[8] = { 63, 63, 63, 63, 193, 193, 193, 193 };

//----------------------------------------------------------------------------
c_OutputTM1814::c_OutputTM1814 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = TM1814_MIN_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputTM1814

//----------------------------------------------------------------------------
c_OutputTM1814::~c_OutputTM1814 ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTM1814

//----------------------------------------------------------------------------
void c_OutputTM1814::Begin ()
{
    // DEBUG_START;
    
    // c_OutputPixel::Begin ();

    SetPreambleInformation (PreambleData, sizeof (PreambleData));

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputPixel::GetStatus (jsonStatus);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputTM1814::SetOutputBufferSize (uint16_t NumChannelsAvailable)
{
    // DEBUG_START;

        // Stop current output operation
    c_OutputPixel::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    FrameMinDurationInMicroSec = (TM1814_MICRO_SEC_PER_INTENSITY * OutputBufferSize) + InterFrameGapInMicroSec + TM1814_COMMAND_DATA_TIME_US;

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputTM1814::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputPixel::SetConfig (jsonConfig);

    // Calculate our refresh time
    FrameMinDurationInMicroSec = (TM1814_MICRO_SEC_PER_INTENSITY * numIntensityBytesPerPixel * OutputBufferSize) + InterFrameGapInMicroSec;

    // DEBUG_END;
    return response;

} // SetConfig
