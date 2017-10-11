/******************************************************************
*  
*       Project: ESPixelStick - An ESP8266 and E1.31 based pixel (And Serial!) driver
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
#include "SerialDriver.h"

extern "C" {
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

/* Uart Buffer tracker */
static const uint8_t *uart_buffer;
static const uint8_t *uart_buffer_tail;

int SerialDriver::begin(HardwareSerial *theSerial, SerialType type,
        uint16_t length) {
    return begin(theSerial, type, length, BaudRate::BR_57600);
}

int SerialDriver::begin(HardwareSerial *theSerial, SerialType type,
        uint16_t length, BaudRate baud) {
    int retval = true;

    _type = type;
    _serial = theSerial;
    _size = length;

    /* Initialize uart */
    /* frameTime = szSymbol * 1000000 / baud * szBuffer */
    if (type == SerialType::RENARD) {
        _size = length + 2;
        /* 10 bit symbols, no idle */
        frameTime = ceil(10.0 * 1000000.0
                / static_cast<float>(baud)
                * static_cast<float>(_size));
        _serial->begin(static_cast<uint32_t>(baud));
    } else if (type == SerialType::DMX512) {
        _size = length + 1;
        /* 11 bit symbols, add BREAK and MAB */
        frameTime = ceil(11.0 * 1000000.0
                / static_cast<float>(BaudRate::BR_250000)
                * static_cast<float>(_size)
                + static_cast<float>(DMX_BREAK)
                + static_cast<float>(DMX_MAB));
        _serial->begin(static_cast<uint32_t>(BaudRate::BR_250000), SERIAL_8N2);
    } else {
        retval = false;
    }

    /* Setup buffers */
    if (_serialdata) free(_serialdata);
    if (_serialdata = static_cast<uint8_t *>(malloc(_size))) {
        memset(_serialdata, 0, _size);
    } else {
        _size = 0;
        retval = false;
    }

    if (_asyncdata) free(_asyncdata);
    if (_asyncdata = static_cast<uint8_t *>(malloc(_size)))
        memset(_asyncdata, 0, _size);
    else
        retval = false;

    if (_serialdata && type == SerialType::RENARD) {
        _serialdata[0] = 0x7E;
        _serialdata[1] = 0x80;
    }

    /* Clear FIFOs */
    SET_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_RXFIFO_RST | UART_TXFIFO_RST);

    /* Disable all interrupts */
    ETS_UART_INTR_DISABLE();

    /* Atttach interrupt handler */
    ETS_UART_INTR_ATTACH(serial_handle, NULL);

    /* Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO */
    WRITE_PERI_REG(UART_CONF1(SEROUT_UART), 80 << UART_TXFIFO_EMPTY_THRHD_S);

    /* Disable RX & TX interrupts. It is enabled by uart.c in the SDK */
    CLEAR_PERI_REG_MASK(UART_INT_ENA(SEROUT_UART), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

    /* Clear all pending interrupts in SEROUT_UART */
    WRITE_PERI_REG(UART_INT_CLR(SEROUT_UART), 0xffff);

    /* Reenable interrupts */
    ETS_UART_INTR_ENABLE();

    return retval;
}

/* move buffer creation to being, added header bytes on eneqeue / fill fifo */
void SerialDriver::startPacket() {
    // Create a buffer and fill in header
    if (_type == SerialType::RENARD) {
        _serialdata = static_cast<uint8_t *>(malloc(_size + 2));
        _serialdata[0] = 0x7E;
        _serialdata[1] = 0x80;
    // Create buffer
    } else if (_type == SerialType::DMX512) {
        _serialdata = static_cast<uint8_t *>(malloc(_size));
    }
}

const uint8_t* ICACHE_RAM_ATTR SerialDriver::fillFifo(const uint8_t *buff, const uint8_t *tail) {
    uint8_t avail = (UART_TX_FIFO_SIZE - getFifoLength());
    if (tail - buff > avail) tail = buff + avail;
    while (buff < tail) enqueue(*buff++);
    return buff;
}

void ICACHE_RAM_ATTR SerialDriver::serial_handle(void *param) {
    /* Process and clear SEROUT_UART */
    if (READ_PERI_REG(UART_INT_ST(SEROUT_UART))) {
        // Fill the FIFO with new data
        uart_buffer = fillFifo(uart_buffer, uart_buffer_tail);

        // Clear TX interrupt when done
        if (uart_buffer == uart_buffer_tail)
            CLEAR_PERI_REG_MASK(UART_INT_ENA(SEROUT_UART), UART_TXFIFO_EMPTY_INT_ENA);

        // Clear all interrupts flags (just in case)
        WRITE_PERI_REG(UART_INT_CLR(SEROUT_UART), 0xffff);
    }

#if SEROUT_UART == 0
    /* Clear UART1 if needed */
    if (READ_PERI_REG(UART_INT_ST(UART1)))
        WRITE_PERI_REG(UART_INT_CLR(UART1), 0xffff);
#elif SEROUT_UART == 1
    /* Clear if UART0 if needed */
    if (READ_PERI_REG(UART_INT_ST(UART0)))
        WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
#endif
}


void SerialDriver::show() {
    if (!_serialdata) return;

    uart_buffer = _serialdata;
    uart_buffer_tail = _serialdata + _size;

    if (_type == SerialType::DMX512) {
        SET_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
        delayMicroseconds(DMX_BREAK);
        CLEAR_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
        delayMicroseconds(DMX_MAB);
    }

    SET_PERI_REG_MASK(UART_INT_ENA(SEROUT_UART), UART_TXFIFO_EMPTY_INT_ENA);

    startTime = micros();

    /* Copy data to the idle buffer and swap it */
    memcpy(_asyncdata, _serialdata, _size);
    std::swap(_asyncdata, _serialdata);
}


uint8_t* SerialDriver::getData() {
    return _serialdata;
}
