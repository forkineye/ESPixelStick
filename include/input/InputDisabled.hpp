#pragma once
/*
* InputDisabled.h - Do Nothing input driver
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2025 Shelby Merrick
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
*   This is a derived class that converts data in the Input buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "InputCommon.hpp"

class c_InputDisabled : public c_InputCommon
{
public:
    // These functions are inherited from c_InputCommon
    c_InputDisabled (c_InputMgr::e_InputChannelIds NewInputChannelId,
                     c_InputMgr::e_InputType       NewChannelType,
                     uint32_t                        BufferSize);
    virtual ~c_InputDisabled ();

    // functions to be provided by the derived class
    void Begin ();                              ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject & jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject & jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject & jsonStatus);
    void Process   ();
    void GetDriverName (String& sDriverName) { sDriverName = "Disabled"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint32_t BufferSize) {}

private:

}; // c_InputDisabled
