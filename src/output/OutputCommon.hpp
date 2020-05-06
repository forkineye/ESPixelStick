// only include this file once
#pragma once
/*
* OutputCommon.hpp - Output base class
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
*   This is a base class used to manage the output port. It provides a common API
*   for use by the factory class to manage the object.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include "OutputMgr.hpp"

class c_OutputCommon
{
public:
    c_OutputCommon (c_OutputMgr::e_OutputChannelIds OutputChannelId);
    virtual ~c_OutputCommon ();

    // functions to be provided by the derived class
    virtual void         begin () = 0;                                         ///< set up the operating environment based on the current config (or defaults)
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig) = 0; ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig) = 0; ///< Get the current config used by the driver
            uint8_t    * GetBufferAddress () {return InputDataBuffer;}         ///< Get the address of the buffer into which the E1.31 handler will stuff data
            uint16_t     GetBufferSize () {return sizeof (InputDataBuffer);}   ///< Get the address of the buffer into which the E1.31 handler will stuff data
    virtual void         render () = 0;                                        ///< Call from loop(),  renders output data
    virtual void         GetDriverName (String & sDriverName) = 0;             ///< get the name for the instantiated driver
    virtual c_OutputMgr::e_OutputType GetOutputType () = 0;                    ///< Have the instance report its type.
    c_OutputMgr::e_OutputChannelIds GetOuputChannelId () { return OutputChannelId; } ///< return the output channel number
protected:

#define MAX_NUM_PIXELS                         1360
#define WS2812_NUM_INTENSITY_BYTES_PER_PIXEL   3
#define INPUT_BUFFER_SIZE                      (MAX_NUM_PIXELS * WS2812_NUM_INTENSITY_BYTES_PER_PIXEL)
    
private:
    uint8_t InputDataBuffer[INPUT_BUFFER_SIZE];
    c_OutputMgr::e_OutputChannelIds OutputChannelId;

}; // c_OutputCommon
