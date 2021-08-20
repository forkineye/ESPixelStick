#pragma once
/*
* OutputWS2801.h - WS2801 driver code for ESPixelStick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#ifdef USE_WS2801
#include "OutputPixel.hpp"

class c_OutputWS2801 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2801 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2801 ();

    // functions to be provided by the derived class
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void         GetDriverName (String & sDriverName) { sDriverName = String (F ("WS2801")); }
    c_OutputMgr::e_OutputType GetOutputType () {return c_OutputMgr::e_OutputType::OutputType_WS2801;} ///< Have the instance report its type.
    virtual void         GetStatus (ArduinoJson::JsonObject& jsonStatus);
    virtual void         SetOutputBufferSize (uint16_t NumChannelsAvailable);

protected:

#define WS2801_PIXEL_NS_BIT_0_HIGH          350.0 // 350ns +/- 150ns per datasheet
#define WS2801_PIXEL_NS_BIT_0_LOW           900.0 // 900ns +/- 150ns per datasheet
#define WS2801_PIXEL_NS_BIT_1_HIGH          900.0 // 900ns +/- 150ns per datasheet
#define WS2801_PIXEL_NS_BIT_1_LOW           350.0 // 350ns +/- 150ns per datasheet
#define WS2801_PIXEL_NS_IDLE             300000.0 // 300us per datasheet

#define WS2801_MICRO_SEC_PER_INTENSITY          10L     // ((1/800000) * 8 bits) = 10us
#define WS2801_MIN_IDLE_TIME_US                 (WS2801_PIXEL_NS_IDLE / 1000.0)
// #define WS2801_DEFAULT_INTENSITY_PER_PIXEL      3

}; // c_OutputWS2801
#endif // def USE_WS2801

