#pragma once
/*
* OutputGECE.h - GECE driver code for ESPixelStick
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
*/

#include "ESPixelStick.h"
#if defined(SUPPORT_OutputType_GECE)

#include "OutputGECEFrame.hpp"
#include "OutputPixel.hpp"

class c_OutputGECE: public c_OutputPixel
{
public:
    c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                  gpio_num_t outputGpio,
                  uart_port_t uart,
                  c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGECE ();

    // functions to be provided by the derived class
    virtual void Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    virtual bool SetConfig(ArduinoJson::JsonObject &jsonConfig);        ///< Set a new config in the driver
    virtual void GetConfig(ArduinoJson::JsonObject &jsonConfig);        ///< Get the current config used by the driver
    virtual void GetStatus(ArduinoJson::JsonObject &jsonStatus);        ///< Get the current config used by the driver
    virtual uint32_t Poll();                                              ///< Call from loop(),  renders output data
    virtual void GetDriverName(String &sDriverName) { sDriverName = CN_GECE; }
            void SetOutputBufferSize(uint32_t NumChannelsAvailable);
            bool validate ();

private:
/*
    output looks like this

    Start bit = High for 8us
    26 data bits.
        Each bit is 31us
        0 = 6 us low, 25 us high
        1 = 25 us low, 6 us high
    stop bit = low for at least 45us
*/
#define GECE_PIXEL_LIMIT        63  ///< Total pixel limit
#define GECE_DEFAULT_BRIGHTNESS 0xCC

#define GECE_PIXEL_NS_BIT_0_HIGH            (25 * NanoSecondsInAMicroSecond)
#define GECE_PIXEL_NS_BIT_0_LOW             (6  * NanoSecondsInAMicroSecond)
#define GECE_PIXEL_NS_BIT_1_HIGH            (6  * NanoSecondsInAMicroSecond)
#define GECE_PIXEL_NS_BIT_1_LOW             (25 * NanoSecondsInAMicroSecond)
#define GECE_PIXEL_START_TIME_NS            (8  * NanoSecondsInAMicroSecond)
#define GECE_PIXEL_STOP_TIME_NS             (45 * NanoSecondsInAMicroSecond)
#define GECE_USEC_PER_GECE_BIT              ((GECE_PIXEL_NS_BIT_0_HIGH + GECE_PIXEL_NS_BIT_0_LOW)/NanoSecondsInAMicroSecond)

#define GECE_NUM_INTENSITY_BYTES_PER_PIXEL  3
#define GECE_BITS_PER_INTENSITY             4
#define GECE_BITS_BRIGHTNESS                8
#define GECE_BITS_ADDRESS                   6
#define GECE_OVERHEAD_BITS                  (GECE_BITS_BRIGHTNESS + GECE_BITS_ADDRESS)
#define GECE_PACKET_SIZE                    ((GECE_NUM_INTENSITY_BYTES_PER_PIXEL * GECE_BITS_PER_INTENSITY) + GECE_OVERHEAD_BITS) //   26

#define GECE_FRAME_TIME_USEC                ((GECE_PACKET_SIZE * GECE_USEC_PER_GECE_BIT) + 90)
#define GECE_FRAME_TIME_NSEC                (GECE_FRAME_TIME_USEC * NanoSecondsInAMicroSecond)

};


#endif // defined(SUPPORT_OutputType_GECE)
