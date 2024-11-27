#pragma once
/*
* OutputUCS1903.h - UCS1903 driver code for ESPixelStick
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

#include "ESPixelStick.h"
#ifdef SUPPORT_OutputType_UCS1903

#include "OutputPixel.hpp"

class c_OutputUCS1903 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputUCS1903 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputUCS1903 ();

    // functions to be provided by the derived class
    virtual void Begin ();
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = CN_UCS1903; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define UCS1903_PIXEL_DATA_RATE              800000.0
#define UCS1903_PIXEL_NS_BIT_TOTAL           ( (1.0 / UCS1903_PIXEL_DATA_RATE) * NanoSecondsInASecond)

#define UCS1903_PIXEL_NS_BIT_0_HIGH          250.0 // 250ns +/- 150ns per datasheet
#define UCS1903_PIXEL_NS_BIT_0_LOW           (UCS1903_PIXEL_NS_BIT_TOTAL - UCS1903_PIXEL_NS_BIT_0_HIGH)

#define UCS1903_PIXEL_NS_BIT_1_HIGH          1000.0 // 1000ns +/- 150ns per datasheet
#define UCS1903_PIXEL_NS_BIT_1_LOW           (UCS1903_PIXEL_NS_BIT_TOTAL - UCS1903_PIXEL_NS_BIT_1_HIGH)

#define UCS1903_PIXEL_IDLE_TIME_NS           25000.0 // 24us per datasheet
#define UCS1903_PIXEL_IDLE_TIME_US           (UCS1903_PIXEL_IDLE_TIME_NS / float(NanoSecondsInAMicroSecond))

#define UCS1903_PIXEL_BITS_PER_INTENSITY     8

}; // c_OutputUCS1903

#endif // def SUPPORT_OutputType_UCS1903
