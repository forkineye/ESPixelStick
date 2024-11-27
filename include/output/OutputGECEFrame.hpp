#pragma once
/*
* OutputGECEFrame.h - GECE driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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

/*
    frame layout: 0x0AAIIBGR (26 bits)

    output looks like this:
    Start bit = High for 8us
    26 data bits.
        Each data bit is 29/31us
        0 = 6 us low, 25 us high
        1 = 23 us low, 6 us high
    stop bit = low for at least 45us
*/

#define GECE_ADDRESS_MASK 0x03F00000
#define GECE_ADDRESS_SHIFT 20

#define GECE_INTENSITY_MASK 0x000FF000
#define GECE_INTENSITY_SHIFT 12

#define GECE_BLUE_MASK 0x00000F00
#define GECE_BLUE_SHIFT 8

#define GECE_GREEN_MASK 0x000000F0
#define GECE_GREEN_SHIFT 0

#define GECE_RED_MASK 0x0000000F
#define GECE_RED_SHIFT 4

#define GECE_SET_ADDRESS(value)     ((uint32_t(value) << GECE_ADDRESS_SHIFT)   & GECE_ADDRESS_MASK)
#define GECE_SET_BRIGHTNESS(value)  ((uint32_t(value) << GECE_INTENSITY_SHIFT) & GECE_INTENSITY_MASK)
#define GECE_SET_BLUE(value)        ((uint32_t(value) << GECE_BLUE_SHIFT)      & GECE_BLUE_MASK)
#define GECE_SET_GREEN(value)       ((uint32_t(value))                         & GECE_GREEN_MASK)
#define GECE_SET_RED(value)         ((uint32_t(value) >> GECE_RED_SHIFT)       & GECE_RED_MASK)
