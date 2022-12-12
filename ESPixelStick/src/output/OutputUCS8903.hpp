#pragma once
/*
* OutputUCS8903.h - UCS8903 driver code for ESPixelStick
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

#include "OutputPixel.hpp"

#ifdef SUPPORT_OutputType_UCS8903
#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputUCS8903 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputUCS8903 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputUCS8903 ();

    // functions to be provided by the derived class
    virtual void Begin ();
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = CN_UCS8903; }
    virtual void GetStatus (ArduinoJson::JsonObject& jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

// #define UCS8903_PIXEL_DATA_RATE              800000.0
#define UCS8903_PIXEL_DATA_RATE             (1 / ((UCS8903_PIXEL_NS_BIT_TOTAL) / NanoSecondsInASecond ))
// #define UCS8903_PIXEL_NS_BIT_TOTAL           ((1.0 / UCS8903_PIXEL_DATA_RATE) * NanoSecondsInASecond)
#define UCS8903_PIXEL_NS_BIT_TOTAL           (UCS8903_PIXEL_NS_BIT_0_HIGH + UCS8903_PIXEL_NS_BIT_0_LOW)

#define UCS8903_PIXEL_NS_BIT_0_HIGH          400.0 // 400ns +/- 40ns per datasheet
#define UCS8903_PIXEL_NS_BIT_0_LOW           800.0 // 800ns -0 per data sheet

#define UCS8903_PIXEL_NS_BIT_1_HIGH          800.0 // 800ns +/- 150ns per datasheet
#define UCS8903_PIXEL_NS_BIT_1_LOW           400.0 // 400ns -0 per data sheet

#define UCS8903_PIXEL_IDLE_TIME_NS           30000.0 // >24us per datasheet
#define UCS8903_PIXEL_IDLE_TIME_US           (UCS8903_PIXEL_IDLE_TIME_NS / float(NanoSecondsInAMicroSecond))
#define UCS8903_INTENSITY_DATA_WIDTH         16

}; // c_OutputUCS8903
#endif // def SUPPORT_OutputType_UCS8903
