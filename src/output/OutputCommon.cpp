/*
* OutputCommon.cpp - Output Interface base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
*   This is an Interface base class used to manage the output port. It provides
*   a common API for use by the factory class to manage the object.
*
*/

#include "ESPixelStick.h"
#include "output/OutputCommon.hpp"

//-------------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputCommon::c_OutputCommon (c_OutputMgr::e_OutputChannelIds _OutputChannelId,
	                            gpio_num_t outputGpio,
	                            uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType)
{
	// remember what channel we are
	HasBeenInitialized       = false;
	OutputChannelId          = _OutputChannelId;
	DataPin                  = outputGpio;
	UartId                   = uart;
    OutputType               = outputType;
    pOutputBuffer            = OutputMgr.GetBufferAddress ();
    FrameStartTimeInMicroSec = 0;

	// logcon (String ("UartId:          '") + UartId + "'");
    // logcon (String ("OutputChannelId: '") + OutputChannelId + "'");
    // logcon (String ("OutputType:      '") + OutputType + "'");

} // c_OutputCommon

//-------------------------------------------------------------------------------
// deallocate any resources and put the output channels into a safe state
c_OutputCommon::~c_OutputCommon ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputCommon

//-----------------------------------------------------------------------------
void c_OutputCommon::ClearBuffer ()
{
    // DEBUG_START;

    memset(GetBufferAddress(), 0x00, OutputMgr.GetBufferSize());

    // DEBUG_END;
} // ClearBuffer

//-----------------------------------------------------------------------------
void c_OutputCommon::BaseGetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonWrite(jsonStatus, CN_id,                 OutputChannelId);
    JsonWrite(jsonStatus, F("framerefreshrate"), int(MicroSecondsInASecond / FrameDurationInMicroSec));
    JsonWrite(jsonStatus, F("FrameCount"),       FrameCount);

    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputCommon::ReportNewFrame ()
{
    // DEBUG_START;

    uint32_t Now = micros ();

    FrameStartTimeInMicroSec    = Now;
    FrameCount++;

    // DEBUG_END;

} // ReportNewFrame

//----------------------------------------------------------------------------
bool c_OutputCommon::SetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    uint8_t tempDataPin = uint8_t (DataPin);

    bool response = setFromJSON (tempDataPin, jsonConfig, CN_data_pin);

    DataPin = gpio_num_t (tempDataPin);
    // DEBUG_V(String(" DataPin: ") + String(DataPin));

    // DEBUG_END;

    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputCommon::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    JsonWrite(jsonConfig, CN_data_pin, uint8_t (DataPin));

    // DEBUG_V(String(" DataPin: ") + String(DataPin));

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputCommon::WriteChannelData (uint32_t StartChannelId, uint32_t ChannelCount, byte * pSourceData)
{
    // DEBUG_START;

    // DEBUG_V(String("               StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
    memcpy(&pOutputBuffer[StartChannelId], pSourceData, ChannelCount);

    // DEBUG_END;

} // WriteChannelData

//----------------------------------------------------------------------------
void c_OutputCommon::ReadChannelData(uint32_t StartChannelId, uint32_t ChannelCount, byte * pTargetData)
{
    // DEBUG_START;

    // DEBUG_V(String("               StartChannelId: 0x") + String(StartChannelId, HEX));
    // DEBUG_V(String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
    memcpy(pTargetData, &pOutputBuffer[StartChannelId], ChannelCount);

    // DEBUG_END;

} // WriteChannelData

//----------------------------------------------------------------------------
bool c_OutputCommon::ValidateGpio (gpio_num_t ConsoleTxGpio, gpio_num_t ConsoleRxGpio)
{
    // DEBUG_START;

    bool response = ((ConsoleTxGpio == DataPin) || (ConsoleRxGpio == DataPin));

    // DEBUG_END;
    return response;
} // ValidateGpio
