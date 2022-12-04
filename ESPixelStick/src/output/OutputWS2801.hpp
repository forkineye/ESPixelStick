#pragma once
/*
* OutputWS2801.h - WS2801 driver code for ESPixelStick
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
#include "OutputPixel.hpp"
#ifdef SUPPORT_OutputType_WS2801

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
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = CN_WS2801; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:
#define WS2801_BIT_RATE                 (APB_CLK_FREQ/80)
#define WS2801_BITS_PER_INTENSITY       8
#define WS2801_MICRO_SEC_PER_INTENSITY  int(((1.0/float(WS2801_BIT_RATE)) * WS2801_BITS_PER_INTENSITY))
#define WS2801_MIN_IDLE_TIME_US         500
    uint16_t    BlockSize = 1;
    float       BlockDelay = 0;

}; // c_OutputWS2801
#endif // def SUPPORT_OutputType_WS2801
