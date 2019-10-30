/*
* GECE.cpp - GECE driver code for ESPixelStick
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

#include <utility>
#include <algorithm>
#include "GECE.h"

extern "C" {
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

int GECE::begin() {
    return begin(63);
}

int GECE::begin(uint16_t length) {
    int retval = true;

    if (pixdata) free(pixdata);
    szBuffer = length * 3;
    if (pixdata = static_cast<uint8_t *>(malloc(szBuffer))) {
        memset(pixdata, 0, szBuffer);
        numPixels = length;
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }

    uint16_t szAsync = szBuffer;
    if (pbuff) free(pbuff);
    if (pbuff = static_cast<uint8_t *>(malloc(GECE_PSIZE))) {
        memset(pbuff, 0, GECE_PSIZE);
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }
    szAsync = GECE_PSIZE;

    if (asyncdata) free(asyncdata);
    if (asyncdata = static_cast<uint8_t *>(malloc(szAsync))) {
        memset(asyncdata, 0, szAsync);
    } else {
        numPixels = 0;
        szBuffer = 0;
        retval = false;
    }

    refreshTime = (GECE_TFRAME + GECE_TIDLE) * length;
    gece_init();

    return retval;
}

void GECE::setPin(uint8_t pin) {
    if (this->pin >= 0)
        this->pin = pin;
}

void GECE::gece_init() {
    // Serial rate is 3x 100KHz for GECE
    Serial1.begin(300000, SERIAL_7N1, SERIAL_TX_ONLY);
    SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
    delayMicroseconds(GECE_TIDLE);
}

void ICACHE_RAM_ATTR GECE::show() {
    if (!pixdata) return;

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

uint8_t* GECE::getData() {
    return asyncdata;	// data post grouping or zigzaging
//    return pixdata;
}
