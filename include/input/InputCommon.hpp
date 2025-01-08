#pragma once
/*
* InputCommon.hpp - Input base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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
*   This is a base class used to manage the Input port. It provides a common API
*   for use by the factory class to manage the object.
*/

#include "ESPixelStick.h"
#include "InputMgr.hpp"

class c_InputCommon
{
public:
    c_InputCommon (c_InputMgr::e_InputChannelIds NewInputChannelId,
                   c_InputMgr::e_InputType       NewChannelType,
                   uint32_t                        BufferSize);
    virtual ~c_InputCommon ();

    // functions to be provided by the derived class
    virtual void Begin () = 0;///< set up the operating environment based on the current config (or defaults)
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig) = 0; ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig) = 0; ///< Get the current config used by the driver
    virtual void GetStatus (JsonObject & jsonStatus) = 0;
    virtual void Process (void) = 0;                                       ///< Call from loop(),  renders Input data
    virtual void GetDriverName (String & sDriverName) = 0;             ///< get the name for the instantiated driver
    virtual void SetBufferInfo (uint32_t BufferSize) = 0;
    virtual void SetOperationalState (bool ActiveFlag) { IsInputChannelActive = ActiveFlag; }
    virtual void NetworkStateChanged (bool IsConnected) {}; // used by poorly designed rx functions
    virtual bool isShutDownRebootNeeded () { return false; }
    virtual void ProcessButtonActions(c_ExternalInput::InputValue_t value) {};

    c_InputMgr::e_InputChannelIds GetInputChannelId () { return InputChannelId; }
    c_InputMgr::e_InputType       GetInputType ()      { return ChannelType; }

protected:
    bool        HasBeenInitialized  = false;
    uint32_t    InputDataBufferSize = 0;
    bool        IsInputChannelActive = true;
    c_InputMgr::e_InputChannelIds InputChannelId = c_InputMgr::e_InputChannelIds::InputChannelId_ALL;
    c_InputMgr::e_InputType       ChannelType = c_InputMgr::e_InputType::InputType_Disabled;

private:

}; // c_InputCommon
