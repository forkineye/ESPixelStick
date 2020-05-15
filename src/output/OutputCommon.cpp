/*
* OutputCommon.cpp - Output Interface base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#include "OutputCommon.hpp"

//-------------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputCommon::c_OutputCommon (c_OutputMgr::e_OutputChannelIds iOutputChannelId, 
	                            gpio_num_t outputGpio, 
	                            uart_port_t uart)
{
	// remember what channel we are
	HasBeenInitialized = false;
	OutputChannelId    = iOutputChannelId;
	DataPin            = outputGpio;
	UartId             = uart;
	 
	// clear the input data buffer
	memset ((char*)&InputDataBuffer[0], 0, sizeof (InputDataBuffer));

} // c_OutputMgr

//-------------------------------------------------------------------------------
// deallocate any resources and put the output channels into a safe state
c_OutputCommon::~c_OutputCommon ()
{
} // ~c_OutputMgr


