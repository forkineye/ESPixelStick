/*
* InputDisabled.cpp - InputDisabled NULL driver code for ESPixelStick
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

#include <Arduino.h>
#include "../ESPixelStick.h"

#include "InputDisabled.hpp"
#include "InputCommon.hpp"

/*
*/
//----------------------------------------------------------------------------
c_InputDisabled::c_InputDisabled (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                  c_InputMgr::e_InputType       NewChannelType,
                                  uint8_t* BufferStart,
                                  uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_InputDisabled

//----------------------------------------------------------------------------
c_InputDisabled::~c_InputDisabled()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_InputDisabled

//----------------------------------------------------------------------------
// Use the current config to set up the Input port
void c_InputDisabled::Begin()
{
    // DEBUG_START;

    Serial.println (String(F ("** Input Disabled Initialization for Chan: ")) + String(InputChannelId) +  " **");

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputDisabled::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonObject e131Status = jsonStatus.createNestedObject (F ("e131"));
    e131Status["unifirst"]   = 0;
    e131Status["unilast"]    = 0;
    e131Status["unichanlim"] = 0;
    // DEBUG_V ("");

    e131Status["num_packets"]   = 0;
    e131Status["packet_errors"] = 0;
    e131Status["last_clientIP"] = "0.0.0.0";

    // DEBUG_END;

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
bool c_InputDisabled::SetConfig(ArduinoJson::JsonObject & /* jsonConfig */)
{
    // DEBUG_START;

    // DEBUG_END;
    return true;

} // SetConfig

//----------------------------------------------------------------------------
void c_InputDisabled::GetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    // DEBUG_END;

} // GetConfig

void c_InputDisabled::Process()
{
    // DEBUG_START;

    // DEBUG_END;

} // Render
