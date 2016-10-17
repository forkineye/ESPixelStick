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
#include "bitbang.h"

extern "C" {
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

/* Uart Buffer tracker */
static const uint8_t *uart_buffer;
static const uint8_t *uart_buffer_tail;

int PixelDriver::begin() {
    return begin(PixelType::WS2811, PixelColor::RGB);
}

int PixelDriver::begin(PixelType type) {
    return begin(type, PixelColor::RGB);
}

int PixelDriver::begin(PixelType type, PixelColor color) {
    int retval = true;

    this->type = type;
    this->color = color;

    updateOrder(color);

    if (type == PixelType::WS2811)
        ws2811_init();
    else if (type == PixelType::GECE)
        gece_init();
    else
        retval = false;

    return retval;
}

void PixelDriver::setPin(uint8_t pin) {
    if (this->pin >= 0)
        this->pin = pin;
}

void PixelDriver::setGamma(float gamma) {
    /* Treat this as a boolean until pow() is fixed */
    if (gamma)
        this->gamma = true;
    else
        this->gamma = false;
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
    ETS_UART_INTR_ATTACH(ws2811_handle, NULL);

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
    /* Setup for bit-banging */
    Serial1.end();
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void PixelDriver::updateLength(uint16_t length) {
    if (pixdata) free(pixdata);
    szBuffer = length * 3;
    if (pixdata = static_cast<uint8_t *>(malloc(szBuffer))) {
        memset(pixdata, 0, szBuffer);
        numPixels = length;
    } else {
        numPixels = 0;
        szBuffer = 0;
    }

    if (asyncdata) free(asyncdata);
    asyncdata = static_cast<uint8_t *>(malloc(szBuffer));
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
        default:
            rOffset = 0;
            gOffset = 1;
            bOffset = 2;
    }
}

void PixelDriver::setPixelColor(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    if (pixel < numPixels) {
        uint8_t *p = &pixdata[pixel*3];
        p[rOffset] = r;
        p[gOffset] = g;
        p[bOffset] = b;
    }
}

void ICACHE_RAM_ATTR PixelDriver::ws2811_handle(void *param) {
    /* Process if UART1 */
    if (READ_PERI_REG(UART_INT_ST(UART1))) {
        // Fill the FIFO with new data
        uart_buffer = fillFifo(uart_buffer, uart_buffer_tail);

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

const uint8_t* ICACHE_RAM_ATTR PixelDriver::fillFifo(const uint8_t *buff, const uint8_t *tail) {
    uint8_t avail = (UART_TX_FIFO_SIZE - getFifoLength()) / 4;
    if (tail - buff > avail) {
        tail = buff + avail;
    }
    while (buff < tail) {
        uint8_t subpix = *buff++;
        enqueue(LOOKUP_2811[(subpix >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(subpix >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(subpix >> 2) & 0x3]);
        enqueue(LOOKUP_2811[subpix & 0x3]);
    }
    return buff;

/*
    //TODO: Add Gamma support and runtime table generation.
    if (gamma) {
        for (uint16_t i = 0; i < szBuffer; i++) {
            buff[0] = LOOKUP_2811[(GAMMA_2811[pixdata[i]] >> 6) & 3];
            buff[1] = LOOKUP_2811[(GAMMA_2811[pixdata[i]] >> 4) & 3];
            buff[2] = LOOKUP_2811[(GAMMA_2811[pixdata[i]] >> 2) & 3];
            buff[3] = LOOKUP_2811[GAMMA_2811[pixdata[i]] & 3];
            Serial1.write(buff, sizeof(buff));
        }
    } else {
        for (uint16_t i = 0; i < szBuffer; i++) {
            buff[0] = LOOKUP_2811[(pixdata[i] >> 6) & 3];
            buff[1] = LOOKUP_2811[(pixdata[i] >> 4) & 3];
            buff[2] = LOOKUP_2811[(pixdata[i] >> 2) & 3];
            buff[3] = LOOKUP_2811[pixdata[i] & 3];
            Serial1.write(buff, sizeof(buff));
        }
    }
*/
}

void PixelDriver::show() {
    if (!pixdata) return;

    if (type == PixelType::WS2811) {
        /* Drop the update if our refresh rate is too high */
        if (!canRefresh(WS2811_TFRAME, WS2811_TIDLE))
            return;

        uart_buffer = pixdata;
        uart_buffer_tail = pixdata + szBuffer;
        SET_PERI_REG_MASK(UART_INT_ENA(1), UART_TXFIFO_EMPTY_INT_ENA);

        endTime = micros();

        // Copy the pixels to the idle buffer and swap them
        memcpy(asyncdata, pixdata, szBuffer);
        std::swap(asyncdata, pixdata);

    } else if (type == PixelType::GECE) {
        uint32_t packet = 0;

        /* Drop the update if our refresh rate is too high */
        if (!canRefresh(GECE_TFRAME, GECE_TIDLE)) return;

        /* Build a GECE packet */
        for (uint8_t i = 0; i < numPixels; i++) {
            packet = (packet & ~GECE_ADDRESS_MASK) | (i << 20);
            packet = (packet & ~GECE_BRIGHTNESS_MASK) |
                    (GECE_DEFAULT_BRIGHTNESS << 12);
            packet = (packet & ~GECE_BLUE_MASK) | (pixdata[i*3+2] << 4);
            packet = (packet & ~GECE_GREEN_MASK) | pixdata[i*3+1];
            packet = (packet & ~GECE_RED_MASK) | (pixdata[i*3] >> 4);

            /* and send it */
            noInterrupts();
            doGECE(pin, packet);
            interrupts();
        }
        endTime = micros();
    }
}
