#pragma once
/*
* OutputWS2811.h - WS2811 driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2024 Shelby Merrick
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
#if defined(SUPPORT_OutputType_WS2811)

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputWS2811 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2811 ();

    // functions to be provided by the derived class
    virtual void Begin ();
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
            void GetDriverName (String & sDriverName) { sDriverName = CN_WS2811; }

protected:

#define WS2811_PIXEL_DATA_RATE              800000.0 // 800Khz
#define WS2811_PIXEL_NS_BIT_TOTAL           ( (1.0 / WS2811_PIXEL_DATA_RATE) * NanoSecondsInASecond)
#ifdef ARDUINO_ARCH_ESP32
                                            // values have been adjusted to work with seed pixels.
//    #define WS2811_PIXEL_NS_BIT_0_HIGH      312.0 // = 312 on logic analyzer 220ns - 380ns per datasheet
//    #define WS2811_PIXEL_NS_BIT_0_LOW       945.0 // = 937 on logic analyzer 580ns - 1.6us per datasheet
//    #define WS2811_PIXEL_NS_BIT_1_HIGH      975.0 // = 937 on logic analyzer 580ns - 1.6us per datasheet
//    #define WS2811_PIXEL_NS_BIT_1_LOW       300.0 // = 312 on logic analyzer 220ns - 380ns per datasheet
    #define WS2811_PIXEL_NS_BIT_0_HIGH      300.0 // 220ns - 380ns per datasheet
    #define WS2811_PIXEL_NS_BIT_0_LOW       (WS2811_PIXEL_NS_BIT_TOTAL - WS2811_PIXEL_NS_BIT_0_HIGH)
    #define WS2811_PIXEL_NS_BIT_1_HIGH      WS2811_PIXEL_NS_BIT_0_LOW
    #define WS2811_PIXEL_NS_BIT_1_LOW       WS2811_PIXEL_NS_BIT_0_HIGH
#else
    #define WS2811_PIXEL_NS_BIT_0_HIGH      312.0 // 220ns - 380ns per datasheet
    #define WS2811_PIXEL_NS_BIT_0_LOW       (WS2811_PIXEL_NS_BIT_TOTAL - WS2811_PIXEL_NS_BIT_0_HIGH)
    #define WS2811_PIXEL_NS_BIT_1_HIGH      975.0 // 580ns - 1.6us per datasheet
    #define WS2811_PIXEL_NS_BIT_1_LOW       (WS2811_PIXEL_NS_BIT_TOTAL - WS2811_PIXEL_NS_BIT_1_HIGH)
#endif // ARDUINO_ARCH_ESP32

#define WS2811_PIXEL_IDLE_TIME_US           350.0 // 350us per datasheet
#define WS2811_PIXEL_IDLE_TIME_NS           WS2811_PIXEL_IDLE_TIME_US * NanoSecondsInAMicroSecond

#define WS2811_PIXEL_BITS_PER_INTENSITY     8

}; // c_OutputWS2811

#endif // defined(SUPPORT_OutputType_WS2811)
