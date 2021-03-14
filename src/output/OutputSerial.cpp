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

#include "OutputSerial.hpp"
#include "OutputCommon.hpp"

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

#define pinMatrixOutDetach
#define pinMatrixOutAttach
#define UART_TXD_IDX

#define UART_STATUS_REG UART_STATUS

#elif defined(ARDUINO_ARCH_ESP32)

#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define SERIAL_TX_ONLY       UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN

#define UART_TXD_IDX(u)     ((u==0)?U0TXD_OUT_IDX:((u==1)?U1TXD_OUT_IDX:((u==2)?U2TXD_OUT_IDX:0)))

#endif

#define FIFO_TRIGGER_LEVEL (UART_TX_FIFO_SIZE / 2)

typedef enum
{
	CMD_DATA_START   = 0x80,
    ESC_CHAR         = 0x7F,
    FRAME_START_CHAR = 0x7E,
    FRAME_PAD_CHAR   = 0x7D,
    ESCAPED_OFFSET   = 0x4E,

    MIN_VAL_TO_ESC   = FRAME_PAD_CHAR,
    MAX_VAL_TO_ESC   = ESC_CHAR
} RenardFrameDefinitions_t;

//----------------------------------------------------------------------------
c_OutputSerial::c_OutputSerial (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio,
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    // DEBUG_END;
} // c_OutputSerial

//----------------------------------------------------------------------------
c_OutputSerial::~c_OutputSerial ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

#ifdef ARDUINO_ARCH_ESP32
    // make sure no existing low level driver is running
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");

    ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
    // DEBUG_V ("");
#endif

    // DEBUG_END;
} // ~c_OutputSerial

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputSerial*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
void c_OutputSerial::Begin ()
{
    // DEBUG_START;

    SetOutputBufferSize (Num_Channels);

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputSerial::StartUart ()
{
    // DEBUG_START;
    int speed = 0;

    if (OutputType == c_OutputMgr::e_OutputType::OutputType_DMX)
    {
        speed = uint (BaudRate::BR_DMX);
        speed = uint (BaudRate::BR_DEF);
    }
    else
    {
        speed = uint (CurrentBaudrate);
    }

#ifdef ARDUINO_ARCH_ESP8266
    /* Initialize uart */
    InitializeUart (speed,
        SERIAL_8N1,
        SERIAL_TX_ONLY,
        FIFO_TRIGGER_LEVEL);
#else
    uart_config_t uart_config;
    uart_config.baud_rate           = speed;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_8_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;
    InitializeUart (uart_config, uint32_t (FIFO_TRIGGER_LEVEL));
#endif

    // make sure we are ready to send a new frame
    RemainingDataCount = 0;

    // Atttach interrupt handler
#ifdef ARDUINO_ARCH_ESP8266
    ETS_UART_INTR_ATTACH (uart_intr_handler, this);
#else
    uart_isr_register (UartId, uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr);
#endif

    // prime the uart status bits
    enqueue (0);

    // DEBUG_END;
} // StartUart

//----------------------------------------------------------------------------
/*
*   Validate that the current values meet our needs
*
*   needs
*       data set in the class elements
*   returns
*       true - no issues found
*       false - had an issue and had to fix things
*/
bool c_OutputSerial::validate ()
{
    // DEBUG_START;
    bool response = true;

    if ((Num_Channels > MAX_CHANNELS) || (Num_Channels < 1))
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Setting to ")) + MAX_CHANNELS + F (" ***"));
        Num_Channels = DEFAULT_NUM_CHANNELS;
        response = false;
    }
    SetOutputBufferSize (Num_Channels);

    if ((CurrentBaudrate < uint (BaudRate::BR_MIN)) || (CurrentBaudrate > uint (BaudRate::BR_MAX)))
    {
        LOG_PORT.println (String (F ("*** Requested BaudRate is Not Valid. Setting to Default ***")));
        CurrentBaudrate = uint (BaudRate::BR_DEF);
        response = false;
    }

    if (GenericSerialHeader.length() > MAX_HDR_SIZE)
    {
        LOG_PORT.println (String (F ("*** Requested Generic Serial Header is too long. Setting to Default ***")));
        GenericSerialHeader = "";
    }

    if (GenericSerialFooter.length() > MAX_FOOTER_SIZE)
    {
        LOG_PORT.println (String (F ("*** Requested Generic Serial Footer is too long. Setting to Default ***")));
        GenericSerialFooter = "";
    }

    pGenericSerialFooter = (char*)GenericSerialFooter.c_str ();
    LengthGenericSerialFooter = GenericSerialFooter.length ();

    // DEBUG_END;
    return response;

} // validate

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputSerial::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;
    uint temp; // Holds enums prior to conversion
    setFromJSON (GenericSerialHeader, jsonConfig, CN_gen_ser_hdr);
    setFromJSON (GenericSerialFooter, jsonConfig, CN_gen_ser_ftr);
    setFromJSON (Num_Channels,        jsonConfig, CN_num_chan);
    setFromJSON (CurrentBaudrate,     jsonConfig, CN_baudrate);

    temp = uint (DataPin);
    setFromJSON (temp, jsonConfig, CN_data_pin);
    DataPin = gpio_num_t (temp);

    bool response = validate ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    StartUart ();

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSerial::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;
    jsonConfig[CN_num_chan]    = Num_Channels;
    jsonConfig[CN_baudrate]    = CurrentBaudrate;
    jsonConfig[CN_data_pin]    = uint (DataPin);
    if (OutputType == c_OutputMgr::e_OutputType::OutputType_Serial)
    {
        jsonConfig[CN_gen_ser_hdr] = GenericSerialHeader;
        jsonConfig[CN_gen_ser_ftr] = GenericSerialFooter;
    }
    // enums need to be converted to uints for json
    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void  c_OutputSerial::GetDriverName (String & sDriverName)
{
    switch (OutputType)
    {
        case c_OutputMgr::e_OutputType::OutputType_Serial:
        {
            sDriverName = F ("Serial");
            break;
        }

        case c_OutputMgr::e_OutputType::OutputType_DMX:
        {
            sDriverName = F ("DMX");
            break;
        }

        case c_OutputMgr::e_OutputType::OutputType_Renard:
        {
            sDriverName = F ("Renard");
            break;
        }

        default:
        {
            sDriverName = F ("Default");
            break;
        }
    } // switch (OutputType)

} // GetDriverName

//----------------------------------------------------------------------------
// Fill the FIFO with as many intensity values as it can hold.
void IRAM_ATTR c_OutputSerial::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        do // once
        {
            // is there anything to send?
            if (0 == RemainingDataCount)
            {
                // at this point the FIFO has at least 40 free bytes. 10 byte header should fit
                // are we in generic serial mode?
                if (OutputType == c_OutputMgr::e_OutputType::OutputType_Serial)
                {
                    for (size_t HeaderIndex = 0; HeaderIndex < LengthGenericSerialFooter; HeaderIndex++)
                    {
                        enqueue (pGenericSerialFooter[HeaderIndex]);
                    }
                } // need to send the footer

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
            while ((3 < SpaceInFifo) && (0 < RemainingDataCount))
            {
                SpaceInFifo--;
                RemainingDataCount--;

                // read the current data value from the buffer and point at the next byte
                data = *pNextChannelToSend++;

                // do we have to adjust the renard data stream?
                if ((OutputType == c_OutputMgr::e_OutputType::OutputType_Renard) &&
                    (data >= RenardFrameDefinitions_t::MIN_VAL_TO_ESC) &&
                    (data <= RenardFrameDefinitions_t::MAX_VAL_TO_ESC))
                {
                    // Send a two byte substitute for the value
                    enqueue (RenardFrameDefinitions_t::ESC_CHAR);
                    data -= uint8_t(RenardFrameDefinitions_t::ESCAPED_OFFSET);

                    // show that we had to add an extra data byte to the output fifo
                    --SpaceInFifo;

                } // end modified data

                // send the intensity data
                enqueue (data);

            } // end send one or more channel value

        } while (false);
    } // end this channel has an interrupt

} // ISR_Handler

//----------------------------------------------------------------------------
void c_OutputSerial::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputCommon::GetStatus (jsonStatus);
    uint32_t conf0       = READ_PERI_REG (UART_CONF0 (UartId));
    uint32_t conf1       = READ_PERI_REG (UART_CONF1 (UartId));
    uint32_t intena      = READ_PERI_REG (UART_INT_ENA (UartId));
    uint32_t intRaw      = READ_PERI_REG (UART_INT_RAW_REG (UartId));
    uint32_t UartIntSt   = READ_PERI_REG (UART_INT_ST (UartId));
    uint32_t UartStatus  = READ_PERI_REG (UART_STATUS_REG (UartId));
    uint16_t CharsInFifo = getFifoLength;

    jsonStatus["pNextChannelToSend"] = String(uint32_t (pNextChannelToSend), HEX);
    jsonStatus["RemainingDataCount"] = uint32_t (RemainingDataCount);
    jsonStatus["UartIntSt"]          = String (UartIntSt, HEX);
    jsonStatus["CharsInFifo"]        = CharsInFifo;
    jsonStatus["conf0"]              = String(uint32_t (conf0), HEX);
    jsonStatus["conf1"]              = String(uint32_t (conf1), HEX);
    jsonStatus["intena"]             = String(uint32_t (intena), HEX);
    jsonStatus["UartIntRaw"]         = String (intRaw, HEX);
    jsonStatus["UartStatus"]         = String (UartStatus, HEX);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputSerial::Render ()
{
    // DEBUG_START;

    // has the ISR stopped running?
    uint32_t UartMask = GET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
    if (UartMask)
    { 
        return;
    }

//    delayMicroseconds (1000000);
//    LOG_PORT.println ("4");

    // start the next frame
    switch (OutputType)
    {
        case c_OutputMgr::e_OutputType::OutputType_DMX:
        {
            if (UART_TX_DONE_INT_RAW != GET_PERI_REG_MASK (UART_INT_RAW_REG (UartId), UART_TX_DONE_INT_RAW))
            {
                return;
            } // go away if not

            delayMicroseconds (DMX_MAB);

#ifdef ARDUINO_ARCH_ESP8266
            SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
            delayMicroseconds (DMX_BREAK);
            CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
            delayMicroseconds (DMX_MAB);
#else
            pinMatrixOutDetach (DataPin, false, false); //Detach our
            pinMode (DataPin, OUTPUT);
            digitalWrite (DataPin, LOW); //88 uS break
            delayMicroseconds (DMX_BREAK);
            digitalWrite (DataPin, HIGH); //4 Us Mark After Break
            delayMicroseconds (DMX_MAB);
            pinMatrixOutAttach (DataPin, UART_TXD_IDX (UartId), false, false);
#endif // def ARDUINO_ARCHITECTURE_ESP8266

            // send the rest of the frame
            break;
        } // DMX512

        case c_OutputMgr::e_OutputType::OutputType_Renard:
        {
            enqueue (RenardFrameDefinitions_t::FRAME_START_CHAR);
            enqueue (RenardFrameDefinitions_t::CMD_DATA_START);

            break;
        } // RENARD

        case c_OutputMgr::e_OutputType::OutputType_Serial:
        {
            // LOG_PORT.println ("5 '" + GenericSerialHeader + "'");

            // load the generic header into the fifo
            for (auto currentByte : GenericSerialHeader)
            {
                enqueue (currentByte);
            }
            // ISR will send the footer
            break;
        } // GENERIC

        default:
        { break; } // this is not possible but the language needs it here

    } // end switch (OutputType)

    // point at the input data buffer
    pNextChannelToSend = pOutputBuffer;
    RemainingDataCount = OutputBufferSize;

    // enable interrupts and start sending
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

    ReportNewFrame ();

    // DEBUG_END;
} // render
