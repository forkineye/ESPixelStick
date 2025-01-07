/*
* OutputTM1814.cpp - TM1814 driver code for ESPixelStick UART
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
#ifdef SUPPORT_OutputType_TM1814

#include "output/OutputTM1814.hpp"

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

    c_OutputPixel::Begin ();
    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputTM1814::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);
    JsonWrite(jsonConfig, CN_currentlimit, CurrentLimit);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputPixel::GetStatus (jsonStatus);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputTM1814::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    // Stop current output operation
    c_OutputPixel::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    SetFrameDurration (float (TM1814_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
bool c_OutputTM1814::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    setFromJSON (CurrentLimit, jsonConfig, CN_currentlimit);

    bool response = c_OutputPixel::SetConfig (jsonConfig);

    uint8_t PreambleValue = map (CurrentLimit, 1, 100, 0, 63);
    // DEBUG_V (String (" CurrentLimit: ")   + String (CurrentLimit));
    // DEBUG_V (String ("PreambleValue: 0x") + String (PreambleValue, HEX));
    memset ((void*)(&PreambleData.normal[0]), ~PreambleValue, sizeof (PreambleData.normal));
    memset ((void*)(&PreambleData.inverted[0]),  PreambleValue, sizeof (PreambleData.inverted));
    SetFramePrependInformation ( (uint8_t*)&PreambleData, sizeof (PreambleData));

    SetInvertData (true);

    // Calculate our refresh time
    SetFrameDurration (float (TM1814_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;
    return response;

} // SetConfig
#endif // def SUPPORT_OutputType_TM1814
