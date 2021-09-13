/*
* OutputCommon.cpp - Output Interface base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
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
*   This is an Interface base class used to manage the output port. It provides
*   a common API for use by the factory class to manage the object.
*
*/

#include "../ESPixelStick.h"
#include "OutputCommon.hpp"

extern "C" {
#ifdef ARDUINO_ARCH_ESP8266
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#elif defined(ARDUINO_ARCH_ESP32)
#   include <esp32-hal-uart.h>
#   include <soc/soc.h>
#   include <soc/uart_reg.h>
#   include <rom/ets_sys.h>
#   include <driver/uart.h>

#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define SERIAL_TX_ONLY       UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN

#define UART_TXD_IDX(u)     ((u==0)?U0TXD_OUT_IDX:((u==1)?U1TXD_OUT_IDX:((u==2)?U2TXD_OUT_IDX:0)))

#endif
}
static void IRAM_ATTR uart_intr_handler (void* param);

//-------------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputCommon::c_OutputCommon (c_OutputMgr::e_OutputChannelIds iOutputChannelId,
	                            gpio_num_t outputGpio,
	                            uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType)
{
	// remember what channel we are
	HasBeenInitialized       = false;
	OutputChannelId          = iOutputChannelId;
	DataPin                  = outputGpio;
	UartId                   = uart;
    OutputType               = outputType;
    pOutputBuffer            = OutputMgr.GetBufferAddress ();
    FrameStartTimeInMicroSec = 0;

	// logcon (String ("UartId:          '") + UartId + "'");
    // logcon (String ("OutputChannelId: '") + OutputChannelId + "'");
    // logcon (String ("OutputType:      '") + OutputType + "'");
    if (gpio_num_t (-1) != DataPin)
    {
        pinMode (DataPin, INPUT_PULLUP);
    }

} // c_OutputMgr

//-------------------------------------------------------------------------------
// deallocate any resources and put the output channels into a safe state
c_OutputCommon::~c_OutputCommon ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    pinMode (DataPin, INPUT_PULLUP);

    // DEBUG_V ("");
    TerminateUartOperation ();
    // DEBUG_END;
} // ~c_OutputMgr

#ifdef ARDUINO_ARCH_ESP8266
//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputCommon::InitializeUart (uint32_t baudrate,
                                     uint32_t uartFlags,
                                     uint32_t uartFlags2,
                                     uint32_t fifoTriggerLevel)
{
    // DEBUG_START;

    do // once
    {
        // are we using a valid config?
        if (gpio_num_t (-1) == DataPin)
        {
            logcon (F ("ERROR: Data pin has not been defined"));
            break;
        }

        TerminateUartOperation ();

        // Set output pins
        pinMode (DataPin, OUTPUT);
        digitalWrite (DataPin, LOW);

        switch (UartId)
        {
            case UART_NUM_0:
            {
                Serial.begin (baudrate, SerialConfig(uartFlags), SerialMode(uartFlags2));
                break;
            }
            case UART_NUM_1:
            {
                Serial1.begin (baudrate, SerialConfig (uartFlags), SerialMode (uartFlags2));
                break;
            }

            default:
            {
                logcon (String (F (" Initializing UART on Chan: '")) + String (OutputChannelId) + "'. ERROR: Invalid UART Id");
                break;
            }

        } // end switch (UartId)

        // Clear FIFOs
        SET_PERI_REG_MASK   (UART_CONF0 (UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);
        CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);

        if (OM_CMN_NO_CUSTOM_ISR != fifoTriggerLevel)
        {
            // Disable all interrupts
            ETS_UART_INTR_DISABLE ();

            // Set TX FIFO trigger. 40 bytes gives 100 us to start to refill the FIFO
            WRITE_PERI_REG (UART_CONF1 (UartId), fifoTriggerLevel << UART_TXFIFO_EMPTY_THRHD_S);

            // Disable RX & TX interrupts. It is enabled by uart.c in the SDK
            CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

            // Clear all pending interrupts in the UART
            WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

            // Reenable interrupts
            ETS_UART_INTR_ENABLE ();
        }

    } while (false);

} // init

#elif defined (ARDUINO_ARCH_ESP32)
void c_OutputCommon::InitializeUart (uart_config_t & uart_config,
                                     uint32_t fifoTriggerLevel)
{
    // DEBUG_START;

    // are we using a valid config?
    if (gpio_num_t (-1) == DataPin)
    {
        logcon (F ("ERROR: Data pin has not been defined"));
        return;
    }

    TerminateUartOperation ();

    // In the ESP32 you need to be careful which CPU is being configured
    // to handle interrupts. These API functions are supposed to handle this
    // selection.

    // DEBUG_V (String ("UartId  = '") + UartId + "'");
    // DEBUG_V (String ("DataPin = '") + DataPin + "'");

    // Set output pins
    pinMode (DataPin, OUTPUT);
    digitalWrite (DataPin, LOW);

    ESP_ERROR_CHECK (uart_set_hw_flow_ctrl (UartId, uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE, 0));
    ESP_ERROR_CHECK (uart_set_sw_flow_ctrl (UartId, false, 0, 0));

    if (OM_CMN_NO_CUSTOM_ISR != fifoTriggerLevel)
    {
        // make sure no existing low level ISR is running
        ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
        // DEBUG_V ("");

        ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
        // DEBUG_V ("");
    }

    // start the generic UART driver.
    // NOTE: Zero for RX buffer size causes errors in the uart API.
    // Must be at least one byte larger than the fifo size
    // Do not set ESP_INTR_FLAG_IRAM here. the driver's ISR handler is not located in IRAM
    // DEBUG_V ("");
    // ESP_ERROR_CHECK (uart_driver_install (UartId, UART_FIFO_LEN+1, 0, 0, NULL, 0));
    // DEBUG_V (String ("                  UartId: 0x") + String (UartId, HEX));
    // DEBUG_V (String ("   uart_config.baud_rate: 0x") + String (uart_config.baud_rate, HEX));
    // DEBUG_V (String ("UART_CLKDIV_REG (UartId): 0x") + String (UART_CLKDIV_REG (UartId), HEX));

    uart_config.parity = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.flow_ctrl = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.source_clk = uart_sclk_t::UART_SCLK_APB;

    ESP_ERROR_CHECK (uart_param_config (UartId, & uart_config));
#define SupportSetUartBaudrateWorkAround
#ifdef SupportSetUartBaudrateWorkAround
    // DEBUG_V (String ("UART_CLKDIV_REG (UartId): 0x") + String (UART_CLKDIV_REG (UartId), HEX));
    uint32_t ClockDivider = (APB_CLK_FREQ << 4) / uart_config.baud_rate;
    // DEBUG_V (String ("            APB_CLK_FREQ: 0x") + String (APB_CLK_FREQ, HEX));
    // DEBUG_V (String ("            ClockDivider: 0x") + String (ClockDivider, HEX));
    uint32_t adjusted_ClockDivider = ((ClockDivider >> 4) & UART_CLKDIV_V) | ((ClockDivider & UART_CLKDIV_FRAG_V) << UART_CLKDIV_FRAG_S);
    // DEBUG_V (String ("   adjusted_ClockDivider: 0x") + String (adjusted_ClockDivider, HEX));
    *((uint32_t *) UART_CLKDIV_REG (UartId)) = adjusted_ClockDivider;
    // DEBUG_V (String ("UART_CLKDIV_REG (UartId): 0x") + String (UART_CLKDIV_REG (UartId), HEX));
#endif // def SupportSetUartBaudrateWorkAround

    ESP_ERROR_CHECK (uart_set_pin (UartId, DataPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // DEBUG_V ("");

    if (OM_CMN_NO_CUSTOM_ISR != fifoTriggerLevel)
    {
        ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
        // DEBUG_V ("");

        // Disable ALL interrupts. They are enabled by uart.c in the SDK
        CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);
        // DEBUG_V ("");

        // Clear all pending interrupts in the UART
        // WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);
    }

    // DEBUG_END;

} // InitializeUart
#endif

//-----------------------------------------------------------------------------
void c_OutputCommon::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

#define MicroSecondsInAsecond 1000000
    jsonStatus[CN_id] = OutputChannelId;
    jsonStatus["framerefreshrate"] = (0 == FrameRefreshTimeInMicroSec) ? 0 : int (MicroSecondsInAsecond / FrameRefreshTimeInMicroSec);
    jsonStatus["FrameCount"] = FrameCount;

    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputCommon::TerminateUartOperation ()
{
    // DEBUG_START;

    if (OutputChannelId <= c_OutputMgr::OutputChannelId_UART_LAST)
    {
        switch (UartId)
        {
            case UART_NUM_0:
            {
                // DEBUG_V ("UART_NUM_0");
                Serial.end ();
                break;
            }
            case UART_NUM_1:
            {
                // DEBUG_V ("UART_NUM_1");
                Serial1.end ();
                break;
            }

    #ifdef ARDUINO_ARCH_ESP32
            case UART_NUM_2:
            {
                // DEBUG_V ("UART_NUM_2");
                Serial2.end ();
                break;
            }
    #endif // def ARDUINO_ARCH_ESP32

            default:
            {
                // DEBUG_V ("default");
                break;
            }
        } // end switch (UartId)
    }

    // DEBUG_END;

} // TerminateUartOperation

//----------------------------------------------------------------------------
void c_OutputCommon::ReportNewFrame ()
{
    // DEBUG_START;

    uint32_t Now = micros ();
    FrameRefreshTimeInMicroSec = Now - FrameStartTimeInMicroSec;
    FrameStartTimeInMicroSec = Now;

    FrameCount++;

    // DEBUG_END;

} // ReportNewFrame

//----------------------------------------------------------------------------
void c_OutputCommon::StartBreak ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
#else
    pinMatrixOutDetach (DataPin, false, false);
    pinMode (DataPin, OUTPUT);
    digitalWrite (DataPin, LOW);
#endif // def ARDUINO_ARCH_ESP8266

    // DEBUG_END;

} // StartBreak

//----------------------------------------------------------------------------
void c_OutputCommon::EndBreak ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
#else
    digitalWrite (DataPin, HIGH);
    pinMatrixOutAttach (DataPin, UART_TXD_IDX (UartId), false, false);
#endif // def ARDUINO_ARCH_ESP8266

    // DEBUG_END;

} // EndBreak

//----------------------------------------------------------------------------
void c_OutputCommon::GenerateBreak (uint32_t DurationInUs)
{
    // DEBUG_START;

    StartBreak ();
    delayMicroseconds (DurationInUs);
    EndBreak ();

    // DEBUG_END;

} // GenerateBreak

//----------------------------------------------------------------------------
bool c_OutputCommon::SetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    uint8_t tempDataPin = uint8_t (DataPin);

    bool response = setFromJSON (tempDataPin, jsonConfig, CN_data_pin);

    DataPin = gpio_num_t (tempDataPin);

    // DEBUG_END;

    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputCommon::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    jsonConfig[CN_data_pin] = uint8_t (DataPin);

    // DEBUG_END;
} // GetConfig
