#pragma once
/*
* OutputGS8208.h - GS8208 driver code for ESPixelStick
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

#ifdef SUPPORT_OutputType_GS8208
#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputGS8208 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputGS8208 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGS8208 ();

    // functions to be provided by the derived class
    virtual void Begin ();
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = CN_GS8208; }
    virtual void GetStatus (ArduinoJson::JsonObject& jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define GS8208_PIXEL_DATA_RATE              1000000.0
#define GS8208_PIXEL_NS_BIT_TOTAL           ( (1.0 / GS8208_PIXEL_DATA_RATE) * float(NanoSecondsInASecond))

#define GS8208_PIXEL_NS_BIT_0_HIGH          (0.25 *  GS8208_PIXEL_NS_BIT_TOTAL) // 250ns +/- 150ns per datasheet
#define GS8208_PIXEL_NS_BIT_0_LOW           (GS8208_PIXEL_NS_BIT_TOTAL - GS8208_PIXEL_NS_BIT_0_HIGH)

#define GS8208_PIXEL_NS_BIT_1_HIGH          (0.75 * GS8208_PIXEL_NS_BIT_TOTAL) // 600ns +/- 150ns per datasheet
#define GS8208_PIXEL_NS_BIT_1_LOW           (GS8208_PIXEL_NS_BIT_TOTAL - GS8208_PIXEL_NS_BIT_1_HIGH)

#define GS8208_PIXEL_IDLE_TIME_NS           300000.0 // 300us per datasheet
#define GS8208_PIXEL_IDLE_TIME_US           (GS8208_PIXEL_IDLE_TIME_NS / float(NanoSecondsInAMicroSecond))

#define GS8208_PIXEL_BITS_PER_INTENSITY     8

}; // c_OutputGS8208

#endif // def SUPPORT_OutputType_GS8208
