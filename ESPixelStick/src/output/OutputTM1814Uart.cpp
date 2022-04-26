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

#include "../ESPixelStick.h"
#if defined(SUPPORT_OutputType_TM1814) && defined(SUPPORT_UART_OUTPUT)

#include "OutputTM1814Uart.hpp"


/*
* 8N2 UART lookup table for TM1814
* Start and stop bits are part of the pixel stream.
*/
struct Convert2BitIntensityToTM1814UartDataStreamEntry_t
{
    uint8_t Translation;
    c_OutputUart::UartDataBitTranslationId_t Id;
};
static const Convert2BitIntensityToTM1814UartDataStreamEntry_t PROGMEM Convert2BitIntensityToTM1814UartDataStream[] =
{
    {0b11111100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID},
    {0b11000000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID},
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

    for (auto CurrentTranslation : Convert2BitIntensityToTM1814UartDataStream)
    {
        Uart.SetIntensity2Uart(CurrentTranslation.Translation, CurrentTranslation.Id);
    }
    
    // DEBUG_V(String("TM1814_PIXEL_UART_BAUDRATE: ") + String(TM1814_PIXEL_UART_BAUDRATE));

    SetIntensityBitTimeInUS(float(TM1814_PIXEL_NS_BIT_TOTAL) / 1000.0);

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.ChannelId                     = OutputChannelId;
    OutputUartConfig.UartId                        = UartId;
    OutputUartConfig.DataPin                       = DataPin;
    OutputUartConfig.IntensityDataWidth            = TM1814_NUM_DATA_BYTES_PER_INTENSITY_BYTE;
    OutputUartConfig.UartDataSize                  = c_OutputUart::UartDataSize_t::OUTPUT_UART_8N2;
    OutputUartConfig.TranslateIntensityData        = c_OutputUart::TranslateIntensityData_t::OneToOne;
    OutputUartConfig.pPixelDataSource              = this;
    OutputUartConfig.Baudrate                      = TM1814_BAUD_RATE;
    OutputUartConfig.InvertOutputPolarity          = true;
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
    JsonObject debugStatus = jsonStatus.createNestedObject("TM1814 UART Debug");
    debugStatus["NewFrameCounter"] = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"] = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def TM1814_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::Render ()
{
    // DEBUG_START;

    do // Once
    {
        if (gpio_num_t(-1) == DataPin)
        {
            break;
        }

        if (!canRefresh())
        {
            break;
        }

        // DEBUG_V("get the next frame started");
        Uart.StartNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;

} // render

//----------------------------------------------------------------------------
void c_OutputTM1814Uart::PauseOutput(bool State)
{
    // DEBUG_START;

    c_OutputTM1814::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_TM1814) && defined(SUPPORT_UART_OUTPUT)
