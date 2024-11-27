/*
* UCS8903Uart.cpp - UCS8903 driver code for ESPixelStick UART
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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

#if defined(SUPPORT_OutputType_UCS8903)

#include "output/OutputUCS8903Uart.hpp"

/*
 * Inverted 8N1 UART lookup table for UCS8903.
 * Start and stop bits are part of the pixel stream.
 */
static const c_OutputUart::ConvertIntensityToUartDataStreamEntry_t ConvertIntensityToUartDataStream[] =
{
    { 0b11111100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID}, // (0)00111111(1)
    { 0b11000000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID}, // (0)00000011(1)
    { 0,          c_OutputUart::UartDataBitTranslationId_t::Uart_LIST_END}
};

#define UCS8903_NUM_UART_BITS_PER_INTENSITY_BIT 10
#define UCS8903_NUM_UART_DATA_BYTES_PER_INTENSITY_VALUE 16
#define UCS8903_PIXEL_UART_BAUDRATE uint32_t(UCS8903_NUM_UART_BITS_PER_INTENSITY_BIT * UCS8903_PIXEL_DATA_RATE)

//----------------------------------------------------------------------------
c_OutputUCS8903Uart::c_OutputUCS8903Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputUCS8903 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputUCS8903Uart

//----------------------------------------------------------------------------
c_OutputUCS8903Uart::~c_OutputUCS8903Uart ()
{
    // DEBUG_START;
    // DEBUG_END;
} // ~c_OutputUCS8903Uart

//----------------------------------------------------------------------------
void c_OutputUCS8903Uart::Begin ()
{
    // DEBUG_START;

    // DEBUG_V(String("UCS8903_PIXEL_NS_BIT_TOTAL: ") + String(UCS8903_PIXEL_NS_BIT_TOTAL));
    // DEBUG_V(String("   UCS8903_PIXEL_DATA_RATE: ") + String(UCS8903_PIXEL_DATA_RATE));

    c_OutputUCS8903::Begin();

    SetIntensityBitTimeInUS(float(UCS8903_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId              = OutputChannelId;
    OutputUartConfig.UartId                 = UartId;
    OutputUartConfig.DataPin                = DataPin;
    OutputUartConfig.IntensityDataWidth     = UCS8903_INTENSITY_DATA_WIDTH;
    OutputUartConfig.UartDataSize           = c_OutputUart::UartDatauint32_t::OUTPUT_UART_8N1;
    OutputUartConfig.TranslateIntensityData = c_OutputUart::TranslateIntensityData_t::OneToOne;
    OutputUartConfig.pPixelDataSource       = this;
    OutputUartConfig.Baudrate               = UCS8903_PIXEL_UART_BAUDRATE;
    OutputUartConfig.InvertOutputPolarity   = true;
    OutputUartConfig.CitudsArray            = ConvertIntensityToUartDataStream;
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
    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
bool c_OutputUCS8903Uart::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response  = c_OutputUCS8903::SetConfig (jsonConfig);
    // DEBUG_V();
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputUCS8903Uart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputUCS8903::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputUCS8903Uart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputUCS8903::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef UCS8903_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["UCS8903 UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"]        = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"]   = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def UCS8903_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputUCS8903Uart::Poll ()
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
        // StartNewFrame();
        // DEBUG_V();
#ifdef UCS8903_UART_DEBUG_COUNTERS
        NewFrameCounter++;
        TimeSinceLastFrameMS = millis() - TimeLastFrameStartedMS;
        TimeLastFrameStartedMS = millis();
#endif // def UCS8903_UART_DEBUG_COUNTERS

        Uart.StartNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return FrameLen;

} // render

//----------------------------------------------------------------------------
void c_OutputUCS8903Uart::PauseOutput (bool State)
{
    // DEBUG_START;

    // c_OutputUCS8903::PauseOutput(State);
    // Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_UCS8903)
