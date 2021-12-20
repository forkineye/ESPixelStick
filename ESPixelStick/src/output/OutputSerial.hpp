#pragma once
/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*       Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*       See Readme for other info and version history
*
*
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
******************************************************************
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

class c_OutputSerial : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSerial (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputSerial ();

    // functions to be provided by the derived class
    void Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void Render ();                                        ///< Call from loop(),  renders output data
    void GetDriverName (String & sDriverName);
    void GetStatus (ArduinoJson::JsonObject& jsonStatus);
    uint16_t GetNumChannelsNeeded () { return Num_Channels; }
    void SetOutputBufferSize (uint16_t NumChannelsAvailable);

#define GS_CHANNEL_LIMIT 2048

    enum class BaudRate
    {
        BR_MIN = 38400,
        BR_DMX = 250000,
        BR_MAX = 460800,
        BR_DEF = 57600,
    };

    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< UART ISR

private:
    const size_t    MAX_HDR_SIZE           = 10;      // Max generic serial header size
    const size_t    MAX_FOOTER_SIZE        = 10;      // max generic serial footer size
    const size_t    MAX_CHANNELS           = 1024;
    const uint16_t  DEFAULT_NUM_CHANNELS   = 64;
    const size_t    BUF_SIZE               = (MAX_CHANNELS + MAX_HDR_SIZE + MAX_FOOTER_SIZE);
    const uint32_t  DMX_BITS_PER_BYTE      = (1.0 + 8.0 + 2.0);

#define  DMX_US_PER_BIT uint32_t((1.0 / 250000.0) * 1000000.0)

    /* DMX minimum timings per E1.11 */
    const uint8_t   DMX_BREAK              = 92; // 23 bits
    const uint8_t   DMX_MAB                = 12; //  3 bits

    bool validate ();
    void StartUart ();

    // config data
    String          GenericSerialHeader;
    String          GenericSerialFooter;
    char           *pGenericSerialFooter      = nullptr;
    size_t          LengthGenericSerialFooter = 0;
    uint32_t        CurrentBaudrate           = uint32_t(BaudRate::BR_DEF); // current transmit rate
    uint16_t        Num_Channels              = DEFAULT_NUM_CHANNELS;      // Number of data channels to transmit

    // non config data
    volatile uint16_t        RemainingDataCount;
    volatile uint8_t       * pNextChannelToSend;
    String                   OutputName;

#define USE_DMX_STATS
#ifdef USE_DMX_STATS
    uint32_t TruncateFrameError = 0;
#endif // def USE_DMX_STATS

#ifdef ARDUINO_ARCH_ESP8266
    /* Returns number of bytes waiting in the TX FIFO of UART1 */
 #  define getFifoLength ((uint16_t)((U1S >> USTXC) & 0xff))

    /* Append a byte to the TX FIFO of UART1 */
 #  define enqueue(data)  (U1F = (char)(data))

#elif defined(ARDUINO_ARCH_ESP32)

    /* Returns number of bytes waiting in the TX FIFO of UART1 */
#   define getFifoLength ((uint16_t)((READ_PERI_REG (UART_STATUS_REG (UartId)) & UART_TXFIFO_CNT_M) >> UART_TXFIFO_CNT_S))

    /* Append a byte to the TX FIFO of UART1 */
// #   define enqueue(value) WRITE_PERI_REG(UART_FIFO_AHB_REG (UART), (char)(value))
#	define enqueue(value) (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UartId)))) = (uint32_t)(value)

#endif

}; // c_OutputGenericSerial

