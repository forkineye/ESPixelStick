#pragma once
/*
* OutputRmt.hpp - RMT driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2024 Shelby Merrick
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
#include <driver/rmt.h>
#include "OutputPixel.hpp"
#include "OutputSerial.hpp"

class c_OutputRmt
{
public:
    enum RmtDataBitIdType_t
    {
        RMT_DATA_BIT_ZERO_ID = 0,   // 0 UART 00
        RMT_DATA_BIT_ONE_ID,        // 1 UART 01
        RMT_DATA_BIT_TWO_ID,        // 2 UART 10
        RMT_DATA_BIT_THREE_ID,      // 3 UART 11
        RMT_INTERFRAME_GAP_ID,      // 4 UART Break / MAB
        RMT_STARTBIT_ID,            // 5
        RMT_STOPBIT_ID,             // 6 UART Stop/start bit
        RMT_END_OF_FRAME,           // 7
        RMT_LIST_END,               // 8 This must be last
        RMT_INVALID_VALUE,
        RMT_NUM_BIT_TYPES = RMT_LIST_END,
        RMT_STOP_START_BIT_ID = RMT_STOPBIT_ID,
    };

    struct ConvertIntensityToRmtDataStreamEntry_t
    {
        rmt_item32_t        Translation;
        RmtDataBitIdType_t  Id;
    };
    typedef ConvertIntensityToRmtDataStreamEntry_t CitrdsArray_t;

    struct OutputRmtConfig_t
    {
        rmt_channel_t       RmtChannelId           = rmt_channel_t(-1);
        gpio_num_t          DataPin                = gpio_num_t(-1);
        rmt_idle_level_t    idle_level             = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
        uint32_t            IntensityDataWidth     = 8;
        bool                SendInterIntensityBits = false;
        bool                SendEndOfFrameBits     = false;
        uint8_t             NumFrameStartBits      = 1;
        uint8_t             NumFrameStopBits       = 1;
        uint8_t             NumIdleBits            = 6;
        enum DataDirection_t
        {
            MSB2LSB = 0,
            LSB2MSB
        };
        DataDirection_t     DataDirection          = DataDirection_t::MSB2LSB;
        const CitrdsArray_t *CitrdsArray           = nullptr;

        c_OutputPixel  *pPixelDataSource      = nullptr;
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
        c_OutputSerial *pSerialDataSource = nullptr;
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    };

private:
#define MAX_NUM_RMT_CHANNELS     8
#define RMT_INT_TX_END          (1)
#define RMT_INT_RX_END          (2)
#define RMT_INT_ERROR           (4)
#define RMT_BITS_PER_CHAN       (3)

#define RMT_INT_THR_EVNT_BIT    (1 << (24 + uint32_t (OutputRmtConfig.RmtChannelId)))

#define RMT_INT_TX_END_BIT      (RMT_INT_TX_END   << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
#define RMT_INT_RX_END_BIT      (RMT_INT_RX_END   << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
#define RMT_INT_ERROR_BIT       (RMT_INT_ERROR    << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
#define NUM_RMT_SLOTS           (sizeof(RMTMEM.chan[0].data32) / sizeof(RMTMEM.chan[0].data32[0]))

    OutputRmtConfig_t   OutputRmtConfig;

    rmt_item32_t        Intensity2Rmt[RmtDataBitIdType_t::RMT_LIST_END];
    bool                OutputIsPaused   = false;

    uint32_t            NumRmtSlotsPerIntensityValue      = 8;
    uint32_t            NumRmtSlotOverruns                = 0;
    uint32_t            MaxNumRmtSlotsPerInterrupt        = (NUM_RMT_SLOTS/2);

    rmt_item32_t    SendBuffer[NUM_RMT_SLOTS * 2];
    uint32_t        RmtBufferWriteIndex         = 0;
    uint32_t        SendBufferWriteIndex        = 0;
    uint32_t        SendBufferReadIndex         = 0;
    uint32_t        NumUsedEntriesInSendBuffer  = 0;

#define MIN_FRAME_TIME_MS 25

    uint32_t            TxIntensityDataStartingMask = 0x80;
    RmtDataBitIdType_t  InterIntensityValueId       = RMT_INVALID_VALUE;

    inline void IRAM_ATTR ISR_TransferIntensityDataToRMT (uint32_t NumEntriesToTransfer);
    inline void IRAM_ATTR ISR_CreateIntensityData ();
    inline void IRAM_ATTR ISR_WriteToBuffer(uint32_t value);
    inline bool IRAM_ATTR ISR_MoreDataToSend();
    inline bool IRAM_ATTR ISR_GetNextIntensityToSend(uint32_t &DataToSend);
    inline void IRAM_ATTR ISR_StartNewDataFrame();
    inline void IRAM_ATTR ISR_ResetRmtBlockPointers();

#ifndef HasBeenInitialized
    bool HasBeenInitialized = false;
#endif // ndef HasBeenInitialized

    TaskHandle_t SendIntensityDataTaskHandle = NULL;

public:
    c_OutputRmt ();
    virtual ~c_OutputRmt ();

    void Begin                                  (OutputRmtConfig_t config, c_OutputCommon * pParent);
    bool StartNewFrame                          ();
    bool StartNextFrame                         () { return ((nullptr != pParent) & (!OutputIsPaused)) ? pParent->RmtPoll() : false; }
    void GetStatus                              (ArduinoJson::JsonObject& jsonStatus);
    void set_pin                                (gpio_num_t _DataPin) { OutputRmtConfig.DataPin = _DataPin; rmt_set_gpio (OutputRmtConfig.RmtChannelId, rmt_mode_t::RMT_MODE_TX, OutputRmtConfig.DataPin, false); }
    void PauseOutput                            (bool State);
    inline uint32_t IRAM_ATTR GetRmtIntMask     ()               { return ((RMT_INT_TX_END_BIT | RMT_INT_ERROR_BIT | RMT_INT_ERROR_BIT)); }
    void GetDriverName                          (String &value)  { value = CN_RMT; }

#define RMT_ISR_BITS         (RMT_INT_TX_END_BIT | RMT_INT_THR_EVNT_BIT)
#define DisableRmtInterrupts RMT.int_ena.val &= ~(RMT_ISR_BITS)
#define EnableRmtInterrupts  RMT.int_ena.val |=  (RMT_ISR_BITS)
#define ClearRmtInterrupts   RMT.int_clr.val  =  (RMT_ISR_BITS)
#define InterrupsAreEnabled  (RMT.int_ena.val &  (RMT_ISR_BITS))

    bool DriverIsSendingIntensityData() {return 0 != InterrupsAreEnabled;}

#define RMT_ClockRate       80000000.0
#define RMT_Clock_Divisor   2.0
#define RMT_TickLengthNS    float ( (1/ (RMT_ClockRate/RMT_Clock_Divisor)) * float(NanoSecondsInASecond))

    void SetIntensity2Rmt (rmt_item32_t NewValue, RmtDataBitIdType_t ID) { Intensity2Rmt[ID] = NewValue; }

    bool ThereIsDataToSend = false;
    bool NoFrameInProgress () { return (0 == (RMT.int_ena.val & (RMT_ISR_BITS))); }

    void IRAM_ATTR ISR_Handler (uint32_t isrFlags);
    c_OutputCommon * pParent = nullptr;

// #define USE_RMT_DEBUG_COUNTERS
#ifdef USE_RMT_DEBUG_COUNTERS
// #define IncludeBufferData
   // debug counters
   uint32_t DataCallbackCounter = 0;
   uint32_t DataTaskcounter = 0;
   uint32_t ISRcounter = 0;
   uint32_t FrameStartCounter = 0;
   uint32_t SendBlockIsrCounter = 0;
   uint32_t RanOutOfData = 0;
   uint32_t UnknownISRcounter = 0;
   uint32_t IntTxEndIsrCounter = 0;
   uint32_t IntTxThrIsrCounter = 0;
   uint32_t RxIsr = 0;
   uint32_t ErrorIsr = 0;
   uint32_t IntensityValuesSent = 0;
   uint32_t IntensityBitsSent = 0;
   uint32_t IntensityValuesSentLastFrame = 0;
   uint32_t IntensityBitsSentLastFrame = 0;
   uint32_t IncompleteFrame = 0;
   uint32_t BitTypeCounters[RmtDataBitIdType_t::RMT_NUM_BIT_TYPES];
   uint32_t RmtEntriesTransfered = 0;
   uint32_t RmtXmtFills = 0;
   uint32_t RmtWhiteDetected = 0;
   uint32_t FailedToSendAllData = 0;

#define RMT_DEBUG_COUNTER(p) p

#else

#define RMT_DEBUG_COUNTER(p)

#endif // def USE_RMT_DEBUG_COUNTERS

};
#endif // def #ifdef ARDUINO_ARCH_ESP32
