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
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

#include "OutputSerial.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputSerialUart : public c_OutputSerial
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSerialUart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputSerialUart ();

    // functions to be provided by the derived class
    void            Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    virtual bool    SetConfig(ArduinoJson::JsonObject &jsonConfig); ///< Set a new config in the driver
    void            Render();
    /// Interrupt Handler
    void IRAM_ATTR ISR_Handler (); ///< UART ISR

private:

    void StartUart ();

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

#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
