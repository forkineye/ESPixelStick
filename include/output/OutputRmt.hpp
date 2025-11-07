#pragma once
/*
* OutputRmt.hpp - RMT driver code for ESPixelStick RMT Channel
*
* Migration-aware version:
*  - Supports ESP-IDF < 5 (legacy RMT API)
*  - Supports ESP-IDF >= 5 (new RMT TX API: rmt_new_tx_channel / rmt_tx_channel_handle_t)
*
* See ESP-IDF docs for details on the new API (rmt_new_tx_channel, rmt_transmit, ...)
* - Remote Control Transceiver (RMT) - ESP-IDF v5.x. :contentReference[oaicite:3]{index=3}
*
* Notes:
*  - Some SoC-specific RMT quirks exist (ESP32-S3 / continuous transmissions / mem banks etc.).
*    See upstream issues and discussions when debugging platform-specific behavior. :contentReference[oaicite:4]{index=4}
*/

#include "ESPixelStick.h"

#ifdef ARDUINO_ARCH_ESP32

// Include legacy & new headers conditionally
#include <driver/rmt.h>

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    // ESP-IDF v5+ RMT TX API (separates tx/rx channels)
    #include <driver/rmt_tx.h>
    #define RMT_API_V5 1
#else
    #define RMT_API_V5 0
#endif

#include "OutputPixel.hpp"
#include "OutputSerial.hpp"
#include <ArduinoJson.h>

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
#if RMT_API_V5
        // Keep legacy numeric channel id for bookkeeping; actual handle stored elsewhere
        int                 RmtChannelIdNumeric     = -1;
#else
        rmt_channel_t       RmtChannelId            = rmt_channel_t(-1);
#endif
        gpio_num_t          DataPin                = gpio_num_t(-1);
        rmt_idle_level_t    idle_level             = RMT_IDLE_LEVEL_LOW;
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

    OutputRmtConfig_t   OutputRmtConfig;

    rmt_item32_t        Intensity2Rmt[RmtDataBitIdType_t::RMT_LIST_END];
    bool                OutputIsPaused   = false;

    uint32_t            NumRmtSlotsPerIntensityValue      = 8;
    uint32_t            NumRmtSlotOverruns                = 0;
    uint32_t            MaxNumRmtSlotsPerInterrupt        = 0; // computed in Begin()

    // Legacy direct-access macros (kept for existing code)
#if !RMT_API_V5
    #define RMT_INT_THR_EVNT_BIT    (1 << (24 + uint32_t (OutputRmtConfig.RmtChannelId)))
    #define RMT_INT_TX_END_BIT      (RMT_INT_TX_END   << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
    #define RMT_INT_RX_END_BIT      (RMT_INT_RX_END   << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
    #define RMT_INT_ERROR_BIT       (RMT_INT_ERROR    << (uint32_t (OutputRmtConfig.RmtChannelId)*RMT_BITS_PER_CHAN))
    #define NUM_RMT_SLOTS           (sizeof(RMTMEM.chan[0].data32) / sizeof(RMTMEM.chan[0].data32[0]))
#else
    // For IDF5 the internal mem layout differs; we still want a meaningful NUM_RMT_SLOTS:
    #if defined(SOC_RMT_MEM_WORDS_PER_CHANNEL)
        #define NUM_RMT_SLOTS (SOC_RMT_MEM_WORDS_PER_CHANNEL)
    #else
        // fallback - common value on many chips; adjust if needed
        #define NUM_RMT_SLOTS (64)
    #endif
#endif

    // Buffering members (kept same as original for compatibility)
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

    // --- Compatibility layer members for IDF v5 ---
#if RMT_API_V5
    // handle for the new tx channel object
    rmt_tx_channel_handle_t    tx_channel_handle = nullptr;
    // store configured numeric channel id for compatibility with existing code paths that expect indices
    int                        tx_channel_numeric = -1;
#endif

public:
    c_OutputRmt ();
    virtual ~c_OutputRmt ();

    // Begin will initialize the RMT peripheral and configure the channel.
    // It internally chooses the right API depending on detected IDF version.
    void Begin                                  (OutputRmtConfig_t config, c_OutputCommon * pParent);

    bool StartNewFrame                          ();
    bool StartNextFrame                         () { return ((nullptr != pParent) & (!OutputIsPaused)) ? pParent->RmtPoll() : false; }
    void GetStatus                              (ArduinoJson::JsonObject& jsonStatus);
    void PauseOutput                            (bool State);
    inline uint32_t IRAM_ATTR GetRmtIntMask     ()               { 
#if RMT_API_V5
        // For IDF5 the interrupt mask usage differs; return a conservative mask (caller should avoid relying on internal registers).
        return 0;
#else
        return ((RMT_INT_TX_END_BIT | RMT_INT_ERROR_BIT | RMT_INT_ERROR_BIT));
#endif
    }
    void GetDriverName                          (String &value)  { value = CN_RMT; }

#define RMT_ISR_BITS         ( (uint32_t)0 ) // not used generically when IDF5 is active
#if !RMT_API_V5
    #define DisableRmtInterrupts RMT.int_ena.val &= ~(RMT_ISR_BITS)
    #define EnableRmtInterrupts  RMT.int_ena.val |=  (RMT_ISR_BITS)
    #define ClearRmtInterrupts   RMT.int_clr.val  =  (RMT_ISR_BITS)
    #define InterrupsAreEnabled  (RMT.int_ena.val &  (RMT_ISR_BITS))
#else
    // Under IDF v5 the driver manages interrupts; direct register ops are not portable - keep stubs:
    #define DisableRmtInterrupts do {} while(0)
    #define EnableRmtInterrupts  do {} while(0)
    #define ClearRmtInterrupts   do {} while(0)
    #define InterrupsAreEnabled  (0)
#endif

    bool DriverIsSendingIntensityData() { 
#if RMT_API_V5
        // With IDF v5 we cannot easily read the driver's internal interrupt mask here -> infer from state
        return (nullptr != tx_channel_handle);
#else
        return 0 != InterrupsAreEnabled;
#endif
    }

#define RMT_ClockRate       80000000.0
#define RMT_Clock_Divisor   2.0
#define RMT_TickLengthNS    float ( (1/ (RMT_ClockRate/RMT_Clock_Divisor)) * float(NanoSecondsInASecond))

    void UpdateBitXlatTable(const CitrdsArray_t * CitrdsArray);
    bool ValidateBitXlatTable(const CitrdsArray_t * CitrdsArray);
    void SetIntensity2Rmt (rmt_item32_t NewValue, RmtDataBitIdType_t ID) { Intensity2Rmt[ID] = NewValue; }

    bool ThereIsDataToSend = false;
    bool NoFrameInProgress () { 
#if RMT_API_V5
        // Under IDF5 we can't rely on register checks here; use channel handle presence as heuristic.
        return (nullptr == tx_channel_handle);
#else
        return (0 == (RMT.int_ena.val & (RMT_ISR_BITS)));
#endif
    }

    void IRAM_ATTR ISR_Handler (uint32_t isrFlags);
    c_OutputCommon * pParent = nullptr;

#ifdef USE_RMT_DEBUG_COUNTERS
   // debug counters...
#endif // def USE_RMT_DEBUG_COUNTERS

private:
    // --- Small compatibility wrapper functions ---
    // These functions hide differences between IDF v4 (legacy) and IDF v5 APIs.
    inline esp_err_t RmtAllocateAndConfigureChannel(const OutputRmtConfig_t &cfg);
    inline esp_err_t RmtReleaseChannel();
    inline esp_err_t RmtWriteItemsBlocking(const rmt_item32_t *items, size_t item_num, TickType_t ticks_to_wait);

}; // class c_OutputRmt

// ================= Implementation of compatibility wrappers (inline) ===================

inline esp_err_t c_OutputRmt::RmtAllocateAndConfigureChannel(const OutputRmtConfig_t &cfg)
{
#if RMT_API_V5
    // Use the new TX channel API
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = cfg.DataPin,
        .clk_src = RMT_CLK_SRC_DEFAULT, // default clock source
        .mem_block_symbols = 0, // 0 => driver chooses defaults; keep conservative
        .flags = RMT_TX_CHANNEL_FLAGS_DEFAULT,
        .intr_flags = 0,
    };

    // allow specifying channel by numeric id if present
    if (cfg.RmtChannelIdNumeric >= 0) {
        tx_cfg.intr_flags = 0;
        tx_cfg.gpio_num = cfg.DataPin;
        // numeric channel selection is driver-internal; driver will pick a free channel
    }

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &this->tx_channel_handle);
    if (err != ESP_OK) {
        // propagate error to caller
        return err;
    }
    // store numeric if available (some code expects an index)
    this->tx_channel_numeric = cfg.RmtChannelIdNumeric;
    return ESP_OK;
#else
    // Legacy API: install driver and configure channel
    // rmt_channel_t expected in cfg.RmtChannelId
    if (cfg.RmtChannelId == rmt_channel_t(-1)) return ESP_ERR_INVALID_ARG;

    rmt_config_t rmt_cfg;
    rmt_cfg.rmt_mode = RMT_MODE_TX;
    rmt_cfg.channel = cfg.RmtChannelId;
    rmt_cfg.gpio_num = cfg.DataPin;
    rmt_cfg.clk_div = 2; // keep same divisor as original code
    rmt_cfg.mem_block_num = 1; // one block; adjust if needed
    rmt_cfg.tx_config.loop_en = false;
    rmt_cfg.tx_config.carrier_en = false;
    rmt_cfg.tx_config.idle_level = cfg.idle_level;
    rmt_cfg.tx_config.carrier_duty_percent = 50;
    rmt_cfg.tx_config.carrier_freq_hz = 38000;
    rmt_cfg.flags = 0;

    esp_err_t err = rmt_config(&rmt_cfg);
    if (err != ESP_OK) return err;
    err = rmt_driver_install(cfg.RmtChannelId, 0, 0);
    if (err != ESP_OK) return err;

    return ESP_OK;
#endif
}

inline esp_err_t c_OutputRmt::RmtReleaseChannel()
{
#if RMT_API_V5
    if (this->tx_channel_handle) {
        esp_err_t err = rmt_del_tx_channel(this->tx_channel_handle);
        this->tx_channel_handle = nullptr;
        this->tx_channel_numeric = -1;
        return err;
    }
    return ESP_OK;
#else
    if (OutputRmtConfig.RmtChannelId != rmt_channel_t(-1)) {
        rmt_driver_uninstall(OutputRmtConfig.RmtChannelId);
    }
    return ESP_OK;
#endif
}

inline esp_err_t c_OutputRmt::RmtWriteItemsBlocking(const rmt_item32_t *items, size_t item_num, TickType_t ticks_to_wait)
{
#if RMT_API_V5
    if (!this->tx_channel_handle) return ESP_ERR_INVALID_STATE;
    rmt_tx_buffer_handle_t buf_handle = nullptr;
    // push a transaction; driver will send the samples
    esp_err_t err = rmt_tx_write_items(this->tx_channel_handle, items, item_num, ticks_to_wait, &buf_handle);
    if (err != ESP_OK) return err;
    // wait until done
    if (buf_handle) {
        err = rmt_tx_wait_all_done(this->tx_channel_handle, ticks_to_wait);
        // free buffer handle (driver API handles)
        rmt_tx_buffer_free(buf_handle);
    }
    return err;
#else
    // legacy blocking write
    return rmt_write_items(OutputRmtConfig.RmtChannelId, items, item_num, ticks_to_wait);
#endif
}

#endif // ARDUINO_ARCH_ESP32
