#include <Arduino.h>
#include "ESPixelUART.h"

extern "C" {
#include "eagle_soc.h"
#include "uart_register.h"
}

/* 6 bit UART lookup table, first 2 bits ignored. Start and stop bits are part of the pixel stream. */
const char data[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

int ESPixelUART::begin() {
    return begin(type, COLOR_RGB);
}

int ESPixelUART::begin(pixel_t type) {
	return begin(type, COLOR_RGB);
}

int ESPixelUART::begin(pixel_t type, color_t color) {
	int retval = true;

	this->type = type;
	this->color = color;

	if (type == PIXEL_WS2811)
		ws2811_init();
	else if (type == PIXEL_GECE)
		gece_init();
	else
		retval = false;

	return retval;
}

void ESPixelUART::ws2811_init() {
	/* Serial rate is 4x 800KHz for WS2811 */
	Serial1.begin(3200000, SERIAL_6N1, SERIAL_TX_ONLY);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_INV_MASK);
    //SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_INV);
    SET_PERI_REG_MASK(UART_CONF0(UART), (BIT(22)));
}

// TODO - gece support
void ESPixelUART::gece_init() {

}

void ESPixelUART::updateLength(uint16_t length) {
	if (pixdata) free(pixdata);
	//TODO: Update with if for GECE
	szBuffer = length * 3;
	if ((pixdata = (uint8_t *)malloc(szBuffer))) {
		memset(pixdata, 0, szBuffer);
		numPixels = length;
	} else {
		numPixels = 0;
		szBuffer = 0;
	}

}

void ESPixelUART::updateType(pixel_t type, color_t color) {
	this->type = type;
	this->color = color;
	
	//TODO: Add handling for switching 2811 / GECE
    switch (color) {
	    case COLOR_GRB:
    		rOffset = 1;
    		gOffset = 0;
    		bOffset = 2;
            break;
	    case COLOR_BRG:
    		rOffset = 1;
    		gOffset = 2;
    		bOffset = 0;
            break;
	    case COLOR_RBG:
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

void ESPixelUART::setPixelColor(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b) {
	if(pixel < numPixels) {
		uint8_t *p = &pixdata[pixel*3];
		p[rOffset] = r;
		p[gOffset] = g;
		p[bOffset] = b;
	}
}

//TODO: Optimize this
void ESPixelUART::show() {
	if(!pixdata) return;
	while(!canShow()) yield();

	char buff[4];
//TODO: Until pow() is fixed and we can generate tables at runtime
#ifdef GAMMA_CORRECTION
	for (uint16_t i = 0; i < szBuffer; i++) {
		buff[0] = data[(GAMMA_2811[pixdata[i]] >> 6) & 3];
		buff[1] = data[(GAMMA_2811[pixdata[i]] >> 4) & 3];
		buff[2] = data[(GAMMA_2811[pixdata[i]] >> 2) & 3];
		buff[3] = data[GAMMA_2811[pixdata[i]] & 3];
		Serial1.write(buff, sizeof(buff));		
	}
#else
	for (uint16_t i = 0; i < szBuffer; i++) {
		buff[0] = data[(pixdata[i] >> 6) & 3];
		buff[1] = data[(pixdata[i] >> 4) & 3];
		buff[2] = data[(pixdata[i] >> 2) & 3];
		buff[3] = data[pixdata[i] & 3];
		Serial1.write(buff, sizeof(buff));		
	}
#endif
    endTime = micros();
}
