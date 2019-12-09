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

#include "_Output.h"

#define UART_INV_MASK  (0x3f << 19)
#define UART 1

#define DATA_PIN        2   ///< Pixel output - GPIO2 (D4 on Wemos / NodeMCU)
#define PIXEL_LIMIT     63  ///< Total pixel limit - GECE Max is 63

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

class GECE: public _Output {
  private:
    static const char CONFIG_FILE[];

    // JSON configuration parameters
    uint8_t     pixel_count;

    // Internal variables
    uint8_t     *pbuff;         // GECE Packet Buffer
    //uint16_t    numPixels;      // Number of pixels
    uint16_t    szBuffer;       // Size of Pixel buffer
    uint32_t    startTime;      // When the last frame TX started
    uint32_t    refreshTime;    // Time until we can refresh after starting a TX

    void gece_init();

 public:
    void ICACHE_RAM_ATTR show();

    /* Drop the update if our refresh rate is too high */
    inline bool canRefresh() {
        return (micros() - startTime) >= refreshTime;
    }

    // Everything below here is inherited from _Output
    static const char KEY[];
    ~GECE();
    void destroy();

    const char* getKey();
    const char* getBrief();
    uint8_t getTupleSize();
    uint16_t getTupleCount();

    void validate();
    void load();
    void save();

    void init();
    void render();

    boolean deserialize(DynamicJsonDocument &json);
    String serialize(boolean pretty);
};

// Cycle counter
static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

#endif /* GECE_H_ */
