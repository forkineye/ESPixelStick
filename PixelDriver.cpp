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
#include "PixelDriver.h"
#include "bitbang.h"

extern "C" {
#include "eagle_soc.h"
#include "uart_register.h"
}

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
    //SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_INV);
    SET_PERI_REG_MASK(UART_CONF0(UART), (BIT(22)));
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
    if ((pixdata = (uint8_t *)malloc(szBuffer))) {
        memset(pixdata, 0, szBuffer);
        numPixels = length;
    } else {
        numPixels = 0;
        szBuffer = 0;
    }
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

//TODO: Optimize UART buffer handling
void PixelDriver::show() {
    if (!pixdata) return;

    if (type == PixelType::WS2811) {
        char buff[4];

        /* Drop the update if our refresh rate is too high */
        if (!canRefresh(WS2811_TFRAME, WS2811_TIDLE)) return;

        while (!canShow_WS2811()) yield();
        //TODO: Until pow() is fixed and we can generate tables at runtime

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
        endTime = micros();
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
