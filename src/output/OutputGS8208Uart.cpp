/*
* GS8208Uart.cpp - GS8208 driver code for ESPixelStick UART
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

#include "output/OutputGS8208Uart.hpp"

 /*
  * Inverted 6N1 UART lookup table for GS8208, MSB 2 bits ignored.
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
c_OutputGS8208Uart::c_OutputGS8208Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputGS8208 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputGS8208Uart

//----------------------------------------------------------------------------
c_OutputGS8208Uart::~c_OutputGS8208Uart ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGS8208Uart

//----------------------------------------------------------------------------
void c_OutputGS8208Uart::Begin ()
{
    // DEBUG_START;
    c_OutputGS8208::Begin();

    // DEBUG_V(String("GS8208_PIXEL_UART_BAUDRATE: ") + String(GS8208_PIXEL_UART_BAUDRATE));

    SetIntensityBitTimeInUS(float(GS8208_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId                     = OutputChannelId;
    OutputUartConfig.UartId                         = UartId;
    OutputUartConfig.DataPin                        = DataPin;
    OutputUartConfig.IntensityDataWidth             = GS8208_PIXEL_BITS_PER_INTENSITY;
    OutputUartConfig.UartDataSize                   = c_OutputUart::UartDatauint32_t::OUTPUT_UART_6N1;
    OutputUartConfig.TranslateIntensityData         = c_OutputUart::TranslateIntensityData_t::TwoToOne;
    OutputUartConfig.pPixelDataSource               = this;
    OutputUartConfig.Baudrate                       = GS8208_NUM_DATA_BYTES_PER_INTENSITY_BYTE * GS8208_PIXEL_DATA_RATE;
    OutputUartConfig.InvertOutputPolarity           = true;
    OutputUartConfig.CitudsArray                    = ConvertIntensityToUartDataStream;
    Uart.Begin(OutputUartConfig);

#ifdef testPixelInsert
    static const uint32_t FrameStartData = 0;
    static const uint32_t FrameEndData = 0xFFFFFFFF;
    static const uint8_t  PixelStartData = 0xC0;

    SetFramePrependInformation ( (uint8_t*)&FrameStartData, sizeof (FrameStartData));
    SetFrameAppendInformation ( (uint8_t*)&FrameEndData, sizeof (FrameEndData));
    SetPixelPrependInformation (&PixelStartData, sizeof (PixelStartData));
#endif // def testPixelInsert

    HasBeenInitialized = true;

} // init

//----------------------------------------------------------------------------
bool c_OutputGS8208Uart::SetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGS8208::SetConfig(jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGS8208Uart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputGS8208::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputGS8208Uart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputGS8208::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef GS8208_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["GS8208 UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"] = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"] = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def GS8208_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputGS8208Uart::Poll()
{
    // DEBUG_START;
    uint32_t FrameLen = ActualFrameDurationMicroSec;

    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount)

    do // Once
    {
        if (gpio_num_t(-1) == DataPin)
        {
            FrameLen = 0;
            break;
        }

        if (!canRefresh())
        {
            FrameLen = 0;
            break;
        }

        // DEBUG_V("get the next frame started");
#ifdef GS8208_UART_DEBUG_COUNTERS
        NewFrameCounter++;
        TimeSinceLastFrameMS = millis() - TimeLastFrameStartedMS;
        TimeLastFrameStartedMS = millis();
#endif // def GS8208_UART_DEBUG_COUNTERS

        Uart.StartNewFrame();
        ReportNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return FrameLen;

} // render

//----------------------------------------------------------------------------
void c_OutputGS8208Uart::PauseOutput(bool State)
{
    // DEBUG_START;

    c_OutputGS8208::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_GS8208)
