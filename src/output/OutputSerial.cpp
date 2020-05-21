/******************************************************************
*
*       Project: ESPixelStick - An ESP8266/ESP32 and E1.31 based pixel (And Serial!) driver
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

#include <Arduino.h>
#include <utility>
#include <algorithm>
#include <math.h>

#include "OutputSerial.hpp"
#include "OutputCommon.hpp"

#include "../ESPixelStick.h"
#include "../FileIO.h"

extern "C" {
#ifdef ARDUINO_ARCH_ESP8266
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#elif defined(ARDUINO_ARCH_ESP32)

#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define SERIAL_TX_ONLY       UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN
#endif
}

#define FIFO_TRIGGER_LEVEL (UART_TX_FIFO_SIZE / 2)

static void ICACHE_RAM_ATTR handleGenericSerial_ISR (void* param);

uint32_t BaudRateToBps[]
{
    38400,
    57600,
    115200,
    230400,
    250000,
    460800,
};


c_OutputSerial::c_OutputSerial (c_OutputMgr::e_OutputChannelIds OutputChannelId, 
                                gpio_num_t outputGpio, uart_port_t uart) : 
    c_OutputCommon(OutputChannelId, outputGpio, uart)
{
    // DEBUG_START;
    // DEBUG_END;
} // c_OutputSerial

c_OutputSerial::~c_OutputSerial ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

#ifdef ARDUINO_ARCH_ESP8266
    Serial1.end ();
#else

    // make sure no existing low level driver is running
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");

    ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
    // DEBUG_V ("");

#endif

    // DEBUG_END;
} // ~c_OutputSerial

void c_OutputSerial::Begin () 
{
    // DEBUG_START;
    LOG_PORT.println (String (F ("** SERIAL Initialization for Chan: ")) + String (OutputChannelId) + " **");
    int speed = 0;

    if (_type == SerialType::DMX512)
    {
        speed = BaudRateToBps[int(BaudRate::BR_250000)];
    }
    else
    {
        speed = BaudRateToBps[int(_baudrate)];
    }

#ifdef ARDUINO_ARCH_ESP8266
    /* Initialize uart */
    /* frameTime = szSymbol * 1000000 / baud * szBuffer */
    InitializeUart (speed,
        SERIAL_8N1,
        SERIAL_TX_ONLY,
        FIFO_TRIGGER_LEVEL);
#else
    /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate           = speed;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_8_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;
    InitializeUart (uart_config, uint32_t(FIFO_TRIGGER_LEVEL));
#endif

//    DEBUG_END;
}

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
    DEBUG_START;
    bool response = true;

    if ((Num_Channels > MAX_CHANNELS) || (Num_Channels < 1))
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Setting to ")) + MAX_CHANNELS + F(" ***"));
        Num_Channels = DEFAULT_NUM_CHANNELS;
        response = false;
    }

    if ((_baudrate < BaudRate::BR_MIN) || (_baudrate > BaudRate::BR_MAX))
    {
        LOG_PORT.println (String (F ("*** Requested BaudRate is Not Valid. Setting to Default ***")));
        _baudrate = BaudRate::BR_DEFAULT;
        response = false;
    }

    if((_type < SerialType::MIN_TYPE) || (_type > SerialType::MAX_TYPE))
    {
        LOG_PORT.println (String (F ("*** Requested Output Type is Not Valid. Setting to Default ***")));
        _type = SerialType::DEFAULT_TYPE;
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
    DEBUG_END;
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
    DEBUG_START;
    uint temp; // Holds enums prior to conversion
    FileIO::setFromJSON (GenericSerialHeader, jsonConfig["gen_ser_hdr"]);
    FileIO::setFromJSON (GenericSerialFooter, jsonConfig["gen_ser_ftr"]);
    FileIO::setFromJSON (Num_Channels,        jsonConfig["num_chan"]);

    // enums need to be converted to uints for json
    temp = uint (_baudrate);
    FileIO::setFromJSON (temp, jsonConfig["baudrate"]);
    _baudrate = BaudRate (temp);

    temp = uint (_type);
    FileIO::setFromJSON (temp, jsonConfig["ser_type"]);
    _type = SerialType (temp);

    temp = uint (DataPin);
    FileIO::setFromJSON (temp, jsonConfig["data_pin"]);
    DataPin = gpio_num_t (temp);

    DEBUG_END;
    return validate ();

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSerial::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    DEBUG_START;
    String DriverName = ""; GetDriverName (DriverName);
    jsonConfig["type"] = DriverName;
    jsonConfig["gen_ser_hdr"] = GenericSerialHeader;
    jsonConfig["gen_ser_ftr"] = GenericSerialFooter;
    jsonConfig["num_chan"] = Num_Channels;
    // enums need to be converted to uints for json
    jsonConfig["baudrate"] = uint (_baudrate);
    jsonConfig["ser_type"] = uint (_type);
    jsonConfig["data_pin"] = uint (DataPin);
    DEBUG_END;
} // GetConfig

// shell function to set the 'this' pointer of the real ISR
// This allows me to use non static variables in the ISR.
static void ICACHE_RAM_ATTR handleGenericSerial_ISR (void* param)
{
    reinterpret_cast <c_OutputSerial*>(param)->ISR_Handler ();
} // handleGenericSerial_ISR

// Fill the FIFO with as many intensity values as it can hold.
void ICACHE_RAM_ATTR c_OutputSerial::ISR_Handler ()
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
                if (_type == SerialType::GENERIC)
                {
                    for (auto CurrentData : GenericSerialFooter)
                    {
                        enqueue (CurrentData);
                    }
                } // need to send the footer

                // Disable TX interrupt when done
                CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

                break;
            } // end close of frame

            // Fill the FIFO with new data
            register uint16_t SpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength));

            // determine how many intensities we can process right now.
            register uint16_t DataToSend = (SpaceInFifo < RemainingDataCount) ? (SpaceInFifo) : RemainingDataCount;

            // only read from ram once per data byte
            register uint8_t data = 0;

            // reduce the remaining count of intensities by the number we are sending now.
            RemainingDataCount -= DataToSend;

            // are there intensity values to send?
            while (0 != DataToSend--)
            {
                // read the next data value from the buffer and point at the next byte
                data = *pNextChannelToSend++;

                // is this a renard port?
                if (_type == SerialType::RENARD)
                {
                    // do we have to adjust the renard data stream?
                    if ((data >= RENARD_BREAK_VALUE_LOW) && (data <= RENARD_BREAK_VALUE_HI))
                    {
                        // Send the two byte substitute for the value
                        enqueue (RENARD_BREAK_VALUE);
                        enqueue (data - RENARD_ADJUST_VALUE);

                        // show that we had to add an extra data byte to the output stream
                        --DataToSend;
                        ++RemainingDataCount;
                    } // end modified data
                    else
                    {
                        enqueue (data);
                    } // end normal data
                } // end renard mode
                else
                {
                    enqueue (data);
                } // end not renard mode
            } // end send one or more channel value

        } while (false);
    } // end this channel has an interrupt

} // ISR_Handler

void c_OutputSerial::Render () 
{
    DEBUG_START;
    // start the next frame
    switch (_type)
    {
        case SerialType::DMX512:
        {
            SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
            delayMicroseconds (DMX_BREAK);
            CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
            delayMicroseconds (DMX_MAB);

            break;
        } // DMX512

        case SerialType::RENARD:
        {
            enqueue (RENARD_FRAME_START1);
            enqueue (RENARD_FRAME_START2);

            break;
        } // RENARD

        case SerialType::GENERIC:
        {
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

    } // end switch (_type)

    // point at the input data buffer
    pNextChannelToSend = GetBufferAddress ();
    RemainingDataCount = Num_Channels;

    // fill the fifo before enableing the transmitter
    ISR_Handler ();

    // enable interrupts
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

    // turn on the transmitter
    DEBUG_END;
} // render
