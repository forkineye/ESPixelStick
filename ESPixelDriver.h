/*
* ESPixelDriver.h - Pixel driver code for ESPixelStick
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

#ifndef ESPIXELDRIVER_H
#define ESPIXELDRIVER_H

#define UART_INV_MASK  (0x3f<<19)
#define UART 1

#define GAMMA_CORRECTION

/* Gamma correction table until pow() is fixed */
#ifdef GAMMA_CORRECTION
const uint8_t GAMMA_2811[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
    6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11,
    11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
    19, 19, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28,
    29, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 37, 38, 39, 40,
    40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 76, 77, 78, 79, 80, 81, 83, 84, 85, 86, 88, 89,
    90, 91, 93, 94, 95, 96, 98, 99,100,102,103,104,106,107,109,110,
    111,113,114,116,117,119,120,121,123,124,126,128,129,131,132,134,
    135,137,138,140,142,143,145,146,148,150,151,153,155,157,158,160,
    162,163,165,167,169,170,172,174,176,178,179,181,183,185,187,189,
    191,193,194,196,198,200,202,204,206,208,210,212,214,216,218,220,
    222,224,227,229,231,233,235,237,239,241,244,246,248,250,252,255
};
#endif

/* 
* 6 bit UART lookup table, first 2 bits ignored.
* Start and stop bits are part of the pixel stream. 
*/
const char LOOKUP_2811[4] = { 0b00110111, 0b00000111, 0b00110100, 0b00000100 };

#define GECE_DEFAULT_BRIGHTNESS 0xCC

#define GECE_ADDRESS_MASK       0x03F00000
#define GECE_BRIGHTNESS_MASK    0x000FF000
#define GECE_BLUE_MASK          0x00000F00
#define GECE_GREEN_MASK         0x000000F0
#define GECE_RED_MASK           0x0000000F

#define GECE_GET_ADDRESS(packet)     (packet >> 20) & 0x3F
#define GECE_GET_BRIGHTNESS(packet)  (packet >> 12) & 0xFF
#define GECE_GET_BLUE(packet)        (packet >> 8) & 0x0F
#define GECE_GET_GREEN(packet)       (packet >> 4) & 0x0F
#define GECE_GET_RED(packet)         packet & 0x0F


/* Pixel Types */
typedef enum {
	PIXEL_WS2811,
	PIXEL_GECE
} pixel_t;

/* Color Order */
typedef enum {
	COLOR_RGB,
	COLOR_GRB,
	COLOR_BRG,
	COLOR_RBG
} color_t;

class ESPixelDriver {
	public:
        int begin();
		int begin(pixel_t type);
		int begin(pixel_t type, color_t color);
		void setPin(uint8_t pin);
		void updateLength(uint16_t length);
		void updateOrder(color_t color);
		void setPixelColor(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b);
		void show();
		/* 50us reset for WS2811 */
		inline bool canShow(void) { return (micros() - endTime) >= 50L; }

	private:
		pixel_t		type;		// Pixel type
		color_t		color;		// Color Order
		uint8_t		pin;		// Pin for bit-banging
		uint8_t		*pixdata;	// Pixel buffer
		uint16_t	numPixels;	// Number of pixels
		uint16_t	szBuffer;	// Size of Pixel buffer
		uint8_t		rOffset;	// Index of red byte
		uint8_t		gOffset;	// Index of red byte
		uint8_t		bOffset;	// Index of red byte
        uint32_t    endTime;    // Reset tracker

		void ws2811_init();
		void gece_init();
};

#endif
