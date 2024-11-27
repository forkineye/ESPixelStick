/*
* OutputGECE.cpp - GECE driver code for ESPixelStick
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

#include "output/OutputGECE.hpp"

//----------------------------------------------------------------------------
c_OutputGECE::c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                            gpio_num_t outputGpio,
                            uart_port_t uart,
                            c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;


    // DEBUG_END;

} // c_OutputGECE

//----------------------------------------------------------------------------
c_OutputGECE::~c_OutputGECE ()
{
    // DEBUG_START;


    // DEBUG_END;
} // ~c_OutputGECE

//----------------------------------------------------------------------------
void c_OutputGECE::Begin ()
{
    // DEBUG_START;

    SetPixelCount(GECE_PIXEL_LIMIT);
    SetOutputBufferSize(GECE_PIXEL_LIMIT * GECE_NUM_INTENSITY_BYTES_PER_PIXEL);
    c_OutputPixel::Begin();

    // DEBUG_V (String ("GECE_BAUDRATE: ") + String (GECE_BAUDRATE));


    HasBeenInitialized = true;

    // DEBUG_END;
} // begin

//----------------------------------------------------------------------------
bool c_OutputGECE::SetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::SetConfig(jsonConfig);
#ifdef foo
    uint temp;
    temp = map(brightness, 0, 255, 0, 100);
    setFromJSON(temp,  jsonConfig, CN_brightness);
    brightness = map (temp, 0, 100, 0, 255);
#endif // def foo

    // DEBUG_END;

    return validate ();

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGECE::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    validate();
    // jsonConfig[CN_brightness] = map(brightness, 0, 255, 0, 100);

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputGECE::GetStatus (ArduinoJson::JsonObject & jsonStatus)
{
    c_OutputPixel::GetStatus(jsonStatus);
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputGECE::SetOutputBufferSize(uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    // Stop current output operation
    c_OutputPixel::SetOutputBufferSize(NumChannelsAvailable);

    // Calculate our refresh time
    SetFrameDurration(float(GECE_FRAME_TIME_USEC) / (GECE_PACKET_SIZE-1));

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
bool c_OutputGECE::validate ()
{
    // DEBUG_START;

    bool response = true;
    uint32_t PixelCount = GetPixelCount();
    if (PixelCount > GECE_PIXEL_LIMIT)
    {
        PixelCount = GECE_PIXEL_LIMIT;
        response = false;
    }
    else if (PixelCount < 1)
    {
        PixelCount = GECE_PIXEL_LIMIT;
        response = false;
    }

    // DEBUG_V (String ("pixel_count: ") + String (pixel_count));
    SetOutputBufferSize(PixelCount * GECE_NUM_INTENSITY_BYTES_PER_PIXEL);
    SetPixelCount(PixelCount);
#ifdef foo

    OutputFrame.CurrentPixelID = 0;
    OutputFrame.pCurrentInputData = pOutputBuffer;
#endif // def foo

    // DEBUG_END;

    return response;

} // validate

//----------------------------------------------------------------------------
uint32_t c_OutputGECE::Poll()
{
    // DEBUG_START;


    // DEBUG_END;
    return 0;
} // render

#endif // defined(SUPPORT_OutputType_GECE)
