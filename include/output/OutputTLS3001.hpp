#pragma once
/*
* OutputTLS3001.h - TLS3001 driver code for ESPixelStick
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
#ifdef SUPPORT_OutputType_TLS3001

#include "OutputPixel.hpp"

class c_OutputTLS3001 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTLS3001 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
        gpio_num_t outputGpio,
        uart_port_t uart,
        c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputTLS3001 ();

    // functions to be provided by the derived class
    virtual void Begin () { c_OutputPixel::Begin (); }
    virtual bool SetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String& sDriverName) { sDriverName = CN_TLS3001; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define TLS3001_PIXEL_DATA_RATE              500000.0
#define TLS3001_PIXEL_NS_BIT                ((1.0 / TLS3001_PIXEL_DATA_RATE) * NanoSecondsInASecond)

#define TLS3001_PIXEL_NS_IDLE                50000.0 // 50us
#define TLS3001_MIN_IDLE_TIME_US             (TLS3001_PIXEL_NS_IDLE / float(NanoSecondsInAMicroSecond))

#define TLS3001_DEFAULT_INTENSITY_PER_PIXEL  3
/*
    Frame Start = 15 1s Always followed by four bit Frame Type

    Frame Types
        Sync   0b0001 Followed by 15 0s followed by an idle period
        Reset  0b0100 Followed by long idle time (calculation needed)
        Data   0b0010 Followed by 39 bit data times num pixels

    39 bit data
        1 zero
        12 R
        1 zero
        12 g
        1 zero
        12 b
*/
#define TLS3001_MAX_CONSECUTIVE_DATA_FRAMES 50

private:

    uint8_t CurrentLimit = 50;
    struct PreambleData_t
    {
        uint8_t normal[4];
        uint8_t inverted[4];
    };
    PreambleData_t PreambleData;

}; // c_OutputTLS3001

#endif // def SUPPORT_OutputType_TLS3001
