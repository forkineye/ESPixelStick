#pragma once
/*
 * OutputUart.hpp - Uart driver code for ESPixelStick Uart Channel
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
 */

#include "ESPixelStick.h"

#ifdef ARDUINO_ARCH_ESP32
#   include <soc/uart_reg.h>
#   include <driver/uart.h>
#   include <driver/gpio.h>
#endif

#include "OutputPixel.hpp"
#include "OutputSerial.hpp"

class c_OutputUart
{
public:
    enum UartDatauint32_t
    {
        OUTPUT_UART_5N1 = 0,
        OUTPUT_UART_5N2,
        OUTPUT_UART_6N1,
        OUTPUT_UART_6N2,
        OUTPUT_UART_7N1,
        OUTPUT_UART_7N2,
        OUTPUT_UART_8N1,
        OUTPUT_UART_8N2,
    };

// TX FIFO trigger level. WS2811 40 bytes gives 100us before the FIFO goes empty
// We need to fill the FIFO at a rate faster than 0.3us per byte (1.2us/pixel)
#define DEFAULT_UART_FIFO_TRIGGER_LEVEL (17)

    enum TranslateIntensityData_t
    {
        NoTranslation = 0,
        OneToOne,
        TwoToOne,
    };

    enum UartDataBitTranslationId_t
    {
        Uart_DATA_BIT_00_ID = 0,
        Uart_DATA_BIT_01_ID,
        Uart_DATA_BIT_10_ID,
        Uart_DATA_BIT_11_ID,
        Uart_LIST_END,
        Uart_INVALID_VALUE,
        Uart_NUM_BIT_TYPES = Uart_LIST_END,
    };

    struct ConvertIntensityToUartDataStreamEntry_t
    {
        uint8_t Translation;
        c_OutputUart::UartDataBitTranslationId_t Id;
    };
    typedef ConvertIntensityToUartDataStreamEntry_t CitudsArray_t;

    struct OutputUartConfig_t
    {
        c_OutputCommon::OID_t       ChannelId                       = c_OutputCommon::OID_t(-1);
        gpio_num_t                  DataPin                         = gpio_num_t(-1);
        uart_port_t                 UartId                          = uart_port_t(-1);
        uint32_t                    IntensityDataWidth              = 8; // 8 bits in a byte
        UartDatauint32_t            UartDataSize                    = UartDatauint32_t::OUTPUT_UART_8N2;
        uint32_t                    FrameStartBreakUS               = 0;
        uint32_t                    FrameStartMarkAfterBreakUS      = 0;
        TranslateIntensityData_t    TranslateIntensityData          = TranslateIntensityData_t::NoTranslation;
        bool                        InvertOutputPolarity            = false;
        uint32_t                    Baudrate                        = 57600; // current transmit rate
        c_OutputPixel               *pPixelDataSource               = nullptr;
        uint32_t                    FiFoTriggerLevel                = DEFAULT_UART_FIFO_TRIGGER_LEVEL;
        uint16_t                    NumInterIntensityBreakBits      = 0;
        uint16_t                    NumInterIntensityMABbits        = 0;
        bool                        TriggerIsrExternally            = false;
        const CitudsArray_t        *CitudsArray                     = nullptr;

#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
            c_OutputSerial *pSerialDataSource = nullptr;
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    };

    c_OutputUart                 ();
    virtual ~c_OutputUart        ();
    void Begin                   (c_OutputUart::OutputUartConfig_t & config);
    bool SetConfig               (ArduinoJson::JsonObject &jsonConfig);
    void GetConfig               (ArduinoJson::JsonObject &jsonConfig);
    void GetDriverName           (String &sDriverName) { sDriverName = CN_OutputUart; }
    void GetStatus               (ArduinoJson::JsonObject &jsonStatus);
    void PauseOutput             (bool State);
    void StartNewFrame           ();

    void IRAM_ATTR ISR_UART_Handler();
    void IRAM_ATTR ISR_Handler_SendIntensityData();

private:
    void StartUart                      ();
    bool RegisterUartIsrHandler         ();
    void InitializeUart                 ();
    void set_pin                        ();
    void TerminateSerialPortOperation   ();
    void RestoreSerialPortOperation     ();
    void ReportNewFrame                 ();
    void StartBreak                     ();
    void EndBreak                       ();
    void GenerateBreak                  (uint32_t DurationInUs, uint32_t MarkDurationInUs);
    void SetIntensityDataWidth          ();
    void SetIntensity2Uart              (uint8_t value, UartDataBitTranslationId_t ID);

    OutputUartConfig_t OutputUartConfig;

    uint8_t Intensity2Uart[UartDataBitTranslationId_t::Uart_LIST_END];
    bool            OutputIsPaused                  = false;
    uint32_t        TxIntensityDataStartingMask     = 0x80;
    bool            HasBeenInitialized              = false;
    uint32_t        NumUartSlotsPerIntensityValue   = 1;
    uint32_t        MarkAfterInterintensityBreakBitCCOUNT          = 0;
    uint32_t        ActiveIsrMask                   = 0;
#if defined(ARDUINO_ARCH_ESP32)
    intr_handle_t   IsrHandle                       = nullptr;
    SemaphoreHandle_t  WaitFrameDone;
#endif // defined(ARDUINO_ARCH_ESP32)

    void     IRAM_ATTR      StartNewDataFrame();
    void                    CalculateEnableUartInterruptFlags();
    inline uint32_t IRAM_ATTR   getUartFifoLength();
    inline bool     IRAM_ATTR   MoreDataToSend();
    inline bool     IRAM_ATTR   GetNextIntensityToSend(uint32_t &DataToSend);
    inline void     IRAM_ATTR   enqueueUartData(uint8_t value);
    inline void     IRAM_ATTR   EnableUartInterrupts();
    inline void     IRAM_ATTR   ClearUartInterrupts();
    inline void     IRAM_ATTR   DisableUartInterrupts();

// #define USE_UART_DEBUG_COUNTERS
#ifdef USE_UART_DEBUG_COUNTERS
    // debug counters
    uint32_t DataCallbackCounter = 0;
    uint32_t DataTaskcounter = 0;
    uint32_t RxIsr = 0;
    uint32_t ErrorIsr = 0;
    uint32_t FiFoISRcounter = 0;
    uint32_t BreakIsrCounter = 0;
    uint32_t IdleIsrCounter = 0;
    uint32_t IsrIsNotForUs = 0;
    uint32_t UnknownIsr = 0;
    uint32_t TimerIsrCounter = 0;
    uint32_t TimerIsrNoDataToSend = 0;
    uint32_t TimerIsrSendData = 0;
    uint32_t FrameThresholdCounter = 0;
    uint32_t FrameEndISRcounter = 0;
    uint32_t FrameStartCounter = 0;
    uint32_t IntensityValuesSent = 0;
    uint32_t IntensityBitsSent = 0;
    uint32_t IntensityValuesSentLastFrame = 0;
    uint32_t IntensityBitsSentLastFrame = 0;
    uint32_t IncompleteFrame = 0;
    uint32_t EnqueueCounter = 0;
    uint32_t FiFoNotEmpty = 0;
    uint32_t FiFoEmpty = 0;
    uint32_t TxStopped = 0;

#endif // def USE_UART_DEBUG_COUNTERS

#ifndef UART_TX_BRK_DONE_INT_ENA
#   define UART_TX_BRK_DONE_INT_ENA 0
#endif // ndef | UART_TX_BRK_DONE_INT_ENA

#ifndef UART_TX_BRK_IDLE_DONE_INT_ENA
#   define UART_TX_BRK_IDLE_DONE_INT_ENA 0
#endif // ndef | UART_TX_BRK_IDLE_DONE_INT_ENA

#ifndef UART_INTR_MASK
#   if defined(ARDUINO_ARCH_ESP32)
#       define UART_INTR_MASK uint32_t((1 << 18) - 1)
#   elif defined(ARDUINO_ARCH_ESP8266)
#       define UART_INTR_MASK uint32_t((1 << 8) - 1)
#   endif //  defined(ARDUINO_ARCH_ESP8266)
#endif // UART_INTR_MASK

#ifndef ESP_INTR_FLAG_IRAM
#   define ESP_INTR_FLAG_IRAM 0
#endif // ndef ESP_INTR_FLAG_IRAM

// timer interrupt support for GECE on ESP8266
#if defined(ARDUINO_ARCH_ESP8266)

public:

	void IRAM_ATTR ISR_Timer_Handler();

private:
    void CalculateInterIntensityMabTime();
    inline bool IsUartTimerInUse() __attribute__((gnu_inline, const))
    {
        return (OutputUartConfig.NumInterIntensityBreakBits || OutputUartConfig.NumInterIntensityMABbits);
    }  // IsUartTimerInUse

    // Cycle counter
    // static uint32_t _getCycleCount(void) __attribute__((always_inline));
    static inline uint32_t _getCycleCount(void)
    {
        uint32_t ccount;
        __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
        return ccount;
    }

#endif // defined(ARDUINO_ARCH_ESP8266)
};
