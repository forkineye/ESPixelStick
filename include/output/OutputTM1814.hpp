#pragma once
/*
* OutputTM1814.h - TM1814 driver code for ESPixelStick
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
#ifdef SUPPORT_OutputType_TM1814

#include "OutputPixel.hpp"

class c_OutputTM1814 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTM1814 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputTM1814 ();

    // functions to be provided by the derived class
    virtual void Begin ();
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = CN_TM1814; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define TM1814_PIXEL_DATA_RATE              800000.0
#define TM1814_PIXEL_NS_BIT_TOTAL           ((1.0 / TM1814_PIXEL_DATA_RATE) * NanoSecondsInASecond)

#define TM1814_PIXEL_NS_BIT_0_LOW           375.0 // 360ns +/- 50ns per datasheet
#define TM1814_PIXEL_NS_BIT_0_HIGH          (TM1814_PIXEL_NS_BIT_TOTAL - TM1814_PIXEL_NS_BIT_0_LOW)

#define TM1814_PIXEL_NS_BIT_1_LOW           800.0 // 720ns -70ns / +280ns per datasheet
#define TM1814_PIXEL_NS_BIT_1_HIGH          (TM1814_PIXEL_NS_BIT_TOTAL - TM1814_PIXEL_NS_BIT_1_LOW)

#define TM1814_PIXEL_NS_IDLE                500000.0 // 500us per datasheet
#define TM1814_MIN_IDLE_TIME_US             (TM1814_PIXEL_NS_IDLE / float(NanoSecondsInAMicroSecond))

#define TM1814_DEFAULT_INTENSITY_PER_PIXEL  3

private:

    uint8_t CurrentLimit = 50;
    struct PreambleData_t
    {
        uint8_t normal[4];
        uint8_t inverted[4];
    };
    PreambleData_t PreambleData;

}; // c_OutputTM1814

#endif // def SUPPORT_OutputType_TM1814
