#pragma once
/*
* OutputCommon.hpp - Output base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
*   This is a base class used to manage the output port. It provides a common API
*   for use by the factory class to manage the object.
*/

#include "../ESPixelStick.h"

#ifdef ARDUINO_ARCH_ESP32
#   include <soc/uart_reg.h>
#   include <driver/uart.h>
#   include <driver/gpio.h>
#endif

#include "OutputMgr.hpp"

#ifndef UART_INTR_MASK
#   define UART_INTR_MASK 0x1ff
#endif

class c_OutputCommon
{
public:
    c_OutputCommon (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                    gpio_num_t outputGpio,
                    uart_port_t uart,
                    c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputCommon ();

    typedef  c_OutputMgr::e_OutputChannelIds OID_t;
    typedef  c_OutputMgr::e_OutputType       OTYPE_t;

    // functions to be provided by the derived class
    virtual void         Begin () {}                                           ///< set up the operating environment based on the current config (or defaults)
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig);     ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig);     ///< Get the current config used by the driver
    virtual void         Render () = 0;                                        ///< Call from loop(),  renders output data
    virtual void         GetDriverName (String & sDriverName) = 0;             ///< get the name for the instantiated driver
            OID_t        GetOutputChannelId () { return OutputChannelId; }     ///< return the output channel number
            uint8_t    * GetBufferAddress ()   { return pOutputBuffer;}        ///< Get the address of the buffer into which the E1.31 handler will stuff data
            size_t       GetBufferUsedSize ()  { return OutputBufferSize;}     ///< Get the address of the buffer into which the E1.31 handler will stuff data
            OTYPE_t      GetOutputType ()      { return OutputType; }          ///< Have the instance report its type.
    virtual void         GetStatus (ArduinoJson::JsonObject & jsonStatus);
            void         SetOutputBufferAddress (uint8_t* pNewOutputBuffer) { pOutputBuffer = pNewOutputBuffer; }
    virtual void         SetOutputBufferSize (size_t NewOutputBufferSize)  { OutputBufferSize = NewOutputBufferSize; };
    virtual size_t       GetNumChannelsNeeded () = 0;
    virtual void         PauseOutput (bool State) {}
    virtual void         WriteChannelData (size_t StartChannelId, size_t ChannelCount, byte *pSourceData);
    virtual void         ReadChannelData (size_t StartChannelId, size_t ChannelCount, byte *pTargetData);

protected:
#define OM_CMN_NO_CUSTOM_ISR                    (-1)

    gpio_num_t  DataPin                    = gpio_num_t (-1); ///< Output pin to use for this driver
    uart_port_t UartId;      ///< Id of the UART used by this instance of the driver
    OTYPE_t     OutputType;  ///< Type to report for this driver
    OID_t       OutputChannelId;
    bool        HasBeenInitialized         = false;
    uint32_t    FrameMinDurationInMicroSec = 20000;
    uint8_t   * pOutputBuffer              = nullptr;
    size_t      OutputBufferSize           = 0;
    uint32_t    FrameCount                 = 0;

#ifdef ARDUINO_ARCH_ESP8266
    void InitializeUart (uint32_t BaudRate,
                         uint32_t UartFlags1,
                         uint32_t UartFlags2,
                         uint32_t fifoTriggerLevel = 0);
#else
    void InitializeUart (uart_config_t & config,
                         uint32_t fifoTriggerLevel = 0);
#endif // ! def ARDUINO_ARCH_ESP8266

    void TerminateUartOperation ();
    void ReportNewFrame ();

#ifndef ESP_INTR_FLAG_IRAM
#   define ESP_INTR_FLAG_IRAM 0
#endif // ndef ESP_INTR_FLAG_IRAM

    bool RegisterUartIsrHandler(void (*fn)(void *), void *arg, int intr_alloc_flags);

    inline bool canRefresh ()
    {
        return (micros () - FrameStartTimeInMicroSec) >= FrameMinDurationInMicroSec;
    }

#ifdef ARDUINO_ARCH_ESP8266
    /* Returns number of bytes waiting in the TX FIFO of UART1 */
#  define getFifoLength ((uint16_t)((U1S >> USTXC) & 0xff))

   /* Append a byte to the TX FIFO of UART1 */
#  define enqueueUart(data)  (U1F = (char)(data))

#elif defined(ARDUINO_ARCH_ESP32)

    /* Returns number of bytes waiting in the TX FIFO of UART1 */
#   define getFifoLength ((uint16_t)((READ_PERI_REG (UART_STATUS_REG (UartId)) & UART_TXFIFO_CNT_M) >> UART_TXFIFO_CNT_S))
// #   define getFifoLength 80

    /* Append a byte to the TX FIFO of UART1 */
// #   define enqueue(value) WRITE_PERI_REG(UART_FIFO_AHB_REG (UART), (char)(value))
#	define enqueueUart(value) (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UartId)))) = (uint32_t)(value)

#endif

private:
    uint32_t    FrameRefreshTimeInMicroSec = 0;
    uint32_t    FrameStartTimeInMicroSec   = 0;

}; // c_OutputCommon
