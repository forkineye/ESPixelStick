/*
* WS2811.h - WS2811 driver code for ESPixelStick
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

#ifndef WS2811_H_
#define WS2811_H_

#include "_Output.h"

#define UART_INV_MASK  (0x3f << 19)
#define UART 1

#define DATA_PIN        2       ///< Pixel output - GPIO2 (D4 on Wemos / NodeMCU)
#define PIXEL_LIMIT     1360    ///< Total pixel limit - 40.85ms for 8 universes

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

#define WS2811_TFRAME   30L     /* 30us frame time */
#define WS2811_TIDLE    300L    /* 300us idle time */

class WS2811 : public _Output  {
   private:
    static const String CONFIG_FILE;

    // JSON configuration parameters
    String      color_order;    ///< Pixel color order
    uint16_t    pixel_count;    ///< Number of pixels
    uint16_t    zig_size;	    ///< Zigsize count - 0 = no zigzag
    uint16_t    group_size;     ///< Group size - 1 = no grouping
    float       gamma;          ///< gamma value to use
    float       brightness;     ///< brightness lto use

    // Internal variables
    uint8_t     *asyncdata;     // Async buffer
    uint8_t     *pbuff;         // GECE Packet Buffer
    uint16_t    szBuffer;       // Size of Pixel buffer
    uint32_t    startTime;      // When the last frame TX started
    uint32_t    refreshTime;    // Time until we can refresh after starting a TX
    static uint8_t    rOffset;  // Index of red byte
    static uint8_t    gOffset;  // Index of green byte
    static uint8_t    bOffset;  // Index of blue byte

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

    /* Drop the update if our refresh rate is too high */
    inline boolean canRefresh() {
        return (micros() - startTime) >= refreshTime;
    }

    /* Gamma correction table */
    void updateGammaTable();

    void updateColorOrder();

  public:
    static const String KEY;

    ~WS2811();
    void destroy();

    String getKey();
    String getBrief();
    uint8_t getTupleSize();
    uint16_t getTupleCount();

    void validate();
    void load();
    void save();

    void init();
    void render();

    void deserialize(DynamicJsonDocument &json);
    String serialize(boolean pretty);

    uint8_t* getData();
};

#endif /* WS2811_H_ */
