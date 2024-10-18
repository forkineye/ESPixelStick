#pragma once
/*
* OutputDisabled.h - WS2811 driver code for ESPixelStick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "OutputCommon.hpp"

class c_OutputDisabled : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputDisabled (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputDisabled ();

    // functions to be provided by the derived class
    void         Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void         GetStatus (ArduinoJson::JsonObject & jsonConfig) {}
    uint32_t     Poll ();                                          ///< Call from loop(),  renders output data
#ifdef ARDUINO_ARCH_ESP32
    bool         RmtPoll () {return false;}
#endif // def ARDUINO_ARCH_ESP32
    void         GetDriverName (String & sDriverName) { sDriverName = CN_Disabled; }
    uint32_t     GetNumOutputBufferBytesNeeded () { return 0; }
    uint32_t     GetNumOutputBufferChannelsServiced () { return 0; }

private:

}; // c_OutputDisabled
