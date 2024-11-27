/*
* UCS1903Uart.cpp - UCS1903 driver code for ESPixelStick UART
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
#if defined(SUPPORT_OutputType_UCS1903)

#include "output/OutputUCS1903Uart.hpp"

/*
 * Inverted 6N1 UART lookup table for UCS1903, MSB 2 bits ignored.
 * Start and stop bits are part of the pixel stream.
 */
static const c_OutputUart::ConvertIntensityToUartDataStreamEntry_t ConvertIntensityToUartDataStream[] =
{
    {0b00110111, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID}, // 00 - (1)000 100(0)
    {0b00000111, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID}, // 01 - (1)000 111(0)
    {0b00110100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_10_ID}, // 10 - (1)110 100(0)
    {0b00000100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_11_ID}, // 11 - (1)110 111(0)
    {0,          c_OutputUart::UartDataBitTranslationId_t::Uart_LIST_END}
};

//----------------------------------------------------------------------------
c_OutputUCS1903Uart::c_OutputUCS1903Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputUCS1903 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputUCS1903Uart

//----------------------------------------------------------------------------
c_OutputUCS1903Uart::~c_OutputUCS1903Uart ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputUCS1903Uart

//----------------------------------------------------------------------------
void c_OutputUCS1903Uart::Begin ()
{
    // DEBUG_START;

    c_OutputUCS1903::Begin();

    SetIntensityBitTimeInUS(float(UCS1903_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId              = OutputChannelId;
    OutputUartConfig.UartId                 = UartId;
    OutputUartConfig.DataPin                = DataPin;
    OutputUartConfig.IntensityDataWidth     = UCS1903_PIXEL_BITS_PER_INTENSITY;
    OutputUartConfig.UartDataSize           = c_OutputUart::UartDatauint32_t::OUTPUT_UART_6N1;
    OutputUartConfig.TranslateIntensityData = c_OutputUart::TranslateIntensityData_t::TwoToOne;
    OutputUartConfig.pPixelDataSource       = this;
    OutputUartConfig.Baudrate               = int(UCS1903_NUM_DATA_BYTES_PER_INTENSITY_BYTE * UCS1903_PIXEL_DATA_RATE);;
    OutputUartConfig.InvertOutputPolarity   = true;
    OutputUartConfig.CitudsArray            = ConvertIntensityToUartDataStream;
    Uart.Begin(OutputUartConfig);

    HasBeenInitialized = true;

} // init

//----------------------------------------------------------------------------
bool c_OutputUCS1903Uart::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputUCS1903::SetConfig (jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputUCS1903Uart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputUCS1903::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputUCS1903Uart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputUCS1903::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef UCS1903_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["UCS1903 UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"] = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"] = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def UCS1903_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputUCS1903Uart::Poll ()
{
    // DEBUG_START;
    uint32_t FrameLen = ActualFrameDurationMicroSec;

    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount)

    if (gpio_num_t (-1) == DataPin) { return 0; }
    if (!canRefresh ()) { return 0; }

    // get the next frame started
    Uart.StartNewFrame ();
    ReportNewFrame ();

    // DEBUG_END;
    return FrameLen;

} // render

//----------------------------------------------------------------------------
void c_OutputUCS1903Uart::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputUCS1903::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_UCS1903)
