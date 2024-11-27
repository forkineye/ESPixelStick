/*
* TM1814Uart.cpp - TM1814 driver code for ESPixelStick UART
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
#if defined(SUPPORT_OutputType_TM1814)

#include "output/OutputTM1814Uart.hpp"


/*
* 8N2 UART lookup table for TM1814
* Start and stop bits are part of the pixel stream.
*/
static const c_OutputUart::ConvertIntensityToUartDataStreamEntry_t ConvertIntensityToUartDataStream[] =
{
    {0b11111100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID}, // (0)00111111(1)
    {0b11000000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID}, // (0)00000011(1)
    {0,          c_OutputUart::UartDataBitTranslationId_t::Uart_LIST_END}
};

//----------------------------------------------------------------------------
c_OutputTM1814Uart::c_OutputTM1814Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputTM1814 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputTM1814Uart

//----------------------------------------------------------------------------
c_OutputTM1814Uart::~c_OutputTM1814Uart ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTM1814Uart

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::Begin ()
{
    // DEBUG_START;

    c_OutputTM1814::Begin();

    // DEBUG_V(String("TM1814_PIXEL_UART_BAUDRATE: ") + String(TM1814_PIXEL_UART_BAUDRATE));

    SetIntensityBitTimeInUS(float(TM1814_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId              = OutputChannelId;
    OutputUartConfig.UartId                 = UartId;
    OutputUartConfig.DataPin                = DataPin;
    OutputUartConfig.IntensityDataWidth     = TM1814_NUM_DATA_BYTES_PER_INTENSITY_BYTE;
    OutputUartConfig.UartDataSize           = c_OutputUart::UartDatauint32_t::OUTPUT_UART_8N2;
    OutputUartConfig.TranslateIntensityData = c_OutputUart::TranslateIntensityData_t::OneToOne;
    OutputUartConfig.pPixelDataSource       = this;
    OutputUartConfig.Baudrate               = TM1814_BAUD_RATE;
    OutputUartConfig.InvertOutputPolarity   = true;
    OutputUartConfig.CitudsArray            = ConvertIntensityToUartDataStream;
    Uart.Begin(OutputUartConfig);

    HasBeenInitialized = true;

} // init

//----------------------------------------------------------------------------
bool c_OutputTM1814Uart::SetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputTM1814::SetConfig(jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputTM1814::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputTM1814::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef TM1814_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["TM1814 UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"] = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"] = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def TM1814_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputTM1814Uart::Poll ()
{
    // DEBUG_START;
    uint32_t FrameLen = ActualFrameDurationMicroSec;

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
        Uart.StartNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return FrameLen;

} // Poll

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::PauseOutput(bool State)
{
    // DEBUG_START;

    c_OutputTM1814::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_TM1814)
