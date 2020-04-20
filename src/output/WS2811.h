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
#ifdef ARDUINO_ARCH_ESP8266
#define UART 1
#else
#   include <driver/uart.h>
#   define UART uart_port_t::UART_NUM_1

#endif

#define DATA_PIN        2       ///< Pixel output - GPIO2 (D4 on Wemos / NodeMCU)
#define PIXEL_LIMIT     1360    ///< Total pixel limit - 40.85ms for 8 universes


#define WS2811_TFRAME   30L     ///< 30us frame time
#define WS2811_TIDLE    300L    ///< 300us idle time

class WS2811 : public _Output  {
   private:
    static const char CONFIG_FILE[];

    // JSON configuration parameters
    String      color_order;    ///< Pixel color order
    uint16_t    pixel_count;    ///< Number of pixels
    uint16_t    zig_size;	    ///< Zigsize count - 0 = no zigzag
    uint16_t    group_size;     ///< Group size - 1 = no grouping
    float       gamma;          ///< gamma value to use
    float       brightness;     ///< brightness lto use

    // Internal variables
    uint8_t     *asyncdata;     ///< Async buffer
    uint8_t     *pbuff;         ///< GECE Packet Buffer
    uint16_t    szBuffer;       ///< Size of Pixel buffer
    uint32_t    startTime;      ///< When the last frame TX started
    uint32_t    refreshTime;    ///< Time until we can refresh after starting a TX
    static uint8_t    rOffset;  ///< Index of red byte
    static uint8_t    gOffset;  ///< Index of green byte
    static uint8_t    bOffset;  ///< Index of blue byte

    /// Interrupt Handler
    static void ICACHE_RAM_ATTR handleWS2811(void *param);

#ifdef ARDUINO_ARCH_ESP8266
    /* Returns number of bytes waiting in the TX FIFO of UART1 */
 #  define getFifoLength ((uint16_t)((U1S >> USTXC) & 0xff))

    /* Append a byte to the TX FIFO of UART1 */
 #  define enqueue(data)  (U1F = (char)(data))

#elif defined(ARDUINO_ARCH_ESP32)
    /* Returns number of bytes waiting in the TX FIFO of UART1 */
#   define getFifoLength ((uint16_t)((READ_PERI_REG (UART_STATUS_REG (UART)) & UART_TXFIFO_CNT_M) >> UART_TXFIFO_CNT_S))

    /* Append a byte to the TX FIFO of UART1 */
// #   define enqueue(value) WRITE_PERI_REG(UART_FIFO_AHB_REG (UART), (char)(value))
#	define enqueue(value) (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UART)))) = (uint32_t)(value)

#endif

    /// Drop the update if our refresh rate is too high
    inline boolean canRefresh() {
        return (micros() - startTime) >= refreshTime;
    }

    /// Generate gamma correction table
    void updateGammaTable();

    /// Update color order
    void updateColorOrder();

  public:
    // Everything below here is inherited from _Output
    static const char KEY[];
    ~WS2811();
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

#endif /* WS2811_H_ */
