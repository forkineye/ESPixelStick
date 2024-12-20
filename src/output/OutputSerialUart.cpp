/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 - 2025 Shelby Merrick
*
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
******************************************************************/

#include "ESPixelStick.h"
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

#include "output/OutputSerialUart.hpp"

//----------------------------------------------------------------------------
c_OutputSerialUart::c_OutputSerialUart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                        gpio_num_t outputGpio,
                                        uart_port_t uart,
                                        c_OutputMgr::e_OutputType outputType) :
    c_OutputSerial(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    // DEBUG_END;
} // c_OutputSerialUart

//----------------------------------------------------------------------------
c_OutputSerialUart::~c_OutputSerialUart ()
{
    // DEBUG_START;
    // DEBUG_END;
} // ~c_OutputSerialUart

//----------------------------------------------------------------------------
void c_OutputSerialUart::Begin ()
{
    // DEBUG_START;

    c_OutputSerial::Begin();

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
#if defined(SUPPORT_OutputType_DMX)
    if (c_OutputMgr::e_OutputType::OutputType_DMX == OutputType)
    {
        OutputUartConfig.FrameStartBreakUS          = 92;
        OutputUartConfig.FrameStartMarkAfterBreakUS = 23;
    }
#endif // defined(SUPPORT_OutputType_DMX)
    OutputUartConfig.ChannelId = OutputChannelId;
    OutputUartConfig.UartId                 = UartId;
    OutputUartConfig.DataPin                = DataPin;
    OutputUartConfig.IntensityDataWidth     = 8;
    OutputUartConfig.UartDataSize           = c_OutputUart::UartDatauint32_t::OUTPUT_UART_8N2;
    OutputUartConfig.TranslateIntensityData = c_OutputUart::TranslateIntensityData_t::NoTranslation;
    OutputUartConfig.pSerialDataSource      = this;
    OutputUartConfig.Baudrate               = CurrentBaudrate;
    Uart.Begin(OutputUartConfig);

    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
bool c_OutputSerialUart::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputSerial::SetConfig(jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSerialUart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputSerial::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputSerialUart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputSerial::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputSerialUart::Poll ()
{
    // DEBUG_START;
    uint32_t FrameLen = ActualFrameDurationMicroSec;

    if (canRefresh())
    {
        Uart.StartNewFrame();
        ReportNewFrame();
    }
    else
    {
        FrameLen = 0;
    }

    // DEBUG_END;
    return FrameLen;

} // render

//----------------------------------------------------------------------------
void c_OutputSerialUart::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputSerial::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
