#pragma once
/*
* OutputAPA102.h - APA102 driver code for ESPixelStick
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
#if defined(SUPPORT_OutputType_APA102)

#include "OutputPixel.hpp"

class c_OutputAPA102 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputAPA102 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                    gpio_num_t outputGpio,
                    uart_port_t uart,
                    c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputAPA102 ();

    // functions to be provided by the derived class
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = "APA102"; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define APA102_BIT_RATE                 (APB_CLK_FREQ/80)
#define APA102_BITS_PER_INTENSITY       8
#define APA102_MICRO_SEC_PER_INTENSITY  int ( ( (1.0/float (APA102_BIT_RATE)) * APA102_BITS_PER_INTENSITY))
#define APA102_MIN_IDLE_TIME_US         500
    uint16_t       BlockSize = 1;
    float          BlockDelay = 0;
    const uint32_t FrameStartData = 0;
    const uint32_t FrameEndData = 0xFFFFFFFF;
    const uint8_t  PixelStartData = 0xFF;     // Max driving current

}; // c_OutputAPA102

#endif // defined(SUPPORT_OutputType_APA102) && defined(SUPPORT_SPI_OUTPUT)
