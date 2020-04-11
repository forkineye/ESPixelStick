/*
* PixelDriver.cpp - Pixel driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
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

#include <Arduino.h>
#include <utility>
#include <algorithm>
#include "PixelDriver.h"

extern "C" {
#if defined(ARDUINO_ARCH_ESP8266)
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#else
#   include <esp32-hal-uart.h>
#   include <soc/soc.h>
#   include <soc/uart_reg.h>
#   include <rom/ets_sys.h>
#   include <driver/uart.h>

// Define ESP8266 style macro conversions to limit changes in the rest of the code.
#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART0                uart_port_t::UART_NUM_0
#   define UART1                uart_port_t::UART_NUM_1
#   define UART2                uart_port_t::UART_NUM_2
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN
#   define PIXEL_TXD            (13)
#   define PIXEL_RXD            (UART_PIN_NO_CHANGE)
#   define PIXEL_RTS            (UART_PIN_NO_CHANGE)
#   define PIXEL_CTS            (UART_PIN_NO_CHANGE)
#   define WS2811_DATA_SPEED    (800000)
    // Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO

    // TX FIFO trigger level. 40 bytes gives 100us before the FIFO goes empty
    // We need to fill the FIFO at a rate faster than 0.3us per byte (1.2us/pixel)
#   define PIXEL_FIFO_TRIGGER_LEVEL (40)

// static uart_intr_config_t IsrConfig;
#endif
}

#define NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define NUM_DATA_BYTES_PER_INTENSITY_BYTE	4

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

uint8_t PixelDriver::rOffset = 0;
uint8_t PixelDriver::gOffset = 1;
uint8_t PixelDriver::bOffset = 2;

int PixelDriver::begin() {
    return begin(PixelType::WS2811, PixelColor::RGB, 170);
}

int PixelDriver::begin(PixelType type) {
    return begin(type, PixelColor::RGB, 170);
}

int PixelDriver::begin(PixelType type, PixelColor color, uint16_t length) {
    int retval = true;

    this->type = type;
    this->color = color;

    updateOrder(color);

    if (pixdata) {free (pixdata); pixdata = nullptr; }
    szBuffer = length * NUM_INTENSITY_BYTES_PER_PIXEL;
    if (nullptr != (pixdata = static_cast<uint8_t *>(malloc(szBuffer+10)))) {
        memset(pixdata, 0, szBuffer);
        numPixels = length;
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }

    uint16_t szAsync = szBuffer;
    if (type == PixelType::GECE) {
        if (pbuff) free(pbuff);
        if (nullptr != (pbuff = static_cast<uint8_t *>(malloc(GECE_PSIZE+10)))) {
            memset(pbuff, 0, GECE_PSIZE);
        } else {
            numPixels = 0;
            szBuffer = 0;
            retval = false;
        }
        szAsync = GECE_PSIZE;
    }

    if (asyncdata) { free (asyncdata); asyncdata = nullptr; }
    if (nullptr != (asyncdata = static_cast<uint8_t *>(malloc(szAsync+10)))) {
        memset(asyncdata, 0, szAsync);
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }

    if (type == PixelType::WS2811) {
        refreshTime = (WS2811_TFRAME * length) + WS2811_TIDLE;
        uart_buffer = asyncdata;
        uart_buffer_tail = asyncdata + szBuffer;

        ws2811_init();
    } else if (type == PixelType::GECE) {
        refreshTime = (GECE_TFRAME + GECE_TIDLE) * length;
        gece_init();
    } else {
        retval = false;
    }

    return retval;
}

void PixelDriver::setPin (uint8_t pin) {
    if (this->pin >= 0)
    {
        this->pin = pin;
#ifdef ARDUINO_ARCH_ESP32
        uart_set_pin (UART, pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
#endif // def ARDUINO_ARCH_ESP32
    }
}

void PixelDriver::ws2811_init() {
#ifdef ARDUINO_ARCH_ESP8266
    /* Serial rate is 4x 800KHz for WS2811 */
    Serial1.begin (3200000, SERIAL_6N1, SERIAL_TX_ONLY);
    CLEAR_PERI_REG_MASK (UART_CONF0 (UART), UART_INV_MASK);
    SET_PERI_REG_MASK (UART_CONF0 (UART), (BIT (22)));

    /* Clear FIFOs */
    SET_PERI_REG_MASK (UART_CONF0 (UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK (UART_CONF0 (UART), UART_RXFIFO_RST | UART_TXFIFO_RST);

    /* Disable all interrupts */
    ETS_UART_INTR_DISABLE ();

    /* Atttach interrupt handler */
    ETS_UART_INTR_ATTACH (handleWS2811, NULL);

    /* Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO */
    WRITE_PERI_REG (UART_CONF1 (UART), 80 << UART_TXFIFO_EMPTY_THRHD_S);

    /* Disable RX & TX interrupts. It is enabled by uart.c in the SDK */
    CLEAR_PERI_REG_MASK (UART_INT_ENA (UART), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

    /* Clear all pending interrupts in UART1 */
    WRITE_PERI_REG (UART_INT_CLR (UART), 0xffff);

    /* Reenable interrupts */
    ETS_UART_INTR_ENABLE ();
#else
    // In the ESP32 you need to be careful which CPU is being configured 
    // to handle interrupts. These API functions are supposed to handle this 
    // selection.

    // kill the Arduino driver (if it is runing)
    Serial1.end ();

    // make sure no existing low level driver is running
    uart_driver_delete (UART1);

    // start the generic UART driver.
    // NOTE: Zero for RX buffer size causes errors in the uart API. 
    // Must be at least one byte larger than the fifo size

    /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate           = 4 * 800000;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_6_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;

    ESP_ERROR_CHECK (uart_driver_install   (UART, UART_FIFO_LEN+1, 0, 0, NULL, 0));
    ESP_ERROR_CHECK (uart_param_config     (UART, &uart_config));
    ESP_ERROR_CHECK (uart_set_pin          (UART, PIXEL_TXD, PIXEL_RXD, PIXEL_RTS, PIXEL_CTS));
    ESP_ERROR_CHECK (uart_set_line_inverse (UART, UART_INVERSE_TXD));
    ESP_ERROR_CHECK (uart_set_mode         (UART, uart_mode_t::UART_MODE_UART));

    // release the pre registered UART handler/subroutines
    ESP_ERROR_CHECK (uart_disable_tx_intr (UART_NUM_1));
    ESP_ERROR_CHECK (uart_disable_rx_intr (UART_NUM_1));
    ESP_ERROR_CHECK (uart_isr_free (UART_NUM_1));

    ESP_ERROR_CHECK (uart_isr_register (UART_NUM_1, handleWS2811, nullptr, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr));
#endif
}

void PixelDriver::gece_init() {
    // Serial rate is 3x 100KHz for GECE
#ifdef ARDUINO_ARCH_ESP8266
    Serial1.begin(300000, SERIAL_7N1, SERIAL_TX_ONLY);
#else
    Serial1.begin(300000, SERIAL_7N1);
#endif
    SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
    delayMicroseconds(GECE_TIDLE);
}

void PixelDriver::updateOrder(PixelColor color) {
    this->color = color;

    switch (color) {
        case PixelColor::GRB:
            rOffset = 1;
            gOffset = 0;
            bOffset = 2;
            break;
        case PixelColor::BRG:
            rOffset = 1;
            gOffset = 2;
            bOffset = 0;
            break;
        case PixelColor::RBG:
            rOffset = 0;
            gOffset = 2;
            bOffset = 1;
            break;
        case PixelColor::GBR:
            rOffset = 2;
            gOffset = 0;
            bOffset = 1;
            break;
        case PixelColor::BGR:
            rOffset = 2;
            gOffset = 1;
            bOffset = 0;
            break;
        default:
            rOffset = 0;
            gOffset = 1;
            bOffset = 2;
    }
}

void ICACHE_RAM_ATTR PixelDriver::handleWS2811(void *param) {
    /* Process if UART1 */
    if (READ_PERI_REG(UART_INT_ST(UART1))) {
        // Fill the FIFO with new data
        uart_buffer = fillWS2811(uart_buffer, uart_buffer_tail);

        // Disable TX interrupt when done
        if (uart_buffer == uart_buffer_tail)
        {
            CLEAR_PERI_REG_MASK (UART_INT_ENA (UART1), UART_TXFIFO_EMPTY_INT_ENA);
        }

        // Clear all interrupts flags (just in case)
        WRITE_PERI_REG(UART_INT_CLR(UART1), 0xffff);
    }

    /* Clear if UART0 */
    if (READ_PERI_REG(UART_INT_ST(UART0)))
        WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);

#ifdef ARDUINO_ARCH_ESP32
    // todo. Should we touching the other UARTS?
    /* Clear if UART2 */
    if (READ_PERI_REG (UART_INT_ST (UART2)))
    {
        WRITE_PERI_REG (UART_INT_CLR (UART2), UART_INTR_MASK);
    }
#endif
}

const uint8_t* ICACHE_RAM_ATTR PixelDriver::fillWS2811(const uint8_t *buff,
        const uint8_t *tail) {

    // free space in the FIFO divided by the number of data bytes per intensity 
    // gives the max number of intensities we can process
    register uint16_t IntensitySpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / NUM_DATA_BYTES_PER_INTENSITY_BYTE;

    // how many intesity bytes are present in the buffer to send?
    register uint16_t RemainingIntensityCount = (((uint16_t)(tail - buff)));

    // determine how many intensities we can process right now.
    register uint16_t IntensitiesToSend = (IntensitySpaceInFifo < RemainingIntensityCount) ? (IntensitySpaceInFifo) : RemainingIntensityCount;

    // only read from ram once per intensity
    register uint8_t subpix = 0;

    while (0 != IntensitiesToSend--)
    {
        subpix = buff[0];
        enqueue ((LOOKUP_2811[(subpix >> 6) & 0x3]));
        enqueue ((LOOKUP_2811[(subpix >> 4) & 0x3]));
        enqueue ((LOOKUP_2811[(subpix >> 2) & 0x3]));
        enqueue ((LOOKUP_2811[(subpix >> 0) & 0x3]));

        // advance to the next intensity byte in the buffer
        buff++;
    }

    // let the caller know where the next intensity byte to send is located.
    return buff;
}

void PixelDriver::show () {

    if (type == PixelType::WS2811) {
        if (!cntZigzag) {  // Normal / group copy
            for (size_t led = 0; led < szBuffer / 3; led++) {
                uint16_t modifier = led / cntGroup;
                asyncdata[3 * led + 0] = GAMMA_TABLE[pixdata[3 * modifier + rOffset]];
                asyncdata[3 * led + 1] = GAMMA_TABLE[pixdata[3 * modifier + gOffset]];
                asyncdata[3 * led + 2] = GAMMA_TABLE[pixdata[3 * modifier + bOffset]];
            }
        } else {  // Zigzag copy
            for (size_t led = 0; led < szBuffer / 3; led++) {
                uint16_t modifier = led / cntGroup;
                if (led / cntZigzag % 2) { // Odd "zig"
                    int group = cntZigzag * (led / cntZigzag);
                    int this_led = (group + cntZigzag - (led % cntZigzag) - 1) / cntGroup;
                    asyncdata[3 * led + 0] = GAMMA_TABLE[pixdata[3 * this_led + rOffset]];
                    asyncdata[3 * led + 1] = GAMMA_TABLE[pixdata[3 * this_led + gOffset]];
                    asyncdata[3 * led + 2] = GAMMA_TABLE[pixdata[3 * this_led + bOffset]];

                } else { // Even "zag"
                    asyncdata[3 * led + 0] = GAMMA_TABLE[pixdata[3 * modifier + rOffset]];
                    asyncdata[3 * led + 1] = GAMMA_TABLE[pixdata[3 * modifier + gOffset]];
                    asyncdata[3 * led + 2] = GAMMA_TABLE[pixdata[3 * modifier + bOffset]];
                }
            }
        }

                // now that we have decided on a data source, copy one 
                // pixels worth of data
                *pAsyncData++ = GAMMA_TABLE[pPixData[rOffset]];
                *pAsyncData++ = GAMMA_TABLE[pPixData[gOffset]];
                *pAsyncData++ = GAMMA_TABLE[pPixData[bOffset]];

            } // end for each pixel in the output buffer
        } // end zig zag copy

        // set the intensity transmit buffer pointers.
        uart_buffer = asyncdata;
        uart_buffer_tail = asyncdata + szBuffer;

#ifdef ARDUINO_ARCH_ESP8266
        SET_PERI_REG_MASK(UART_INT_ENA(1), UART_TXFIFO_EMPTY_INT_ENA);
#else
        ESP_ERROR_CHECK (uart_enable_tx_intr (UART, true, PIXEL_FIFO_TRIGGER_LEVEL));
#endif
        startTime = micros();

    } else if (type == PixelType::GECE) {
        uint32_t packet = 0;
        uint32_t pTime = 0;

        // Build a GECE packet
        startTime = micros();
        for (uint8_t i = 0; i < numPixels; i++) {
            packet = (packet & ~GECE_ADDRESS_MASK) | (i << 20);
            packet = (packet & ~GECE_BRIGHTNESS_MASK) |
                    (GECE_DEFAULT_BRIGHTNESS << 12);
            packet = (packet & ~GECE_BLUE_MASK) | (pixdata[i*3+2] << 4);
            packet = (packet & ~GECE_GREEN_MASK) | pixdata[i*3+1];
            packet = (packet & ~GECE_RED_MASK) | (pixdata[i*3] >> 4);

            uint8_t shift = GECE_PSIZE;
            for (uint8_t i = 0; i < GECE_PSIZE; i++)
                pbuff[i] = LOOKUP_GECE[(packet >> --shift) & 0x1];

            // Wait until ready
            while ((micros() - pTime) < (GECE_TFRAME + GECE_TIDLE)) {}

            // 10us start bit
            pTime = micros();
            uint32_t c = _getCycleCount();
            CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
            while ((_getCycleCount() - c) < CYCLES_GECE_START - 100) {}

            // Send packet and idle low (break)
            Serial1.write(pbuff, GECE_PSIZE);
            SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
        }
    }
}

uint8_t* PixelDriver::getData() {
    return asyncdata;	// data post grouping or zigzaging
//    return pixdata;
}
