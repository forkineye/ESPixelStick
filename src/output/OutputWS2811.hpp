#pragma once
/*
* OutputWS2811.h - WS2811 driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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
*   This is a derived class that converts data in the output buffer into 
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/

#include "OutputCommon.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputWS2811 : public c_OutputCommon  
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId, 
                      gpio_num_t outputGpio, 
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2811 ();

    // functions to be provided by the derived class
    void         Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void         Render ();                                        ///< Call from loop(),  renders output data
    void         GetDriverName (String & sDriverName) { sDriverName = String (F ("WS2811")); }
    c_OutputMgr::e_OutputType GetOutputType () {return c_OutputMgr::e_OutputType::OutputType_WS2811;} ///< Have the instance report its type.
    void         GetStatus (ArduinoJson::JsonObject & jsonStatus) { c_OutputCommon::GetStatus (jsonStatus); }
    uint16_t     GetBufferSize () { return (pixel_count * WS2812_NUM_INTENSITY_BYTES_PER_PIXEL); } ///< Get the address of the buffer into which the E1.31 handler will stuff data

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< UART ISR

#define WS2812_PIXEL_LIMIT                          1360    ///< Total pixel limit - 40.85ms for 8 universes
#define WS2812_NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE	4
#define WS2812_OUTPUT_BUFF_SIZE                     (WS2812_PIXEL_LIMIT * WS2812_NUM_INTENSITY_BYTES_PER_PIXEL)

private:
    // JSON configuration parameters
    String      color_order; ///< Pixel color order
    uint16_t    pixel_count; ///< Number of pixels
    uint16_t    zig_size;    ///< Zigsize count - 0 = no zigzag
    uint16_t    group_size;  ///< Group size - 1 = no grouping
    float       gamma;       ///< gamma value to use
    float       brightness;  ///< brightness to use

    // Internal variables
    uint8_t     IsrOutputBuffer[WS2812_OUTPUT_BUFF_SIZE+1]; ///< Data ready to be sent to the UART
    uint8_t    *pNextIntensityToSend;                       ///< start of output buffer being sent to the UART
    uint16_t    RemainingIntensityCount;                    ///< Used by ISR to determine how much more data to send
    time_t      startTime;                                  ///< When the last frame TX started
    uint8_t     rOffset;                                    ///< Index of red byte
    uint8_t     gOffset;                                    ///< Index of green byte
    uint8_t     bOffset;                                    ///< Index of blue byte
    uint8_t     gamma_table[256] = { 0 };                   ///< Gamma Adjustment table

#ifdef ARDUINO_ARCH_ESP8266
    /* Returns number of bytes waiting in the TX FIFO of UART1 */
 #  define getFifoLength ((uint16_t)((U1S >> USTXC) & 0xff))

    /* Append a byte to the TX FIFO of UART1 */
 #  define enqueue(data)  (U1F = (char)(data))

#elif defined(ARDUINO_ARCH_ESP32)

    /* Returns number of bytes waiting in the TX FIFO of UART1 */
//#   define getFifoLength ((uint16_t)((READ_PERI_REG (UART_STATUS_REG (UartId)) & UART_TXFIFO_CNT_M) >> UART_TXFIFO_CNT_S))
#   define getFifoLength 80

    /* Append a byte to the TX FIFO of UART1 */
// #   define enqueue(value) WRITE_PERI_REG(UART_FIFO_AHB_REG (UART), (char)(value))
#	define enqueue(value) (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UartId)))) = (uint32_t)(value)

#endif

    /// Drop the update if our refresh rate is too high
    inline boolean canRefresh() 
    {
        return (micros() - startTime) >= FrameRefreshTimeMs;
    }
    
    void updateGammaTable(); ///< Generate gamma correction table
    void updateColorOrder(); ///< Update color order
    bool validate ();        ///< confirm that the current configuration is valid

}; // c_OutputWS2811

