/*
* OutputGS8208.cpp - GS8208 driver code for ESPixelStick UART
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

#if defined(SUPPORT_OutputType_GS8208)

#include "output/OutputGS8208.hpp"

//----------------------------------------------------------------------------
c_OutputGS8208::c_OutputGS8208 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputPixel (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = GS8208_PIXEL_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputGS8208

//----------------------------------------------------------------------------
c_OutputGS8208::~c_OutputGS8208 ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGS8208

//----------------------------------------------------------------------------
void c_OutputGS8208::Begin ()
{
    // DEBUG_START;

    c_OutputPixel::Begin ();

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputGS8208::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputGS8208::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputPixel::GetStatus (jsonStatus);

    // uint32_t UartIntSt = GET_PERI_REG_MASK (UART_INT_ST (UartId), UART_TXFIFO_EMPTY_INT_ENA);
    // uint16_t SpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getGS8208FifoLength));
    // jsonStatus["UartIntSt"] = UartIntSt;
    // jsonStatus["SpaceInFifo"] = SpaceInFifo;

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputGS8208::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

        // Stop current output operation
    c_OutputPixel::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    SetFrameDurration (float(GS8208_PIXEL_NS_BIT_0_HIGH + GS8208_PIXEL_NS_BIT_0_LOW) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputGS8208::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputPixel::SetConfig (jsonConfig);

    // Calculate our refresh time
    SetFrameDurration (float (GS8208_PIXEL_NS_BIT_0_HIGH + GS8208_PIXEL_NS_BIT_0_LOW) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;
    return response;

} // SetConfig

#endif // defined(SUPPORT_OutputType_GS8208)
