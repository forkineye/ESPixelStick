/*
* OutputUart.cpp - TM1814 driver code for ESPixelStick UART Channel
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
#include "../ESPixelStick.h"
#ifdef SUPPORT_UART_OUTPUT

#include "OutputUart.hpp"
extern "C"
{
#if defined(ARDUINO_ARCH_ESP8266)
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#   include <uart.h>

#elif defined(ARDUINO_ARCH_ESP32)

#   include <driver/uart.h>
#   include <driver/gpio.h>

#   include <soc/soc.h>
#   include <soc/uart_reg.h>
#   include <esp32-hal-uart.h>

// Define ESP8266 style macro conversions to limit changes in the rest of the code.
#   define UART_CONF0          UART_CONF0_REG
#   define UART_CONF1          UART_CONF1_REG
#   define UART_INT_ENA        UART_INT_ENA_REG
#   define UART_INT_CLR        UART_INT_CLR_REG
#   define UART_INT_ST         UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE   UART_FIFO_LEN
#endif
} // extern C

#ifndef UART_INV_MASK
#   define UART_INV_MASK (0x3f << 19)
#endif // ndef UART_INV_MASK

// forward declaration for the isr handler
static void IRAM_ATTR uart_intr_handler (void* param);

#ifdef ARDUINO_ARCH_ESP8266
const PROGMEM uint32_t UartDataSizeXlat[8] =
{
    SERIAL_5N1,
    SERIAL_5N2,
    SERIAL_6N1,
    SERIAL_6N2,
    SERIAL_7N1,
    SERIAL_7N2,
    SERIAL_8N1,
    SERIAL_8N2,
};
#elif defined(ARDUINO_ARCH_ESP32)
struct UartDataSizeXlatEntry_t
{
    uart_word_length_t DataWidth;
    uart_stop_bits_t   NumStopBits;
};
const PROGMEM UartDataSizeXlatEntry_t UartDataSizeXlat[8] =
{
    {uart_word_length_t::UART_DATA_5_BITS, uart_stop_bits_t::UART_STOP_BITS_1},
    {uart_word_length_t::UART_DATA_5_BITS, uart_stop_bits_t::UART_STOP_BITS_2},
    {uart_word_length_t::UART_DATA_6_BITS, uart_stop_bits_t::UART_STOP_BITS_1},
    {uart_word_length_t::UART_DATA_6_BITS, uart_stop_bits_t::UART_STOP_BITS_2},
    {uart_word_length_t::UART_DATA_7_BITS, uart_stop_bits_t::UART_STOP_BITS_1},
    {uart_word_length_t::UART_DATA_7_BITS, uart_stop_bits_t::UART_STOP_BITS_2},
    {uart_word_length_t::UART_DATA_8_BITS, uart_stop_bits_t::UART_STOP_BITS_1},
    {uart_word_length_t::UART_DATA_8_BITS, uart_stop_bits_t::UART_STOP_BITS_2},
};
#endif // def ARDUINO_ARCH_ESP32

#define UART_TXD_IDX(u) ((u == 0) ? U0TXD_OUT_IDX : ((u == 1) ? U1TXD_OUT_IDX : ((u == 2) ? U2TXD_OUT_IDX : 0)))

//----------------------------------------------------------------------------
c_OutputUart::c_OutputUart()
{
    // DEBUG_START;

    memset((void *)&Intensity2Uart[0],   0x00, sizeof(Intensity2Uart));

    // DEBUG_END;
} // c_OutputUart

//----------------------------------------------------------------------------
c_OutputUart::~c_OutputUart ()
{
    // DEBUG_START;

    TerminateUartOperation();

    // DEBUG_END;
} // ~c_OutputUart

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    if (param)
    {
        reinterpret_cast<c_OutputUart *>(param)->ISR_Handler();
    }

} // uart_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputUart::Begin (OutputUartConfig_t & config )
{
    // DEBUG_START;

    do // once
    {
        // save the initial config
        OutputUartConfig = config;
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
        if ((nullptr == OutputUartConfig.pPixelDataSource) && (nullptr == OutputUartConfig.pSerialDataSource))
#else
        if (nullptr == OutputUartConfig.pPixelDataSource)
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

        {
            LOG_PORT.println (F("Invalid UART configuration parameters. Rebooting"));
            reboot = true;
            break;
        }

        // initial data width
        SetIntensityDataWidth(OutputUartConfig.IntensityDataWidth);

        // DEBUG_V (String ("      OutputUartConfig.DataPin: ") + String (OutputUartConfig.DataPin));
        // DEBUG_V (String ("OutputUartConfig.UartChannelId: ") + String (OutputUartConfig.ChannelId));

        LastFrameStartTime = millis();

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputUart::StartBreak()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK(UART_CONF0(OutputUartConfig.UartId), UART_TXD_BRK);
#else
    pinMatrixOutDetach(OutputUartConfig.DataPin, false, false);
    pinMode(OutputUartConfig.DataPin, OUTPUT);
    digitalWrite(OutputUartConfig.DataPin, LOW);
#endif // def ARDUINO_ARCH_ESP8266

    // DEBUG_END;

} // StartBreak

//----------------------------------------------------------------------------
void c_OutputUart::EndBreak()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    CLEAR_PERI_REG_MASK(UART_CONF0(OutputUartConfig.UartId), UART_TXD_BRK);
#else
    digitalWrite(OutputUartConfig.DataPin, HIGH);
    pinMatrixOutAttach(OutputUartConfig.DataPin, UART_TXD_IDX(OutputUartConfig.UartId), false, false);
#endif // def ARDUINO_ARCH_ESP8266

    // DEBUG_END;

} // EndBreak

//----------------------------------------------------------------------------
void c_OutputUart::GenerateBreak(uint32_t DurationInUs, uint32_t MarkDurationInUs)
{
    // DEBUG_START;

    StartBreak();
    delayMicroseconds(DurationInUs);
    EndBreak();
    delayMicroseconds(MarkDurationInUs);

    // DEBUG_END;

} // GenerateBreak

//----------------------------------------------------------------------------
void c_OutputUart::GetConfig(JsonObject &jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    jsonConfig[CN_data_pin] = uint8_t(OutputUartConfig.DataPin);
    jsonConfig[CN_baudrate] = OutputUartConfig.Baudrate;

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputUart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

#ifdef USE_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus.createNestedObject("UART Debug");
    debugStatus["ChannelId"]                     = OutputUartConfig.ChannelId;
    debugStatus["RxIsr"]                         = RxIsr;
    debugStatus["DataISRcounter"]                = DataISRcounter;
    debugStatus["IsrIsNotForUs"]                 = IsrIsNotForUs;
    debugStatus["ErrorIsr"]                      = ErrorIsr;
    debugStatus["FrameStartCounter"]             = FrameStartCounter;
    debugStatus["FrameEndISRcounter"]            = FrameEndISRcounter;
    debugStatus["FrameThresholdCounter"]         = FrameThresholdCounter;
    debugStatus["IncompleteFrame"]               = IncompleteFrame;
    debugStatus["IntensityBytesSent"]            = IntensityBytesSent;
    debugStatus["IntensityBytesSentLastFrame"]   = IntensityBytesSentLastFrame;
    debugStatus["IntensityBitsSent"]             = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"]    = IntensityBitsSentLastFrame;
    debugStatus["NumUartSlotsPerIntensityValue"] = OutputUartConfig.NumUartSlotsPerIntensityValue;
    debugStatus["EnqueueCounter"]                = EnqueueCounter;

#endif // def USE_UART_DEBUG_COUNTERS
    // DEBUG_END;
} // GetStatus

#ifdef ARDUINO_ARCH_ESP8266
//----------------------------------------------------------------------------
void c_OutputUart::InitializeUart()
{
    // DEBUG_START;

    do // once
    {
        switch (OutputUartConfig.UartId)
        {
            case UART_NUM_0:
            {
                Serial.begin( OutputUartConfig.Baudrate, 
                              SerialConfig(UartDataSizeXlat[OutputUartConfig.UartDataSize]), 
                              SerialMode(SERIAL_TX_ONLY));
                break;
            }
            case UART_NUM_1:
            {
                Serial1.begin( OutputUartConfig.Baudrate, 
                               SerialConfig (UartDataSizeXlat[OutputUartConfig.UartDataSize]), 
                               SerialMode (SERIAL_TX_ONLY));
                break;
            }

            default:
            {
                logcon(String(F(" Initializing UART on Chan: '")) + String(OutputUartConfig.ChannelId) + "'. ERROR: Invalid UART Id");
                break;
            }

        } // end switch (OutputUartConfig.UartId)

        if (OutputUartConfig.InvertOutputPolarity)
        {
            // invert the output
            CLEAR_PERI_REG_MASK (UART_CONF0(UartId), UART_INV_MASK);
            SET_PERI_REG_MASK (UART_CONF0(UartId), (BIT(22)));
        }

        // Clear FIFOs
        SET_PERI_REG_MASK   (UART_CONF0(OutputUartConfig.UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);
        CLEAR_PERI_REG_MASK (UART_CONF0(OutputUartConfig.UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);

        if (uint32_t(OM_CMN_NO_CUSTOM_ISR) != OutputUartConfig.FiFoTriggerLevel)
        {
            // Disable all interrupts
            ETS_UART_INTR_DISABLE();

            // Set TX FIFO trigger. 40 bytes gives 100 us to start to refill the FIFO
            WRITE_PERI_REG(UART_CONF1(OutputUartConfig.UartId), OutputUartConfig.FiFoTriggerLevel << UART_TXFIFO_EMPTY_THRHD_S);

            // Disable RX & TX interrupts. It is enabled by uart.c in the SDK
            CLEAR_PERI_REG_MASK(UART_INT_ENA(OutputUartConfig.UartId), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

            // Clear all pending interrupts in the UART
            WRITE_PERI_REG(UART_INT_CLR(OutputUartConfig.UartId), UART_INTR_MASK);

            // Reenable interrupts
            ETS_UART_INTR_ENABLE();
        }

        set_pin();

    } while (false);

} // init

#elif defined(ARDUINO_ARCH_ESP32)
//----------------------------------------------------------------------------
void c_OutputUart::InitializeUart()
{
    // DEBUG_START;

    // In the ESP32 you need to be careful which CPU is being configured
    // to handle interrupts. These API functions are supposed to handle this
    // selection.

    // DEBUG_V (String ("OutputUartConfig.UartId  = '") + OutputUartConfig.UartId + "'");
    // DEBUG_V (String ("OutputUartConfig.DataPin = '") + OutputUartConfig.DataPin + "'");

    ESP_ERROR_CHECK(uart_set_hw_flow_ctrl(OutputUartConfig.UartId, uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE, 0));
    ESP_ERROR_CHECK(uart_set_sw_flow_ctrl(OutputUartConfig.UartId, false, 0, 0));

    if (OM_CMN_NO_CUSTOM_ISR != OutputUartConfig.FiFoTriggerLevel)
    {
        // make sure no existing low level ISR is running
        ESP_ERROR_CHECK(uart_disable_tx_intr(OutputUartConfig.UartId));
        // DEBUG_V ("");

        ESP_ERROR_CHECK(uart_disable_rx_intr(OutputUartConfig.UartId));
        // DEBUG_V ("");
    }

    // start the generic UART driver.
    // NOTE: Zero for RX buffer size causes errors in the uart API.
    // Must be at least one byte larger than the fifo size
    // Do not set ESP_INTR_FLAG_IRAM here. the driver's ISR handler is not located in IRAM
    // DEBUG_V ("");
    // ESP_ERROR_CHECK (uart_driver_install (OutputUartConfig.UartId, UART_FIFO_LEN+1, 0, 0, NULL, 0));
    // DEBUG_V (String ("   UartId: ") + String (OutputUartConfig.UartId));
    // DEBUG_V (String (" Baudrate: ") + String (OutputUartConfig.Baudrate));
    // DEBUG_V (String ("ChannelId: ") + String (OutputUartConfig.ChannelId));
    // DEBUG_V (String ("  DataPin: ") + String (OutputUartConfig.DataPin));

    uart_config_t uart_config;
    memset((void *)&uart_config, 0x00, sizeof(uart_config));
    uart_config.baud_rate           = OutputUartConfig.Baudrate;
    uart_config.data_bits           = UartDataSizeXlat[OutputUartConfig.UartDataSize].DataWidth;
    uart_config.stop_bits           = UartDataSizeXlat[OutputUartConfig.UartDataSize].NumStopBits;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.source_clk          = uart_sclk_t::UART_SCLK_APB;

    // DEBUG_V (String ("UART_CLKDIV_REG (OutputUartConfig.UartId): 0x") + String (*((uint32_t*)UART_CLKDIV_REG (OutputUartConfig.UartId)), HEX));
    ESP_ERROR_CHECK(uart_param_config(OutputUartConfig.UartId, &uart_config));
    // DEBUG_V (String ("UART_CLKDIV_REG (OutputUartConfig.UartId): 0x") + String (*((uint32_t*)UART_CLKDIV_REG (OutputUartConfig.UartId)), HEX));

    if (OutputUartConfig.InvertOutputPolarity)
    {
        // invert the output
        CLEAR_PERI_REG_MASK (UART_CONF0(OutputUartConfig.UartId), UART_INV_MASK);
        SET_PERI_REG_MASK   (UART_CONF0(OutputUartConfig.UartId), (BIT(22)));
    }

// #define SupportSetUartBaudrateWorkAround
#ifdef SupportSetUartBaudrateWorkAround
    uint32_t ClockDivider = (APB_CLK_FREQ << 4) / uart_config.baud_rate;
    // DEBUG_V (String ("            APB_CLK_FREQ: 0x") + String (APB_CLK_FREQ, HEX));
    // DEBUG_V (String ("            ClockDivider: 0x") + String (ClockDivider, HEX));
    uint32_t adjusted_ClockDivider = ((ClockDivider >> 4) & UART_CLKDIV_V) | ((ClockDivider & UART_CLKDIV_FRAG_V) << UART_CLKDIV_FRAG_S);
    // DEBUG_V (String ("   adjusted_ClockDivider: 0x") + String (adjusted_ClockDivider, HEX));
    *((uint32_t *)UART_CLKDIV_REG(OutputUartConfig.UartId)) = adjusted_ClockDivider;
    // DEBUG_V (String ("UART_CLKDIV_REG (OutputUartConfig.UartId): 0x") + String (*((uint32_t*)UART_CLKDIV_REG (OutputUartConfig.UartId)), HEX));
#endif // def SupportSetUartBaudrateWorkAround

    set_pin();
    // DEBUG_V ("");

    if (uint32_t(OM_CMN_NO_CUSTOM_ISR) != OutputUartConfig.FiFoTriggerLevel)
    {
        ESP_ERROR_CHECK(uart_disable_tx_intr(OutputUartConfig.UartId));
        // DEBUG_V ("");

        // Disable ALL interrupts. They are enabled by uart.c in the SDK
        // CLEAR_PERI_REG_MASK(UART_INT_ENA(OutputUartConfig.UartId), UART_INTR_MASK);
        // DEBUG_V ("");

        // Clear all pending interrupts in the UART
        // WRITE_PERI_REG (UART_INT_CLR (OutputUartConfig.UartId), UART_INTR_MASK);
    }

    // DEBUG_END;

} // InitializeUart
#endif

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputUart::ISR_Handler ()
{
    do // once
    {
#ifdef USE_UART_DEBUG_COUNTERS
        RxIsr++;
#endif // def USE_UART_DEBUG_COUNTERS

       // Process if the desired UART has raised an interrupt
        if (READ_PERI_REG(UART_INT_ST(OutputUartConfig.UartId)))
        {
#ifdef USE_UART_DEBUG_COUNTERS
            DataISRcounter++;
#endif // def USE_UART_DEBUG_COUNTERS
            // Fill the FIFO with new data
            ISR_Handler_SendIntensityData();

            if (!MoreDataToSend())
            {
                DisableUartInterrupts;
#ifdef USE_UART_DEBUG_COUNTERS
                FrameEndISRcounter++;
#endif // def USE_UART_DEBUG_COUNTERS
            }

            // Clear all interrupt flags for this uart
            WRITE_PERI_REG(UART_INT_CLR(OutputUartConfig.UartId), UART_INTR_MASK);
        } // end Our uart generated an interrupt
#ifdef USE_UART_DEBUG_COUNTERS
        else
        {
            IsrIsNotForUs++;
        }
#endif // def USE_UART_DEBUG_COUNTERS

    } while(false);

} // ISR_Handler

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputUart::ISR_Handler_SendIntensityData ()
{
    size_t NumAvailableUartSlotsToFill = ((((size_t)UART_TX_FIFO_SIZE) - (getUartFifoLength)) / OutputUartConfig.IntensityDataWidth);
    while (MoreDataToSend() && (NumAvailableUartSlotsToFill > OutputUartConfig.NumUartSlotsPerIntensityValue))
    {
        uint32_t IntensityValue = GetNextIntensityToSend();
#ifdef USE_UART_DEBUG_COUNTERS
        IntensityBytesSent++;
#endif // def USE_UART_DEBUG_COUNTERS
        if (OutputUartConfig.TranslateIntensityData)
        {
#ifdef USE_UART_DEBUG_COUNTERS
            IntensityBitsSent += 4;
#endif // def USE_UART_DEBUG_COUNTERS
            enqueueUartData(Intensity2Uart[(IntensityValue >> 6) & 0x0003]); --NumAvailableUartSlotsToFill;
            enqueueUartData(Intensity2Uart[(IntensityValue >> 4) & 0x0003]); --NumAvailableUartSlotsToFill;
            enqueueUartData(Intensity2Uart[(IntensityValue >> 2) & 0x0003]); --NumAvailableUartSlotsToFill;
            enqueueUartData(Intensity2Uart[(IntensityValue >> 0) & 0x0003]); --NumAvailableUartSlotsToFill;
        }
        else
        {
#ifdef USE_UART_DEBUG_COUNTERS
            IntensityBitsSent += 1;
#endif // def USE_UART_DEBUG_COUNTERS
            --NumAvailableUartSlotsToFill;
            enqueueUartData(IntensityValue); 
        }
    } // end while there is space in the buffer

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
void c_OutputUart::PauseOutput(bool PauseOutput)
{
    // DEBUG_START;

    if (OutputIsPaused == PauseOutput)
    {
        // DEBUG_V("no change. Ignore the call");
    }
    else if (PauseOutput)
    {
        // DEBUG_V("stop the output");
        DisableUartInterrupts;
    }

    OutputIsPaused = PauseOutput;

    // DEBUG_END;
} // PauseOutput

//-----------------------------------------------------------------------------
bool c_OutputUart::RegisterUartIsrHandler()
{
    // DEBUG_START;

    bool ret = true;
    DisableUartInterrupts;

#ifdef ARDUINO_ARCH_ESP8266
    // ETS_UART_INTR_DETACH(uart_intr_handler);
    ETS_UART_INTR_ATTACH(uart_intr_handler, this);
#else
    // UART_ENTER_CRITICAL(&(uart_context[uart_num].spinlock));
    if (IsrHandle)
    {
        esp_intr_free(IsrHandle);
        IsrHandle = nullptr;
    }
    ret = (ESP_OK == esp_intr_alloc((OutputUartConfig.UartId == UART_NUM_1) ? ETS_UART1_INTR_SOURCE : ETS_UART2_INTR_SOURCE,
                                    UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM,
                                    uart_intr_handler,
                                    this,
                                    &IsrHandle));
    // UART_EXIT_CRITICAL(&(uart_context[uart_num].spinlock));
#endif
    // DEBUG_END;

    return ret;
} // RegisterUartIsrHandler

//----------------------------------------------------------------------------
bool c_OutputUart::SetConfig(JsonObject &jsonConfig)
{
    // DEBUG_START;

    uint8_t tempDataPin = uint8_t(OutputUartConfig.DataPin);

    bool response = false;
    response |= setFromJSON(tempDataPin, jsonConfig, CN_data_pin);
    response |= setFromJSON(OutputUartConfig.Baudrate, jsonConfig, CN_baudrate);

    OutputUartConfig.DataPin = gpio_num_t(tempDataPin);

    StartUart();

    // DEBUG_END;

    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputUart::SetIntensity2Uart(uint8_t value, UartDataBitTranslationId_t ID)
{
    Intensity2Uart[ID] = value;
} // SetIntensity2Uart

//----------------------------------------------------------------------------
void c_OutputUart::SetIntensityDataWidth(uint32_t DataWidth, size_t NumUartSlotsPerIntensityValue)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V(String("                     DataWidth: ") + String(DataWidth));

        if ((0 == DataWidth) || (DataWidth >= (sizeof(TxIntensityDataStartingMask)-1) * 8))
        {
            // DEBUG_V(String(F("Invalid DataWidth: ")) + String(DataWidth));
            break;
        }
       
        noInterrupts();
        IntensityMapDstMax = (1 << DataWidth) - 1;
        TxIntensityDataStartingMask = 1 << (DataWidth-1);
        OutputUartConfig.NumUartSlotsPerIntensityValue = NumUartSlotsPerIntensityValue;
        interrupts();

        // DEBUG_V(String("NumUartSlotsPerIntensityValue: ")   + String(NumUartSlotsPerIntensityValue));
        // DEBUG_V(String("  TxIntensityDataStartingMask: 0x") + String(TxIntensityDataStartingMask, HEX));

    } while (false);

    // DEBUG_END;

} // SetIntensityDataWidth

//----------------------------------------------------------------------------
void c_OutputUart::set_pin()
{
    pinMode(OutputUartConfig.DataPin, OUTPUT);
    digitalWrite(OutputUartConfig.DataPin, LOW);

#ifdef ARDUINO_ARCH_ESP8266
    // UART Pins cannot be modified on the ESP8266
#else
    ESP_ERROR_CHECK(uart_set_pin(OutputUartConfig.UartId,
                                 OutputUartConfig.DataPin,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    pinMatrixOutAttach(OutputUartConfig.DataPin, UART_TXD_IDX(OutputUartConfig.UartId), false, false);

#endif // def ARDUINO_ARCH_ESP32
} // set_pin

//----------------------------------------------------------------------------
void c_OutputUart::SetMinFrameDurationInUs(uint32_t value)
{
    FrameMinDurationInMicroSec = value;
} // SetMinFrameDurationInUs

//----------------------------------------------------------------------------
void c_OutputUart::SetSendBreak(bool value)
{
    SendBreak = value;
} // SetSendBreak

//----------------------------------------------------------------------------
void c_OutputUart::StartNewFrame()
{
    // DEBUG_START;
#ifdef USE_UART_DEBUG_COUNTERS
    FrameStartCounter++;
#endif // def USE_UART_DEBUG_COUNTERS

#ifdef USE_UART_DEBUG_COUNTERS
    if(MoreDataToSend())
    {
        IncompleteFrame++;
    }

    IntensityBytesSentLastFrame = IntensityBytesSent;
    IntensityBytesSent = 0;
    IntensityBitsSentLastFrame = IntensityBitsSent;
    IntensityBitsSent = 0;
#endif // def USE_UART_DEBUG_COUNTERS

    // set up to send a new frame
    if(SendBreak)
    {
        GenerateBreak(92, 23);
    }
    // DEBUG_V();
    StartNewDataFrame();
    // DEBUG_V();

    ISR_Handler_SendIntensityData();

    EnableUartInterrupts;

    // DEBUG_END;

} // StartNewFrame

//----------------------------------------------------------------------------
void c_OutputUart::StartUart()
{
    // DEBUG_START;

    do // once
    {
        // are we using a valid config?
        if (gpio_num_t(-1) == OutputUartConfig.DataPin)
        {
            logcon(F("ERROR: Data pin has not been defined"));
            return;
        }

        TerminateUartOperation();

        // Set output pins
        set_pin();

        /* Initialize uart */
        InitializeUart();

        // Atttach interrupt handler
        RegisterUartIsrHandler();

        enqueueUartData(0xff);

    } while (false);

    // DEBUG_END;
} // StartUart

//----------------------------------------------------------------------------
void c_OutputUart::TerminateUartOperation()
{
    // DEBUG_START;
    
    DisableUartInterrupts;

#ifdef SUPPORT_UART_OUTPUT
    if (OutputUartConfig.ChannelId <= c_OutputMgr::e_OutputChannelIds::OutputChannelId_UART_LAST)
    {
        switch (OutputUartConfig.UartId)
        {
            case UART_NUM_0:
            {
                // DEBUG_V ("UART_NUM_0");
                Serial.end();
                break;
            }

            case UART_NUM_1:
            {
                // DEBUG_V ("UART_NUM_1");
                Serial1.end();
                break;
            }

#ifdef ARDUINO_ARCH_ESP32
            case UART_NUM_2:
            {
                // DEBUG_V ("UART_NUM_2");
                Serial2.end();
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
#endif // def SUPPORT_UART_OUTPUT

#ifdef ARDUINO_ARCH_ESP32
    if (IsrHandle)
    {
        esp_intr_free(IsrHandle);
        IsrHandle = nullptr;
    }
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;

} // TerminateUartOperation

#endif // def SUPPORT_UART_OUTPUT
