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
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

uint8_t PixelDriver::rOffset = 0;
uint8_t PixelDriver::gOffset = 1;
uint8_t PixelDriver::bOffset = 2;
uint8_t PixelDriver::wOffset = 3;

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

    if (color >= PixelColor::RGBW) {
        this->color_count = 4;
    }
    else {
        this->color_count = 3;
    }

    updateOrder(color);

    if (pixdata) free(pixdata);
    szBuffer = length * this->color_count;
    if (pixdata = static_cast<uint8_t *>(malloc(szBuffer))) {
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
        if (pbuff = static_cast<uint8_t *>(malloc(GECE_PSIZE))) {
            memset(pbuff, 0, GECE_PSIZE);
        } else {
            numPixels = 0;
            szBuffer = 0;
            retval = false;
        }
        szAsync = GECE_PSIZE;
    }

    if (asyncdata) free(asyncdata);
    if (asyncdata = static_cast<uint8_t *>(malloc(szAsync))) {
        memset(asyncdata, 0, szAsync);
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }

    if (type == PixelType::WS2811) {
        refreshTime = WS2811_TFRAME * length + WS2811_TIDLE;
        ws2811_init();
    } else if (type == PixelType::GECE) {
        refreshTime = (GECE_TFRAME + GECE_TIDLE) * length;
        gece_init();
    } else if (type == PixelType::SK6812RGBW) {
        refreshTime = SK6812_TFRAME * length + SK6812_TIDLE;
        ws2811_init();
    } else {
        retval = false;
    }

    return retval;
}

void PixelDriver::setPin(uint8_t pin) {
    if (this->pin >= 0)
        this->pin = pin;
}

void PixelDriver::ws2811_init() {
    /* Serial rate is 4x 800KHz for WS2811 */
    Serial1.begin(3200000, SERIAL_6N1, SERIAL_TX_ONLY);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(UART), (BIT(22)));

    /* Clear FIFOs */
    SET_PERI_REG_MASK(UART_CONF0(UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_RXFIFO_RST | UART_TXFIFO_RST);

    /* Disable all interrupts */
    ETS_UART_INTR_DISABLE();

    /* Atttach interrupt handler */
    ETS_UART_INTR_ATTACH(handleWS2811, this);

    /* Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO */
    WRITE_PERI_REG(UART_CONF1(UART), 80 << UART_TXFIFO_EMPTY_THRHD_S);

    /* Disable RX & TX interrupts. It is enabled by uart.c in the SDK */
    CLEAR_PERI_REG_MASK(UART_INT_ENA(UART), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

    /* Clear all pending interrupts in UART1 */
    WRITE_PERI_REG(UART_INT_CLR(UART), 0xffff);

    /* Reenable interrupts */
    ETS_UART_INTR_ENABLE();
}

void PixelDriver::gece_init() {
    // Serial rate is 3x 100KHz for GECE
    Serial1.begin(300000, SERIAL_7N1, SERIAL_TX_ONLY);
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
        case PixelColor::RGBW:
            rOffset = 0;
            gOffset = 1;
            bOffset = 2;
            wOffset = 3;
            break;
        case PixelColor::GRBW:
            rOffset = 1;
            gOffset = 0;
            bOffset = 2;
            wOffset = 3;
            break;
        case PixelColor::BRGW:
            rOffset = 1;
            gOffset = 2;
            bOffset = 0;
            wOffset = 3;
            break;
        case PixelColor::RBGW:
            rOffset = 0;
            gOffset = 2;
            bOffset = 1;
            wOffset = 3;
            break;
        case PixelColor::GBRW:
            rOffset = 2;
            gOffset = 0;
            bOffset = 1;
            wOffset = 3;
            break;
        case PixelColor::BGRW:
            rOffset = 2;
            gOffset = 1;
            bOffset = 0;
            wOffset = 3;
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
        uart_buffer = fillWS2811(uart_buffer, uart_buffer_tail, ((PixelDriver*)param)->color_count);

        // Disable TX interrupt when done
        if (uart_buffer == uart_buffer_tail)
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART1), UART_TXFIFO_EMPTY_INT_ENA);

        // Clear all interrupts flags (just in case)
        WRITE_PERI_REG(UART_INT_CLR(UART1), 0xffff);
    }

    /* Clear if UART0 */
    if (READ_PERI_REG(UART_INT_ST(UART0)))
        WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
}

const uint8_t* ICACHE_RAM_ATTR PixelDriver::fillWS2811(const uint8_t *buff, const uint8_t *tail, const uint8_t color_count) {
    uint8_t avail = (UART_TX_FIFO_SIZE - getFifoLength()) / 4;
    if (tail - buff > avail)
        tail = buff + avail;

    while (buff + (color_count - 1) < tail) {
        uint8_t subpix = buff[rOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        subpix = buff[gOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        subpix = buff[bOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        if (color_count == 4) {
            subpix = buff[wOffset];
            enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
            enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
            enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
            enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);
        }

        buff += color_count;
    }
    return buff;
}

void ICACHE_RAM_ATTR PixelDriver::show() {
    if (!pixdata) return;

    if (type == PixelType::WS2811 || type == PixelType::SK6812RGBW) {
        if (!cntZigzag) {  // Normal / group copy
            for (size_t led = 0; led < szBuffer / this->color_count; led++) {
                uint16 modifier = led / cntGroup;
                for (uint8_t c = 0; c < this->color_count; c++) {
                    asyncdata[this->color_count * led + c] = pixdata[this->color_count * modifier + c];
                }
            }
        } else {  // Zigzag copy
            for (size_t led = 0; led < szBuffer / this->color_count; led++) {
                uint16 modifier = led / cntGroup;
                if (led / cntZigzag % 2) { // Odd "zig"
                    int group = cntZigzag * (led / cntZigzag);
                    int this_led = (group + cntZigzag - (led % cntZigzag) - 1) / cntGroup;
                    for (uint8_t c = 0; c < this->color_count; c++) {
                        asyncdata[this->color_count * led + c] = pixdata[this->color_count * this_led + c];
                    }

                } else { // Even "zag"
                    for (uint8_t c = 0; c < this->color_count; c++) {
                        asyncdata[this->color_count * led + c] = pixdata[this->color_count * modifier + c];
                    }
                }
            }
        }

        uart_buffer = asyncdata;
        uart_buffer_tail = asyncdata + szBuffer;

        SET_PERI_REG_MASK(UART_INT_ENA(1), UART_TXFIFO_EMPTY_INT_ENA);
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
