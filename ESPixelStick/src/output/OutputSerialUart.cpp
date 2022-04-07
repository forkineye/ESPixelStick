/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*       Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*       See Readme for other info and version history
*
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

#include "../ESPixelStick.h"
#include <utility>
#include <algorithm>
#include <math.h>

#include "OutputSerialUart.hpp"
#include "OutputCommon.hpp"

#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

#ifdef ARDUINO_ARCH_ESP8266
extern "C" {
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
}
#define GET_PERI_REG_MASK(reg, mask) (READ_PERI_REG((reg)) & (mask))
#define UART_INT_RAW_REG             UART_INT_RAW
#define UART_TX_DONE_INT_RAW         UART_TXFIFO_EMPTY_INT_RAW

#define UART_STATUS_REG UART_STATUS

#ifndef UART_TXD_INV
#   define UART_TXD_INV BIT(22)
#endif // ndef UART_TXD_INV

#define UART_INT_CLR_REG UART_INT_CLR

#define UART_TX_DONE_INT_CLR BIT(1)

#elif defined(ARDUINO_ARCH_ESP32)
#   include <soc/uart_reg.h>

#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define SERIAL_TX_ONLY       UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN

#endif

#define FIFO_TRIGGER_LEVEL (UART_TX_FIFO_SIZE / 2)

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
    if (HasBeenInitialized)
    {
#ifdef ARDUINO_ARCH_ESP32
        // make sure no existing low level driver is running
        ESP_ERROR_CHECK(uart_disable_tx_intr(UartId));
        // DEBUG_V ("");

        ESP_ERROR_CHECK(uart_disable_rx_intr(UartId));
        // DEBUG_V ("");
#endif
    }

    // DEBUG_END;
} // ~c_OutputSerialUart

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputSerialUart*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
void c_OutputSerialUart::Begin ()
{
    // DEBUG_START;
    c_OutputSerial::Begin();
    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputSerialUart::StartUart ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    /* Initialize uart */
    InitializeUart(CurrentBaudrate,
                   SERIAL_8N2,
                   SERIAL_TX_ONLY,
                   FIFO_TRIGGER_LEVEL);
#else
    uart_config_t uart_config;
    memset ((void*)&uart_config, 0x00, sizeof (uart_config));
    uart_config.baud_rate = CurrentBaudrate;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_8_BITS;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_2;
    InitializeUart (uart_config, uint32_t (FIFO_TRIGGER_LEVEL));
#endif

    // Atttach interrupt handler
    RegisterUartIsrHandler(uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM);

    enqueue (0xff);

    // DEBUG_END;
} // StartUart

//----------------------------------------------------------------------------
bool c_OutputSerialUart::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputSerial::SetConfig(jsonConfig);
    // DEBUG_V(String("   CurrentBaudrate: ") + String(CurrentBaudrate));
    float BitTime = (1.0 / float(CurrentBaudrate));
    // DEBUG_V(String("         BitTimeUS: ") + String(BitTime * 1000000.0));
    c_OutputSerial::SetFrameDurration(BitTime);

#ifdef ARDUINO_ARCH_ESP32
    ESP_ERROR_CHECK (uart_set_pin (UartId, DataPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#endif

    StartUart ();

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
// Fill the FIFO with as many intensity values as it can hold.
void IRAM_ATTR c_OutputSerialUart::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        do // once
        {
            // is there anything to send?
            if (!ISR_MoreDataToSend())
            {
                // Disable ALL interrupts when done
                CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);

                // Clear all interrupts flags for this uart
                WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);
                break;
            } // end close of frame

            // Fill the FIFO with new data
            uint16_t SpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength));

            // only read from ram once per data byte
            uint8_t data = 0;

            // cant precalc this since data sent count is data value dependent (for Renard)
            // is there an intensity value to send and do we have the space to send it?
            while (SpaceInFifo && ISR_MoreDataToSend())
            {
                SpaceInFifo--;

                // read the current data value from the buffer and point at the next byte
                data = ISR_GetNextIntensityToSend();

                // send the intensity data
                enqueue (data);

            } // end send one or more channel value

        } while (false);
    } // end this channel has an interrupt
} // ISR_Handler

//----------------------------------------------------------------------------
void c_OutputSerialUart::Render ()
{
    // DEBUG_START;

    // has the ISR stopped running?
    uint32_t UartMask = GET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
    if (UartMask)
    {
        return;
    }

    // delayMicroseconds (1000000);
    // DEBUG_V ("4");

    // start the next frame
    switch (OutputType)
    {
#ifdef SUPPORT_OutputType_DMX
        case c_OutputMgr::e_OutputType::OutputType_DMX:
        {
            if (!canRefresh())
            {
                return;
            }

            c_OutputSerial::StartNewFrame();

            GenerateBreak (DMX_BREAK_US);
            delayMicroseconds (DMX_MAB_US);

            break;
        } // DMX512
#endif // def SUPPORT_OutputType_DMX

#ifdef SUPPORT_OutputType_Renard
        case c_OutputMgr::e_OutputType::OutputType_Renard:
        {
            c_OutputSerial::StartNewFrame();

            break;
        } // RENARD
#endif // def SUPPORT_OutputType_Renard

#ifdef SUPPORT_OutputType_Serial
        case c_OutputMgr::e_OutputType::OutputType_Serial:
        {
            c_OutputSerial::StartNewFrame();
            break;
        } // GENERIC
#endif // def SUPPORT_OutputType_Serial

        default:
        { break; } // this is not possible but the language needs it here

    } // end switch (OutputType)

    // enable interrupts and start sending
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

    ReportNewFrame ();

    // DEBUG_END;

} // render

#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
