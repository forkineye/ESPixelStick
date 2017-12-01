/*
* PixelDriver.h - Pixel driver code for ESPixelStick
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

#ifndef PIXELDRIVER_H_
#define PIXELDRIVER_H_

#define UART_INV_MASK  (0x3f << 19)
#define UART 1

/* Gamma correction table */
extern uint8_t GAMMA_TABLE[];

/* 
* Inverted 6N1 UART lookup table for ws2811, first 2 bits ignored.
* Start and stop bits are part of the pixel stream. 
*/
const char LOOKUP_2811[4] = {
    0b00110111,     // 00 - (1)000 100(0)
    0b00000111,     // 01 - (1)000 111(0)
    0b00110100,     // 10 - (1)110 100(0)
    0b00000100      // 11 - (1)110 111(0)
};

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

#define WS2811_TFRAME   30L     /* 30us frame time */
#define WS2811_TIDLE    300L    /* 300us idle time */
#define GECE_TFRAME     790L    /* 790us frame time */
#define GECE_TIDLE      45L     /* 45us idle time - should be 30us */

#define CYCLES_GECE_START   (F_CPU / 100000) // 10us

/* Pixel Types */
enum class PixelType : uint8_t {
    WS2811,
    GECE
};

/* Color Order */
enum class PixelColor : uint8_t {
    RGB,
    GRB,
    BRG,
    RBG,
    GBR,
    BGR
};

class PixelDriver {
 public:
    int begin();
    int begin(PixelType type);
    int begin(PixelType type, PixelColor color, uint16_t length);
    void setPin(uint8_t pin);
    void setGamma(bool gamma);
    void updateOrder(PixelColor color);
    void ICACHE_RAM_ATTR show();
    uint8_t* getData();

    /* Set channel value at address */
    inline void setValue(uint16_t address, uint8_t value) {
        pixdata[address] = value;
    }

    /* Drop the update if our refresh rate is too high */
    inline bool canRefresh() {
        return (micros() - startTime) >= refreshTime;
    }

 private:
    PixelType  type;            // Pixel type
    PixelColor color;           // Color Order
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

    void ws2811_init();
    void gece_init();

    /* FIFO Handlers */
    static const uint8_t* ICACHE_RAM_ATTR fillWS2811(const uint8_t *buff,
            const uint8_t *tail);

    /* Interrupt Handlers */
    static void ICACHE_RAM_ATTR handleWS2811(void *param);

    /* Returns number of bytes waiting in the TX FIFO of UART1 */
    static inline uint8_t getFifoLength() {
        return (U1S >> USTXC) & 0xff;
    }

    /* Append a byte to the TX FIFO of UART1 */
    static inline void enqueue(uint8_t byte) {
        U1F = byte;
    }
};

// Cycle counter
static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

#endif /* PIXELDRIVER_H_ */
