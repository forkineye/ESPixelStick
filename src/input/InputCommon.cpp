/*
* InputCommon.cpp - Input Interface base class
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
*   This is an Interface base class used to manage the Input port. It provides
*   a common API for use by the factory class to manage the object.
*
*/

#include "ESPixelStick.h"
#include "input/InputCommon.hpp"

//-------------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_InputCommon::c_InputCommon (c_InputMgr::e_InputChannelIds NewInputChannelId,
                              c_InputMgr::e_InputType       NewChannelType,
                              uint32_t                        BufferSize) :
    InputDataBufferSize(BufferSize),
    InputChannelId(NewInputChannelId),
    ChannelType(NewChannelType)
{
} // c_InputMgr

//-------------------------------------------------------------------------------
// deallocate any resources and put the Input channels into a safe state
c_InputCommon::~c_InputCommon ()
{
    // DEBUG_START;

    OutputMgr.ClearBuffer ();

    // DEBUG_END;

} // ~c_InputMgr
