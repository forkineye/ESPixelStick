/*
* OutputRmt.cpp - driver code for ESPixelStick RMT Channel
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
#ifdef ARDUINO_ARCH_ESP32
#include "output/OutputRmt.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    #include <driver/rmt_tx.h>
#else
    #include <driver/rmt.h>
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

// forward declaration for the isr handler
static void IRAM_ATTR   rmt_intr_handler (void* param);
static rmt_isr_handle_t RMT_intr_handle = NULL;
static c_OutputRmt *    rmt_isr_ThisPtrs[MAX_NUM_RMT_CHANNELS];
static bool             InIsr = false;

#ifdef USE_RMT_DEBUG_COUNTERS
static uint32_t RawIsrCounter = 0;
#endif // def USE_RMT_DEBUG_COUNTERS

static TaskHandle_t SendFrameTaskHandle = NULL;
static BaseType_t xHigherPriorityTaskWoken = pdTRUE;
static uint32_t FrameCompletes = 0;
static uint32_t FrameTimeouts = 0;
static uint32_t SavedInterruptEnables = 0;
static uint32_t SavedInterruptStatus = 0;

//----------------------------------------------------------------------------
void RMT_Task (void *arg)
{
    // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));
    // pinMode(17, OUTPUT);
    // digitalWrite(17, HIGH);
    while(1)
    {
        // Give the outputs a chance to catch up.
        delay(1);

        // process all possible channels
        for (c_OutputRmt * pRmt : rmt_isr_ThisPtrs)
        {
            // do we have a driver on this channel?
            if(nullptr != pRmt)
            {
                // digitalWrite(17, LOW);

                // invoke the channel
                if (pRmt->StartNextFrame())
                {
                    // sys_delay_ms(500);
                    uint32_t NotificationValue = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(100) );
                    // digitalWrite(17, HIGH);

                    if(1 == NotificationValue)
                    {
                        // DEBUG_V("The transmission ended as expected.");
                        ++FrameCompletes;
                    }
                    else
                    {
                        ++FrameTimeouts;
                        // DEBUG_V("Transmit Timed Out.");
                    }
                }
            }
        }
    }
} // RMT_Task

//----------------------------------------------------------------------------
c_OutputRmt::c_OutputRmt()
{
    // DEBUG_START;

    memset((void *)&SendBuffer[0], 0x00, sizeof(SendBuffer));


    // DEBUG_END;
} // c_OutputRmt

//----------------------------------------------------------------------------
c_OutputRmt::~c_OutputRmt ()
{
    // DEBUG_START;

    if (HasBeenInitialized)
    {
        String Reason = (F("Shutting down an RMT channel requires a reboot"));
        RequestReboot(Reason, 100000);

        ISR_ResetRmtBlockPointers (); // Stop transmitter
        DisableRmtInterrupts();
        ClearRmtInterrupts();
        yield();
        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = (c_OutputRmt*)nullptr;
    }

    // DEBUG_END;
} // ~c_OutputRmt

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR rmt_intr_handler (void* param)
{
    RMT_DEBUG_COUNTER(RawIsrCounter++);
#ifdef DEBUG_GPIO
    // digitalWrite(DEBUG_GPIO, HIGH);
#endif // def DEBUG_GPIO
    if(!InIsr)
    {
        InIsr = true;

        // read the current ISR flags
        bool HaveAnInterrupt = false;
        c_OutputRmt::isrTxFlags_t isrTxFlags;
        // SavedInterruptEnables = RMT.int_ena.val;
        // WSavedInterruptStatus  = RMT.int_raw.val;
        HaveAnInterrupt |= (0 != (isrTxFlags.End   = rmt_ll_get_tx_end_interrupt_status(&RMT)));
        HaveAnInterrupt |= (0 != (isrTxFlags.Err   = rmt_ll_get_tx_err_interrupt_status(&RMT)));
        HaveAnInterrupt |= (0 != (isrTxFlags.Thres = rmt_ll_get_tx_thres_interrupt_status(&RMT)));
        RMT.int_clr.val = uint32_t(-1);

        while(HaveAnInterrupt)
        {
            for (auto CurrentRmtChanThisPtr : rmt_isr_ThisPtrs)
            {
                if(nullptr != CurrentRmtChanThisPtr)
                {
                    CurrentRmtChanThisPtr->ISR_Handler(isrTxFlags);
                }
            }

            HaveAnInterrupt = false;
            HaveAnInterrupt |= (0 != (isrTxFlags.End   = rmt_ll_get_tx_end_interrupt_status(&RMT)));
            HaveAnInterrupt |= (0 != (isrTxFlags.Err   = rmt_ll_get_tx_err_interrupt_status(&RMT)));
            HaveAnInterrupt |= (0 != (isrTxFlags.Thres = rmt_ll_get_tx_thres_interrupt_status(&RMT)));
            RMT.int_clr.val = uint32_t(-1);
        }
        InIsr = false;
    }
#ifdef DEBUG_GPIO
    // digitalWrite(DEBUG_GPIO, LOW);
#endif // def DEBUG_GPIO

} // rmt_intr_handler

//----------------------------------------------------------------------------
void c_OutputRmt::Begin (OutputRmtConfig_t config, c_OutputCommon * _pParent )
{
    // DEBUG_START;

    do // once
    {
        // save the new config
        OutputRmtConfig = config;

        if (nullptr == OutputRmtConfig.ISR_GetNextIntensityBit ||
            nullptr == OutputRmtConfig.StartNewDataFrame)
        {
            String Reason = (F("Invalid RMT configuration parameters. Rebooting"));
            RequestReboot(Reason, 10000);
            break;
        }

        // DEBUG_V (String("          IntensityDataWidth: ") + String(OutputRmtConfig.IntensityDataWidth));
        // DEBUG_V (String ("                    DataPin: ") + String (OutputRmtConfig.DataPin));
        // DEBUG_V (String ("               RmtChannelId: ") + String (OutputRmtConfig.RmtChannelId));

        // Configure RMT channel
        rmt_config_t RmtConfig = RMT_DEFAULT_CONFIG_TX(OutputRmtConfig.DataPin, rmt_channel_t(OutputRmtConfig.RmtChannelId));
        RmtConfig.channel = rmt_channel_t(OutputRmtConfig.RmtChannelId);
        RmtConfig.rmt_mode = rmt_mode_t::RMT_MODE_TX;
        RmtConfig.gpio_num = OutputRmtConfig.DataPin;
        RmtConfig.clk_div = RMT_Clock_Divisor;
        RmtConfig.mem_block_num = rmt_reserve_memsize_t::RMT_MEM_64;

        RmtConfig.tx_config.carrier_freq_hz = uint32_t(10); // cannot be zero due to a driver bug
        RmtConfig.tx_config.carrier_level = rmt_carrier_level_t::RMT_CARRIER_LEVEL_LOW; /*!< Level of the RMT output, when the carrier is applied */
        RmtConfig.tx_config.carrier_duty_percent = 50;
        RmtConfig.tx_config.idle_level = OutputRmtConfig.idle_level;
        RmtConfig.tx_config.carrier_en = false;
        RmtConfig.tx_config.loop_en = false;
        RmtConfig.tx_config.idle_output_en = true;
        // DEBUG_V();
        // DEBUG_V(String("RmtChannelId: ") + String(OutputRmtConfig.RmtChannelId));
        // DEBUG_V(String("     Datapin: ") + String(OutputRmtConfig.DataPin));
        ResetGpio(OutputRmtConfig.DataPin);
        ESP_ERROR_CHECK(rmt_config(&RmtConfig));

        // DEBUG_V();

        if(NULL == RMT_intr_handle)
        {
            // DEBUG_V("Allocate interrupt handler");
            // ESP_ERROR_CHECK (esp_intr_alloc (ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, rmt_intr_handler, this, &RMT_intr_handle));
            for(auto & currentThisPtr : rmt_isr_ThisPtrs)
            {
                currentThisPtr = nullptr;
            }
            ESP_ERROR_CHECK(rmt_isr_register(rmt_intr_handler, this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED, &RMT_intr_handle));
        }

        // reset the internal and external pointers to the start of the mem block
        ISR_ResetRmtBlockPointers ();

        // DEBUG_V();

        if(!SendFrameTaskHandle)
        {
            // DEBUG_V("Start SendFrameTask");
            xTaskCreatePinnedToCore(RMT_Task, "RMT_Task", 4096, NULL, 5, &SendFrameTaskHandle, 1);
            vTaskPrioritySet(SendFrameTaskHandle, 5);
        }
        // DEBUG_V("Add this instance to the running list");
        pParent = _pParent;
        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = this;

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
void c_OutputRmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;

    jsonStatus[F("NumRmtSlotOverruns")] = NumRmtSlotOverruns;
#ifdef USE_RMT_DEBUG_COUNTERS
    jsonStatus[F("OutputIsPaused")] = OutputIsPaused;
    JsonObject debugStatus = jsonStatus["RMT Debug"].to<JsonObject>();
    debugStatus["RmtChannelId"]                 = OutputRmtConfig.RmtChannelId;
    debugStatus["GPIO"]                         = OutputRmtConfig.DataPin;
    #ifdef CONFIG_IDF_TARGET_ESP32S3
    debugStatus["conf0"]                        = "0x" + String(RMT.chnconf0[OutputRmtConfig.RmtChannelId].val, HEX);
    debugStatus["conf1"]                        = "0x" + String(RMT.chmconf[OutputRmtConfig.RmtChannelId].conf0.val, HEX);
    debugStatus["tx_lim_ch"]                    = String(RMT.chn_tx_lim[OutputRmtConfig.RmtChannelId].tx_lim_chn);
    #else
    debugStatus["conf0"]                        = "0x" + String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf0.val, HEX);
    debugStatus["conf1"]                        = "0x" + String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.val, HEX);
    debugStatus["tx_lim_ch"]                    = String(RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit);
    debugStatus["int_ena"]                      = "0x" + String(SavedInterruptEnables, HEX);
    debugStatus["int_st"]                       = "0x" + String(SavedInterruptStatus, HEX);

    #endif // def CONFIG_IDF_TARGET_ESP32S3

    debugStatus["ErrorIsr"]                     = ErrorIsr;
    debugStatus["FrameCompletes"]               = FrameCompletes;
    debugStatus["FrameStartCounter"]            = FrameStartCounter;
    debugStatus["FrameTimeouts"]                = FrameTimeouts;
    debugStatus["FailedToSendAllData"]          = FailedToSendAllData;
    debugStatus["IncompleteFrame"]              = IncompleteFrame;
    debugStatus["IntensityValuesSent"]          = IntensityValuesSent;
    debugStatus["IntensityValuesSentLastFrame"] = IntensityValuesSentLastFrame;
    debugStatus["IntensityBitsSent"]            = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"]   = IntensityBitsSentLastFrame;
    debugStatus["IntTxEndIsrCounter"]           = IntTxEndIsrCounter;
    debugStatus["IntTxThrIsrCounter"]           = IntTxThrIsrCounter;
    debugStatus["ISRcounter"]                   = ISRcounter;
    debugStatus["RanOutOfData"]                 = RanOutOfData;
    debugStatus["RawIsrCounter"]                = RawIsrCounter;
    debugStatus["RMT_INT_BIT"]                  = "0x" + String (RMT_INT_BIT, HEX);
    debugStatus["RmtEntriesTransfered"]         = RmtEntriesTransfered;
    debugStatus["RmtWhiteDetected"]             = RmtWhiteDetected;
    debugStatus["RmtXmtFills"]                  = RmtXmtFills;
    debugStatus["RxIsr"]                        = RxIsr;
    debugStatus["SendBlockIsrCounter"]          = SendBlockIsrCounter;
    debugStatus["UnknownISRcounter"]            = UnknownISRcounter;

#ifdef IncludeBufferData
    {
        uint32_t index = 0;
        uint32_t * CurrentPointer = (uint32_t*)const_cast<rmt_item32_t*>(&SendBuffer[0]);
        // for(index = 0; index < NUM_RMT_SLOTS; index++)
        for(index = 0; index < 2; index++)
        {
            uint32_t data = CurrentPointer[index];
            debugStatus[String("Buffer Data ") + String(index)] = String(data, HEX);
        }
    }
    {
        uint32_t index = 0;
        uint32_t * CurrentPointer = (uint32_t*)const_cast<rmt_item32_t*>(&RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0]);
        // for(index = 0; index < NUM_RMT_SLOTS; index++)
        for(index = 0; index < 2; index++)
        {
            uint32_t data = CurrentPointer[index];
            debugStatus[String("RMT Data ") + String(index)] = String(data, HEX);
        }
    }
#endif // def IncludeBufferData
#endif // def USE_RMT_DEBUG_COUNTERS
    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputRmt::SetBitDuration (double BitLenNs, rmt_item32_t & OutputBit, uint32_t & OutputNumBits)
{
    // DEBUG_START;

    double NumBitTicks        = BitLenNs / RMT_TickLengthNS;
    double MaxTicksPerBitHalf = 0b111111111111111;
    double MaxTicksPerBit     = MaxTicksPerBitHalf * 2;
           OutputNumBits      = ceil(NumBitTicks / MaxTicksPerBit);
    if(0 == OutputNumBits) {OutputNumBits = 1;}
    uint   NumTicksPerbit     = (NumBitTicks / OutputNumBits);

    OutputBit.duration0       = uint(NumTicksPerbit / 2);
    // OutputBit.level0          = 0;
    OutputBit.duration1       = OutputBit.duration0;
    // OutputBit.level1          = 0;

    // DEBUG_V(String("            BitLenNs: ") + String(BitLenNs));
    // DEBUG_V(String("         NumBitTicks: ") + String(NumBitTicks));
    // DEBUG_V(String("  MaxTicksPerBitHalf: ") + String(MaxTicksPerBitHalf));
    // DEBUG_V(String("      MaxTicksPerBit: ") + String(MaxTicksPerBit));
    // DEBUG_V(String("       OutputNumBits: ") + String(OutputNumBits));
    // DEBUG_V(String("      NumTicksPerbit: ") + String(NumTicksPerbit));
    // DEBUG_V(String(" OutputBit.duration0: ") + String(OutputBit.duration0));
    // DEBUG_V(String(" OutputBit.duration1: ") + String(OutputBit.duration1));
    // DEBUG_V(String("     OutputBit.Total: ") + String(OutputBit.duration0 + OutputBit.duration1));

    // DEBUG_END;
} // SetBitDuration

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_CreateIntensityData ()
{
    /// DEBUG_START;
    // Serial.print('I');

    uint32_t NumAvailableBufferSlotsToFill = NumSendBufferSlots - NumUsedEntriesInSendBuffer;
    // Serial.print(String(NumAvailableBufferSlotsToFill));
    while(ThereIsDataToSend && NumAvailableBufferSlotsToFill)
    {
        // Serial.print('K');
        --NumAvailableBufferSlotsToFill;
        rmt_item32_t Data;
        ThereIsDataToSend = OutputRmtConfig.ISR_GetNextIntensityBit(OutputRmtConfig.arg, Data);
        ISR_WriteToBuffer(Data.val);
    };

    ///DEBUG_END;

} // ISR_CreateIntensityData

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler (isrTxFlags_t isrTxFlags)
{
    /// DEBUG_START;

    // uint32_t int_st = RMT.int_raw.val;
    ///DEBUG_V(String("              int_st: 0x") + String(int_st, HEX));
    ///DEBUG_V(String("         RMT_INT_BIT: 0x") + String(RMT_INT_BIT, HEX));
    // ClearRmtInterrupts;

    RMT_DEBUG_COUNTER(++ISRcounter);
    if(OutputIsPaused)
    {
        DisableRmtInterrupts();
    }
    // did the transmitter stall?
    else if (isrTxFlags.End & RMT_INT_BIT )
    {
        RMT_DEBUG_COUNTER(++IntTxEndIsrCounter);
        DisableRmtInterrupts();

        if(NumUsedEntriesInSendBuffer)
        {
            RMT_DEBUG_COUNTER(++FailedToSendAllData);
        }

        // tell the background task to start the next output
        vTaskNotifyGiveFromISR( SendFrameTaskHandle, &xHigherPriorityTaskWoken );
        // portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
    else if (isrTxFlags.Thres & RMT_INT_BIT )
    {
        RMT_DEBUG_COUNTER(++IntTxThrIsrCounter);

        // do we still have data to send?
        if(NumUsedEntriesInSendBuffer)
        {
            // LOG_PORT.println(String("NumUsedEntriesInSendBuffer1: ") + String(NumUsedEntriesInSendBuffer));
            RMT_DEBUG_COUNTER(++SendBlockIsrCounter);
            // transfer any prefetched data to the hardware transmitter
            ISR_TransferIntensityDataToRMT( MaxNumRmtSlotsPerInterrupt );
            // LOG_PORT.println(String("NumUsedEntriesInSendBuffer2: ") + String(NumUsedEntriesInSendBuffer));

            // refill the buffer
            ISR_CreateIntensityData();
            // LOG_PORT.println(String("NumUsedEntriesInSendBuffer3: ") + String(NumUsedEntriesInSendBuffer));

            // is there any data left to enqueue?
            if (!ThereIsDataToSend && 0 == NumUsedEntriesInSendBuffer)
            {
                RMT_DEBUG_COUNTER(++RanOutOfData);
                DisableRmtInterrupts();

                // tell the background task to start the next output
                vTaskNotifyGiveFromISR( SendFrameTaskHandle, &xHigherPriorityTaskWoken );
            }
        }
        // else ignore the interrupt and let the transmitter stall when it runs out of data
    }
#ifdef USE_RMT_DEBUG_COUNTERS
    else
    {
        RMT_DEBUG_COUNTER(++UnknownISRcounter);
        if (isrTxFlags.Err & RMT_INT_BIT)
        {
            ErrorIsr++;
        }
    }
#endif // def USE_RMT_DEBUG_COUNTERS

    ///DEBUG_END;
} // ISR_Handler

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_ResetRmtBlockPointers()
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    rmt_enable(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_rx_set_mem_owner(&RMT, OutputRmtConfig.RmtChannelId, rmt_ll_mem_owner_t::RMT_LL_MEM_OWNER_SW);
    rmt_ll_tx_reset_pointer(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_enable_mem_access(&RMT, RMT_DATA_MODE_MEM);
#else
    rmt_ll_tx_stop(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_rx_set_mem_owner(&RMT, OutputRmtConfig.RmtChannelId,RMT_MEM_OWNER_TX);
    rmt_ll_tx_reset_pointer(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_enable_mem_access(&RMT, RMT_DATA_MODE_MEM);
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

    memset((void*)&RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0], 0x0, sizeof(RMTMEM.chan[0].data32));

    RmtBufferWriteIndex  = 0;
    SendBufferWriteIndex = 0;
    SendBufferReadIndex  = 0;
    NumUsedEntriesInSendBuffer = 0;
}

//----------------------------------------------------------------------------
inline void c_OutputRmt::StartNewDataFrame()
{
    OutputRmtConfig.StartNewDataFrame(OutputRmtConfig.arg);
} // StartNewDataFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_TransferIntensityDataToRMT (uint32_t MaxNumEntriesToTransfer)
{
    /// DEBUG_START;

    uint32_t NumEntriesToTransfer = min(NumUsedEntriesInSendBuffer, MaxNumEntriesToTransfer);

#ifdef USE_RMT_DEBUG_COUNTERS
    if(NumEntriesToTransfer)
    {
        ++RmtXmtFills;
        RmtEntriesTransfered = NumEntriesToTransfer;
    }
#endif // def USE_RMT_DEBUG_COUNTERS
    while(NumEntriesToTransfer)
    {
        RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex++].val = SendBuffer[SendBufferReadIndex++].val;
        RmtBufferWriteIndex = (RmtBufferWriteIndex >= NUM_RMT_SLOTS ? 0 : RmtBufferWriteIndex); // do wrap
        SendBufferReadIndex &= (NumSendBufferSlots - 1); // do wrap
        --NumEntriesToTransfer;
        --NumUsedEntriesInSendBuffer;
    }

    // terminate the data stream
    RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = uint32_t(0);

    ///DEBUG_END;

} // ISR_TransferIntensityDataToRMT

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_WriteToBuffer(uint32_t value)
{
    /// DEBUG_START;

    SendBuffer[SendBufferWriteIndex++].val = value;
    SendBufferWriteIndex &= uint32_t(NumSendBufferSlots - 1);
    ++NumUsedEntriesInSendBuffer;

    ///DEBUG_END;
}

//----------------------------------------------------------------------------
void c_OutputRmt::PauseOutput(bool PauseOutput)
{
    /// DEBUG_START;

    if (OutputIsPaused == PauseOutput)
    {
        ///DEBUG_V("no change. Ignore the call");
    }
    else if (PauseOutput)
    {
        ///DEBUG_V("stop the output");
        DisableRmtInterrupts();
        ClearRmtInterrupts();
    }

    OutputIsPaused = PauseOutput;

    ///DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
bool c_OutputRmt::StartNewFrame ()
{
    // DEBUG_START;

    bool Response = false;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Paused");
            // Stop the transmitter
            DisableRmtInterrupts ();
            ISR_ResetRmtBlockPointers ();
            break;
        }

        if(InterrupsAreEnabled)
        {
            RMT_DEBUG_COUNTER(IncompleteFrame++);
        }

		// Stop the transmitter
        DisableRmtInterrupts ();
        ISR_ResetRmtBlockPointers ();

        #ifdef USE_RMT_DEBUG_COUNTERS
        FrameStartCounter++;
        IntensityValuesSentLastFrame = IntensityValuesSent;
        IntensityValuesSent          = 0;
        IntensityBitsSentLastFrame   = IntensityBitsSent;
        IntensityBitsSent            = 0;
        #endif // def USE_RMT_DEBUG_COUNTERS

        ThereIsDataToSend = true;
        // DEBUG_V();

        // set up to send a new frame
        StartNewDataFrame ();
        // DEBUG_V();

        // this fills the send buffer
        ISR_CreateIntensityData ();

        // Fill the RMT Hardware buffer
        ISR_TransferIntensityDataToRMT(NUM_RMT_SLOTS - 1);

        // this refills the send buffer
        ISR_CreateIntensityData ();

        // DEBUG_V(String("             NUM_RMT_SLOTS: ") + String(NUM_RMT_SLOTS));
        // DEBUG_V(String("NumUsedEntriesInSendBuffer: ") + String(NumUsedEntriesInSendBuffer));
        // DEBUG_V(String("         IntensityBitsSent: ") + String(IntensityBitsSent));
        // DEBUG_V(String("       IntensityValuesSent: ") + String(IntensityValuesSent));
        // DEBUG_V(String("NumInterFrameRmtSlotsCount: ") + String(NumInterFrameRmtSlotsCount));
        // DEBUG_V(String("NumFrameStartRmtSlotsCount: ") + String(NumFrameStartRmtSlotsCount));

        // At this point we have bits of data created and ready to send
        rmt_ll_tx_set_limit(&RMT, OutputRmtConfig.RmtChannelId, MaxNumRmtSlotsPerInterrupt);

        ClearRmtInterrupts();
        EnableRmtInterrupts();
        vPortYieldOtherCore(0);
        // pinMode(42, OUTPUT);
        // digitalWrite(42, LOW);
        // DEBUG_V("start the transmitter");
        rmt_ll_power_down_mem(&RMT, false);
        // rmt_set_gpio (OutputRmtConfig.RmtChannelId, rmt_mode_t::RMT_MODE_TX, OutputRmtConfig.DataPin, false);
        rmt_ll_tx_start(&RMT, OutputRmtConfig.RmtChannelId);
        // digitalWrite(42, HIGH);
        // delay(1);

        Response = true;
    } while(false);

    // DEBUG_END;
    return Response;

} // StartNewFrame

#endif // def ARDUINO_ARCH_ESP32