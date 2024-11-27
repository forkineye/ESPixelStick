/*
* OutputGECEUart.cpp - GECE driver code for ESPixelStick UART
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

#include "output/OutputGECEUart.hpp"

/*
 * 8N1 UART lookup table for GECE,
 * Start bit and stop bits are part of the packet.
 * Bits are backwards since we need MSB out first.
 */
static const c_OutputUart::ConvertIntensityToUartDataStreamEntry_t ConvertIntensityToUartDataStream[] =
{
    {0b11101111, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID}, // (0)1111 0111(1)
    {0b11101000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID}, // (0)0001 0111(1)
    {0b00001111, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_10_ID}, // (0)1111 1111(1)
    {0b00001000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_11_ID}, // (0)0001 0000(1)
    {0,          c_OutputUart::UartDataBitTranslationId_t::Uart_LIST_END}
};

//----------------------------------------------------------------------------
c_OutputGECEUart::c_OutputGECEUart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                    gpio_num_t outputGpio,
                                    uart_port_t uart,
                                    c_OutputMgr::e_OutputType outputType) :
                                    c_OutputGECE (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputGECEUart

//----------------------------------------------------------------------------
c_OutputGECEUart::~c_OutputGECEUart ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGECEUart

//----------------------------------------------------------------------------
void c_OutputGECEUart::Begin ()
{
    // DEBUG_START;

    c_OutputGECE::Begin();

    // DEBUG_V(String("GECE_PIXEL_UART_BAUDRATE: ") + String(GECE_PIXEL_UART_BAUDRATE));

    SetIntensityBitTimeInUS(float(GECE_USEC_PER_GECE_BIT));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId                      = OutputChannelId;
    OutputUartConfig.UartId                         = UartId;
    OutputUartConfig.DataPin                        = DataPin;
    OutputUartConfig.IntensityDataWidth             = GECE_PACKET_SIZE;
    OutputUartConfig.UartDataSize                   = c_OutputUart::UartDatauint32_t::OUTPUT_UART_8N1;
    OutputUartConfig.TranslateIntensityData         = c_OutputUart::TranslateIntensityData_t::TwoToOne;
    OutputUartConfig.pPixelDataSource               = this;
    OutputUartConfig.Baudrate                       = GECE_BAUDRATE;
    OutputUartConfig.InvertOutputPolarity           = false;
    OutputUartConfig.NumInterIntensityBreakBits     = GECE_UART_BREAK_BITS; // number of bit times to delay
    OutputUartConfig.TriggerIsrExternally           = false;
    OutputUartConfig.NumInterIntensityMABbits       = uint32_t((float(GECE_PIXEL_START_TIME_NS / NanoSecondsInAMicroSecond) / float(GECE_UART_USEC_PER_BIT)) + 0.5);
    OutputUartConfig.CitudsArray                    = ConvertIntensityToUartDataStream;
    Uart.Begin(OutputUartConfig);

    // DEBUG_V (String ("  GECE_FRAME_TIME_USEC: ") + String (GECE_FRAME_TIME_USEC));
    // DEBUG_V (String ("  GECE_FRAME_TIME_NSEC: ") + String (GECE_FRAME_TIME_NSEC));
    // DEBUG_V (String ("GECE_CCOUNT_FRAME_TIME: ") + String (GECE_CCOUNT_FRAME_TIME));

    HasBeenInitialized = true;

    // DEBUG_END;
} // init

//----------------------------------------------------------------------------
bool c_OutputGECEUart::SetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputGECE::SetConfig(jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGECEUart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputGECE::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputGECEUart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputGECE::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef GECE_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["GECE UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"]          = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"]     = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"]   = TimeLastFrameStartedMS;
#endif // def GECE_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputGECEUart::Poll ()
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
#ifdef GECE_UART_DEBUG_COUNTERS
        NewFrameCounter++;
        TimeSinceLastFrameMS = millis() - TimeLastFrameStartedMS;
        TimeLastFrameStartedMS = millis();
#endif // def GECE_UART_DEBUG_COUNTERS

        ReportNewFrame();
        Uart.StartNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return FrameLen;

} // render

//----------------------------------------------------------------------------
void c_OutputGECEUart::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputGECE::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_GECE)
