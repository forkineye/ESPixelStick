#pragma once
/*
* OutputRmt.hpp - RMT driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2026 Shelby Merrick
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
#ifdef SUPPORT_RMT
#include "OutputPixel.hpp"
#include "OutputSerial.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    #include <driver/rmt_tx.h>
    #include "driver/rmt_common.h"
    #include "driver/rmt_encoder.h"
    #define rmt_item32_t rmt_symbol_word_t
    typedef enum
    {
        RMT_IDLE_LEVEL_LOW,  /*!< RMT TX idle level: low Level */
        RMT_IDLE_LEVEL_HIGH, /*!< RMT TX idle level: high Level */
        RMT_IDLE_LEVEL_MAX,
    } rmt_idle_level_t;
#else
    #include <driver/rmt.h>
    #include <hal/rmt_ll.h>
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

class c_OutputRmt
{
public:
    struct OutputRmtConfig_t
    {
        uint32_t            RmtChannelId                = uint32_t(-1);
        gpio_num_t          DataPin                     = gpio_num_t(-1);
        rmt_idle_level_t    idle_level                  = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
        void                *arg                        = nullptr;
        bool                (*ISR_GetNextIntensityBit)  (void*arg, rmt_item32_t&data) = nullptr;
        void                (*StartNewDataFrame)        (void*arg) = nullptr;
        void                *BufferStart                = nullptr;
        size_t              NumBytesInFrame             = 0;
    };

    struct isrTxFlags_t
    {
        uint32_t End = 0;
        uint32_t Err = 0;
        uint32_t Thres = 0;
    };

private:

#define MAX_NUM_RMT_CHANNELS    SOC_RMT_TX_CANDIDATES_PER_GROUP
#define NUM_RMT_SLOTS           SOC_RMT_MEM_WORDS_PER_CHANNEL

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    rmt_channel_handle_t rmt_channel_handle;
    rmt_encoder_handle_t rmt_encoder_handle;
    rmt_transmit_config_t tx_config =
    {
        .loop_count = 0, // no transfer loop
        .flags =
        {
            .eot_level = OutputRmtConfig.idle_level,    /*!< Set the output level for the "End Of Transmission" */
            .queue_nonblocking = true                   /*!< If set, when the transaction queue is full, driver will not block the thread but return directly */
        }
    };
#else
    #define RMT_INT_BIT         uint32_t(1 << uint32_t (OutputRmtConfig.RmtChannelId))
    #define InterrupsAreEnabled (0 != (RMT.int_ena.val & RMT_INT_BIT))
    rmt_item32_t * RmtChanData = nullptr;

#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)


    OutputRmtConfig_t   OutputRmtConfig;
    bool                OutputIsPaused              = false;
    uint32_t            NumRmtSlotOverruns          = 0;
    const uint32_t      MaxNumRmtSlotsPerInterrupt  = (NUM_RMT_SLOTS/2);

    #define             NumSendBufferSlots 64
    rmt_item32_t        SendBuffer[NumSendBufferSlots];
    uint32_t            RmtBufferWriteIndex         = 0;
    uint32_t            SendBufferWriteIndex        = 0;
    uint32_t            SendBufferReadIndex         = 0;
    uint32_t            NumUsedEntriesInSendBuffer  = 0;

    void ISR_TransferIntensityDataToRMT (uint32_t NumEntriesToTransfer);
    size_t ISR_TransferIntensityDataToRMT (rmt_item32_t *symbols, uint32_t MaxNumEntriesToTransfer);
    void ISR_CreateIntensityData ();
    bool ISR_MoreDataToSend();
    void StartNewDataFrame();
    void ISR_ResetRmtBlockPointers();

#ifndef HasBeenInitialized
    bool HasBeenInitialized = false;
#endif // ndef HasBeenInitialized

    TaskHandle_t SendIntensityDataTaskHandle = NULL;

public:
    c_OutputRmt ();
    virtual ~c_OutputRmt ();

    void Begin              (OutputRmtConfig_t config, c_OutputCommon * pParent);
    bool StartNewFrame      ();
    bool StartNextFrame     () { return ((nullptr != pParent) & (!OutputIsPaused)) ? pParent->RmtPoll() : false; }
    void GetStatus          (ArduinoJson::JsonObject& jsonStatus);
    void PauseOutput        (bool State);
    void GetDriverName      (String &value)  { value = CN_RMT; }
    void SetBitDuration     (double BitLenNs, rmt_item32_t & OutputBit, uint32_t & OutputNumBits);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
__attribute__((always_inline))
inline void DisableRmtInterrupts()
{
    rmt_ll_enable_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
    rmt_ll_enable_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
    rmt_ll_enable_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
} // DisableRmtInterrupts

__attribute__((always_inline))
inline void EnableRmtInterrupts()
{
    rmt_ll_enable_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
    rmt_ll_enable_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
    rmt_ll_enable_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
} // EnableRmtInterrupts

__attribute__((always_inline))
inline void ClearRmtInterrupts()
{
    rmt_ll_clear_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_clear_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_clear_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
} // ClearRmtInterrupts
#endif // ndef rmt_ll_clear_tx_thres_interrupt

#define RMT_ClockRate           80000000.0
#define RMT_Clock_Divisor       2.0
#define RMT_TICK_RESOLUTION_HZ  (RMT_ClockRate / RMT_Clock_Divisor)
#define RMT_TickLengthNS        uint32_t((1.0 / RMT_TICK_RESOLUTION_HZ) * float(NanoSecondsInASecond))

    void ISR_Handler (isrTxFlags_t isrFlags);
    size_t ISR_Handler (const void *data, size_t data_size,
                        size_t symbols_written, size_t symbols_free,
                        rmt_item32_t *symbols, bool *done);
    c_OutputCommon * pParent = nullptr;

// #define USE_RMT_DEBUG_COUNTERS
#ifdef USE_RMT_DEBUG_COUNTERS
// #define IncludeBufferData
   // debug counters
    struct RmtDebugCounters_t
    {
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
        uint32_t RmtEntriesTransfered = 0;
        uint32_t RmtXmtFills = 0;
        uint32_t ISRpaused = 0;
        uint32_t RmtWhiteDetected = 0;
        uint32_t FailedToSendAllData = 0;
        uint32_t WriteToBuffer = 0;
        uint32_t WriteToRmt = 0;
    } ;
    RmtDebugCounters_t RmtDebugCounters;

#define RMT_DEBUG_INC_COUNTER(p) (++RmtDebugCounters.p)
#define RMT_DEBUG_COUNTER(p)  (RmtDebugCounters.p)

#else

#define RMT_DEBUG_INC_COUNTER(p)
#define RMT_DEBUG_COUNTER(p)

#endif // def USE_RMT_DEBUG_COUNTERS

private:
__attribute__((always_inline))
inline void WriteToBuffer(rmt_item32_t value)
{
    RMT_DEBUG_INC_COUNTER(WriteToBuffer);

    SendBuffer[SendBufferWriteIndex] = value;
    if(++SendBufferWriteIndex >= NumSendBufferSlots) {SendBufferWriteIndex = 0;}
    ++NumUsedEntriesInSendBuffer;
} // WriteToBuffer

};
#endif // def #ifdef SUPPORT_RMT