/*
* GECE.h - GECE driver code for ESPixelStick
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

#ifndef GECE_H_
#define GECE_H_

#include <Arduino.h>

#define UART_INV_MASK  (0x3f << 19)
#define UART 1

/*
From main sketch:
validateConfig():
        // 3 channel per pixel limits
        if (config.channel_count % 3)
            config.channel_count = (config.channel_count / 3) * 3;

        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
        break;

        config.pixel_color = PixelColor::RGB;

updateConfig():
    pixels.begin(config.pixel_type, config.pixel_color, config.channel_count / 3);
}
*/

/*
* 7N1 UART lookup table for GECE, first bit is ignored.
* Start bit and stop bits are part of the packet.
* Bits are backwards since we need MSB out.
*/

const char LOOKUP_GECE[2] = {
    0b01111100,     // 0 - (0)00 111 11(1)
    0b01100000      // 1 - (0)00 000 11(1)
};

#define GECE_DEFAULT_BRIGHTNESS 0xCC

#define GECE_ADDRESS_MASK       0x03F00000
#define GECE_BRIGHTNESS_MASK    0x000FF000
#define GECE_BLUE_MASK          0x00000F00
#define GECE_GREEN_MASK         0x000000F0
#define GECE_RED_MASK           0x0000000F

#define GECE_GET_ADDRESS(packet)    (packet >> 20) & 0x3F
#define GECE_GET_BRIGHTNESS(packet) (packet >> 12) & 0xFF
#define GECE_GET_BLUE(packet)       (packet >> 8) & 0x0F
#define GECE_GET_GREEN(packet)      (packet >> 4) & 0x0F
#define GECE_GET_RED(packet)        packet & 0x0F
#define GECE_PSIZE                  26

#define GECE_TFRAME     790L    /* 790us frame time */
#define GECE_TIDLE      45L     /* 45us idle time - should be 30us */

#define CYCLES_GECE_START   (F_CPU / 100000) // 10us

class GECE {
 public:
    int begin();
    int begin(uint16_t length);
    void setPin(uint8_t pin);
    void ICACHE_RAM_ATTR show();
    uint8_t* getData();

    /* Set channel value at address */
    inline void setValue(uint16_t address, uint8_t value) {
        pixdata[address] = value;
    }

    /* Set group / zigzag counts */
    inline void setGroup(uint16_t _group, uint16_t _zigzag) {
        this->cntGroup = _group;
        this->cntZigzag = _zigzag;
    }

    /* Drop the update if our refresh rate is too high */
    inline bool canRefresh() {
        return (micros() - startTime) >= refreshTime;
    }

 private:
    uint16_t    cntGroup;       // Output modifying interval (in LEDs, not channels)
    uint16_t    cntZigzag;      // Zigzag every cntZigzag physical pixels
    uint8_t     pin;            // Pin for bit-banging
    uint8_t     *pixdata;       // Pixel buffer
    uint8_t     *asyncdata;     // Async buffer
    uint8_t     *pbuff;         // GECE Packet Buffer
    uint16_t    numPixels;      // Number of pixels
    uint16_t    szBuffer;       // Size of Pixel buffer
    uint32_t    startTime;      // When the last frame TX started
    uint32_t    refreshTime;    // Time until we can refresh after starting a TX
    static uint8_t    rOffset;  // Index of red byte
    static uint8_t    gOffset;  // Index of green byte
    static uint8_t    bOffset;  // Index of blue byte

    void gece_init();
};

// Cycle counter
static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

#endif /* GECE_H_ */
