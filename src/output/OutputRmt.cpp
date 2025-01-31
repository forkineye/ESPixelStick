/*
* OutputRmt.cpp - driver code for ESPixelStick RMT Channel
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
#include "output/OutputRmt.hpp"

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

//----------------------------------------------------------------------------
void RMT_Task (void *arg)
{
    // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));

    unsigned long  FrameStartTimeMS = millis();
    unsigned long  FrameEndTimeMS = FrameStartTimeMS;
    const uint32_t MinFrameTimeMs = 25;
    TickType_t     DelayTimeTicks = pdMS_TO_TICKS(MinFrameTimeMs);

    while(1)
    {
        uint32_t DeltaTimeMS = FrameEndTimeMS - FrameStartTimeMS;
        // did the timer wrap?
        if (DeltaTimeMS > MinFrameTimeMs)
        {
            // Timer has wrapped or
            // frame took longer than 25MS to run.
            // Dont wait a long time for the next one.
            DelayTimeTicks = pdMS_TO_TICKS(1);
        }
        else
        {
            DelayTimeTicks = pdMS_TO_TICKS( MinFrameTimeMs - DeltaTimeMS );
        }

        vTaskDelay(DelayTimeTicks);

        FrameStartTimeMS = millis();
        // process all possible channels
        for (c_OutputRmt * pRmt : rmt_isr_ThisPtrs)
        {
            // do we have a driver on this channel?
            if(nullptr != pRmt)
            {
                // invoke the channel
                if (pRmt->StartNextFrame())
                {
                    uint32_t NotificationValue = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(100) );
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
        // record the loop end time
        FrameEndTimeMS = millis();
    }
} // RMT_Task

//----------------------------------------------------------------------------
c_OutputRmt::c_OutputRmt()
{
    // DEBUG_START;

    memset((void *)&Intensity2Rmt[0], 0x00, sizeof(Intensity2Rmt));
    memset((void *)&SendBuffer[0],    0x00, sizeof(SendBuffer));

#ifdef USE_RMT_DEBUG_COUNTERS
    memset((void *)&BitTypeCounters[0], 0x00, sizeof(BitTypeCounters));
#endif // def USE_RMT_DEBUG_COUNTERS

    // DEBUG_END;
} // c_OutputRmt

//----------------------------------------------------------------------------
c_OutputRmt::~c_OutputRmt ()
{
    // DEBUG_START;

    if (HasBeenInitialized)
    {
        logcon(F("Shutting down an RMT channel requires a reboot"));
        RequestReboot(100000);

        DisableRmtInterrupts;
        ClearRmtInterrupts;

        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = (c_OutputRmt*)nullptr;
        RMT.data_ch[OutputRmtConfig.RmtChannelId] = 0x00;
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
        uint32_t isrFlags = RMT.int_st.val;
        RMT.int_clr.val = uint32_t(-1);

        while(0 != isrFlags)
        {
            for (auto CurrentRmtChanThisPtr : rmt_isr_ThisPtrs)
            {
                if(nullptr != CurrentRmtChanThisPtr)
                {
                    CurrentRmtChanThisPtr->ISR_Handler(isrFlags);
                }
            }

            isrFlags = (volatile uint32_t) RMT.int_st.val;
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
        OutputRmtConfig = config;
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
        if ((nullptr == OutputRmtConfig.pPixelDataSource) && (nullptr == OutputRmtConfig.pSerialDataSource))
#else
        if (nullptr == OutputRmtConfig.pPixelDataSource)
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

        {
            LOG_PORT.println (F("Invalid RMT configuration parameters. Rebooting"));
            RequestReboot(100000);;
            break;
        }

        NumRmtSlotsPerIntensityValue = OutputRmtConfig.IntensityDataWidth + ((OutputRmtConfig.SendInterIntensityBits) ? 1 : 0);
        if(OutputRmtConfig_t::DataDirection_t::MSB2LSB == OutputRmtConfig.DataDirection)
        {
            TxIntensityDataStartingMask = 1 << (OutputRmtConfig.IntensityDataWidth - 1);
        }
        else
        {
            TxIntensityDataStartingMask = 1;
        }
        // DEBUG_V (String("          IntensityDataWidth: ") + String(OutputRmtConfig.IntensityDataWidth));
        // DEBUG_V (String("NumRmtSlotsPerIntensityValue: ") + String (NumRmtSlotsPerIntensityValue));
        // DEBUG_V(String("  TxIntensityDataStartingMask: 0x") + String(TxIntensityDataStartingMask, HEX));
        // DEBUG_V (String ("                    DataPin: ") + String (OutputRmtConfig.DataPin));
        // DEBUG_V (String ("               RmtChannelId: ") + String (OutputRmtConfig.RmtChannelId));

        // Configure RMT channel

        rmt_config_t RmtConfig;
        RmtConfig.rmt_mode = rmt_mode_t::RMT_MODE_TX;
        RmtConfig.channel = OutputRmtConfig.RmtChannelId;
        RmtConfig.gpio_num = OutputRmtConfig.DataPin;
        RmtConfig.clk_div = RMT_Clock_Divisor;
        RmtConfig.mem_block_num = rmt_reserve_memsize_t::RMT_MEM_64;

        RmtConfig.tx_config.carrier_freq_hz = uint32_t(10); // cannot be zero due to a driver bug
        RmtConfig.tx_config.carrier_level = rmt_carrier_level_t::RMT_CARRIER_LEVEL_LOW; /*!< Level of the RMT output, when the carrier is applied */
        RmtConfig.tx_config.carrier_duty_percent = 50;
        RmtConfig.tx_config.idle_level = OutputRmtConfig.idle_level;
        RmtConfig.tx_config.carrier_en = false;
        RmtConfig.tx_config.loop_en = true;
        RmtConfig.tx_config.idle_output_en = true;
        // DEBUG_V();
        ResetGpio(OutputRmtConfig.DataPin);
        ESP_ERROR_CHECK(rmt_config(&RmtConfig));

        // DEBUG_V();
        // ESP_ERROR_CHECK(rmt_set_source_clk(RmtConfig.channel, rmt_source_clk_t::RMT_BASECLK_APB));

        if(NULL == RMT_intr_handle)
        {
            // DEBUG_V();
            // ESP_ERROR_CHECK (esp_intr_alloc (ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, rmt_intr_handler, this, &RMT_intr_handle));
            for(auto & currentThisPtr : rmt_isr_ThisPtrs)
            {
                currentThisPtr = nullptr;
            }
            ESP_ERROR_CHECK(rmt_isr_register(rmt_intr_handler, this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED, &RMT_intr_handle));
        }
        // DEBUG_V();

        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_conti_mode = 0;
        RMT.apb_conf.mem_tx_wrap_en = 1;  // data wrap allowed

        // DEBUG_V();

        // reset the internal and external pointers to the start of the mem block
        ISR_ResetRmtBlockPointers ();
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.apb_mem_rst = 0;/* Make sure the FIFO is not in reset */
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_owner = RMT_MEM_OWNER_TX;  /* TX owns the memory */

        RMT.apb_conf.fifo_mask = RMT_DATA_MODE_MEM;
        memset((void*)&RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0], 0x0, sizeof(RMTMEM.chan[0].data32));

        // DEBUG_V();

        // this should be a vector.
        if (nullptr != OutputRmtConfig.CitrdsArray)
        {
            // DEBUG_V();
            const ConvertIntensityToRmtDataStreamEntry_t *CurrentTranslation = OutputRmtConfig.CitrdsArray;
            while (CurrentTranslation->Id != RmtDataBitIdType_t::RMT_LIST_END)
            {
                // DEBUG_V(String("CurrentTranslation->Id: ") + String(uint32_t(CurrentTranslation->Id)));
                SetIntensity2Rmt(CurrentTranslation->Translation, CurrentTranslation->Id);
                CurrentTranslation++;
            }
        }

        // DEBUG_V (String ("                Intensity2Rmt[0]: 0x") + String (uint32_t (Intensity2Rmt[0].val), HEX));
        // DEBUG_V (String ("                Intensity2Rmt[1]: 0x") + String (uint32_t (Intensity2Rmt[1].val), HEX));
        if(!SendFrameTaskHandle)
        {
            // DEBUG_V("Start SendFrameTask");
            xTaskCreatePinnedToCore(RMT_Task, "RMT_Task", 4096, NULL, 5, &SendFrameTaskHandle, 0);
            vTaskPrioritySet(SendFrameTaskHandle, 5);
        }
        // DEBUG_V("Add this instance to the running list");
        pParent = _pParent;
        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = this;

        ResetGpio(OutputRmtConfig.DataPin);
        rmt_set_gpio(OutputRmtConfig.RmtChannelId, RMT_MODE_TX, OutputRmtConfig.DataPin, false);

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // init

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
    debugStatus["conf0"]                        = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf0.val, HEX);
    debugStatus["conf1"]                        = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.val, HEX);
    debugStatus["tx_lim_ch"]                    = String(RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit);

    debugStatus["ErrorIsr"]                     = ErrorIsr;
    debugStatus["FrameCompletes"]               = String (FrameCompletes);
    debugStatus["FrameStartCounter"]            = FrameStartCounter;
    debugStatus["FrameTimeouts"]                = String (FrameTimeouts);
    debugStatus["FailedToSendAllData"]          = String (FailedToSendAllData);
    debugStatus["IncompleteFrame"]              = IncompleteFrame;
    debugStatus["IntensityValuesSent"]          = IntensityValuesSent;
    debugStatus["IntensityValuesSentLastFrame"] = IntensityValuesSentLastFrame;
    debugStatus["IntensityBitsSent"]            = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"]   = IntensityBitsSentLastFrame;
    debugStatus["IntTxEndIsrCounter"]           = IntTxEndIsrCounter;
    debugStatus["IntTxThrIsrCounter"]           = IntTxThrIsrCounter;
    debugStatus["ISRcounter"]                   = ISRcounter;
    debugStatus["NumIdleBits"]                  = OutputRmtConfig.NumIdleBits;
    debugStatus["NumFrameStartBits"]            = OutputRmtConfig.NumFrameStartBits;
    debugStatus["NumFrameStopBits"]             = OutputRmtConfig.NumFrameStopBits;
    debugStatus["NumRmtSlotsPerIntensityValue"] = NumRmtSlotsPerIntensityValue;
    debugStatus["OneBitValue"]                  = String (Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val,  HEX);
    debugStatus["RanOutOfData"]                 = RanOutOfData;
    uint32_t temp = RMT.int_ena.val;
    debugStatus["Raw int_ena"]                  = "0x" + String (temp, HEX);
    temp = RMT.int_st.val;
    debugStatus["Raw int_st"]                   = "0x" + String (temp, HEX);
    debugStatus["RawIsrCounter"]                = RawIsrCounter;
    debugStatus["RMT_INT_TX_END_BIT"]           = "0x" + String (RMT_INT_TX_END_BIT, HEX);
    debugStatus["RMT_INT_THR_EVNT_BIT"]         = "0x" + String (RMT_INT_THR_EVNT_BIT, HEX);
    debugStatus["RmtEntriesTransfered"]         = RmtEntriesTransfered;
    debugStatus["RmtWhiteDetected"]             = String (RmtWhiteDetected);
    debugStatus["RmtXmtFills"]                  = RmtXmtFills;
    debugStatus["RxIsr"]                        = RxIsr;
    debugStatus["SendBlockIsrCounter"]          = SendBlockIsrCounter;
    debugStatus["SendInterIntensityBits"]       = OutputRmtConfig.SendInterIntensityBits;
    debugStatus["SendEndOfFrameBits"]           = OutputRmtConfig.SendEndOfFrameBits;
    debugStatus["TX END int_ena"]               = "0x" + String (temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX END int_st"]                = "0x" + String (temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX THRSH int_ena"]             = "0x" + String (temp & (RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["TX THRSH int_st"]              = "0x" + String (temp & (RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["UnknownISRcounter"]            = UnknownISRcounter;
    debugStatus["ZeroBitValue"]                 = String (Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val, HEX);

#ifdef IncludeBufferData
    {
        uint32_t index = 0;
        for (auto CurrentCounter : BitTypeCounters)
        {
            break;
            debugStatus[String("RMT TYPE used Counter ") + String(index++)] = String(CurrentCounter);
        }
    }
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
void IRAM_ATTR c_OutputRmt::ISR_CreateIntensityData ()
{
    // //DEBUG_START;

    register uint32_t OneBitValue  = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val;
    register uint32_t ZeroBitValue = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val;

    register uint32_t IntensityValue; // = 0;
    uint32_t NumAvailableBufferSlotsToFill = NUM_RMT_SLOTS - NumUsedEntriesInSendBuffer;
    while ((NumAvailableBufferSlotsToFill > NumRmtSlotsPerIntensityValue) && ThereIsDataToSend)
    {
        ThereIsDataToSend = ISR_GetNextIntensityToSend(IntensityValue);
        RMT_DEBUG_COUNTER(IntensityValuesSent++);
#ifdef USE_RMT_DEBUG_COUNTERS
        if(200 < IntensityValue)
        {
            ++RmtWhiteDetected;
        }
#endif // def USE_RMT_DEBUG_COUNTERS

        // convert the intensity data into RMT slot data
        uint32_t bitmask = TxIntensityDataStartingMask;
        for (uint32_t BitCount = OutputRmtConfig.IntensityDataWidth; 0 < BitCount; --BitCount)
        {
            RMT_DEBUG_COUNTER(IntensityBitsSent++);
            ISR_WriteToBuffer((IntensityValue & bitmask) ? OneBitValue : ZeroBitValue);
            if(OutputRmtConfig_t::DataDirection_t::MSB2LSB == OutputRmtConfig.DataDirection)
            {
                bitmask >>= 1;
            }
            else
            {
                bitmask <<= 1;
            }
#ifdef USE_RMT_DEBUG_COUNTERS
            if (IntensityValue & bitmask)
            {
                BitTypeCounters[int(RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID)]++;
            }
            else
            {
                BitTypeCounters[int(RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID)]++;
            }
#endif // def USE_RMT_DEBUG_COUNTERS
        } // end send one intensity value

        if (OutputRmtConfig.SendEndOfFrameBits && !ThereIsDataToSend)
        {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_END_OF_FRAME].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_END_OF_FRAME)]++);
        }
        else if (OutputRmtConfig.SendInterIntensityBits)
        {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_STOP_START_BIT_ID].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_STOP_START_BIT_ID)]++);
        }

        // recalc how much space is still in the buffer.
        NumAvailableBufferSlotsToFill = (NUM_RMT_SLOTS - 1) - NumUsedEntriesInSendBuffer;
    } // end while there is space in the buffer

    // //DEBUG_END;

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
inline bool IRAM_ATTR c_OutputRmt::ISR_GetNextIntensityToSend(uint32_t &DataToSend)
{
    if (nullptr != OutputRmtConfig.pPixelDataSource)
    {
        return OutputRmtConfig.pPixelDataSource->ISR_GetNextIntensityToSend(DataToSend);
    }
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    else
    {
        return OutputRmtConfig.pSerialDataSource->ISR_GetNextIntensityToSend(DataToSend);
    }
#else
    return false;
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
} // GetNextIntensityToSend

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler (uint32_t isrFlags)
{
    // //DEBUG_START;

    // uint32_t int_st = RMT.int_raw.val;
    // //DEBUG_V(String("              int_st: 0x") + String(int_st, HEX));
    // //DEBUG_V(String("  RMT_INT_TX_END_BIT: 0x") + String(RMT_INT_TX_END_BIT, HEX));
    // ClearRmtInterrupts;

    RMT_DEBUG_COUNTER(++ISRcounter);
    if(OutputIsPaused)
    {
        DisableRmtInterrupts;
    }
    // did the transmitter stall?
    else if (isrFlags & RMT_INT_TX_END_BIT )
    {
        RMT_DEBUG_COUNTER(++IntTxEndIsrCounter);
        DisableRmtInterrupts;

        if(NumUsedEntriesInSendBuffer)
        {
            RMT_DEBUG_COUNTER(++FailedToSendAllData);
        }

        // tell the background task to start the next output
        vTaskNotifyGiveFromISR( SendFrameTaskHandle, &xHigherPriorityTaskWoken );
        // portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
    else if (isrFlags & RMT_INT_THR_EVNT_BIT )
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
                DisableRmtInterrupts;

                // tell the background task to start the next output
                vTaskNotifyGiveFromISR( SendFrameTaskHandle, &xHigherPriorityTaskWoken );
                // portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
            }
        }
        // else ignore the interrupt and let the transmitter stall when it runs out of data
    }
#ifdef USE_RMT_DEBUG_COUNTERS
    else
    {
        RMT_DEBUG_COUNTER(++UnknownISRcounter);
        if (isrFlags & RMT_INT_ERROR_BIT)
        {
            ErrorIsr++;
        }

        if (isrFlags & RMT_INT_RX_END_BIT)
        {
            RxIsr++;
        }
    }
#endif // def USE_RMT_DEBUG_COUNTERS

    // //DEBUG_END;
} // ISR_Handler

//----------------------------------------------------------------------------
inline bool IRAM_ATTR c_OutputRmt::ISR_MoreDataToSend()
{
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    if (nullptr != OutputRmtConfig.pPixelDataSource)
    {
        return OutputRmtConfig.pPixelDataSource->ISR_MoreDataToSend();
    }
    else
    {
        return OutputRmtConfig.pSerialDataSource->ISR_MoreDataToSend();
    }
#else
    return OutputRmtConfig.pPixelDataSource->ISR_MoreDataToSend();
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
}

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_ResetRmtBlockPointers()
{
    RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 1; /* move the mempointers to the start of the block */ 
    RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 0; /* Allow access to the mem pointers */ 
    RmtBufferWriteIndex = 0;
    SendBufferWriteIndex = 0;
    SendBufferReadIndex  = 0;
    NumUsedEntriesInSendBuffer = 0;
}

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_StartNewDataFrame()
{
    if (nullptr != OutputRmtConfig.pPixelDataSource)
    {
        OutputRmtConfig.pPixelDataSource->StartNewFrame();
    }
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    else
    {
        OutputRmtConfig.pSerialDataSource->StartNewFrame();
    }
#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
} // StartNewDataFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_TransferIntensityDataToRMT (uint32_t MaxNumEntriesToTransfer)
{
    // //DEBUG_START;

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
        RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = SendBuffer[SendBufferReadIndex].val;
        RmtBufferWriteIndex = (++RmtBufferWriteIndex) & (NUM_RMT_SLOTS - 1);
        SendBufferReadIndex = (++SendBufferReadIndex) & (NUM_RMT_SLOTS - 1);
        --NumEntriesToTransfer;
        --NumUsedEntriesInSendBuffer;
    }

    // terminate the data stream
    RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = uint32_t(0);

    // //DEBUG_END;

} // ISR_Handler_TransferBufferToRMT

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_WriteToBuffer(uint32_t value)
{
    // //DEBUG_START;

    SendBuffer[SendBufferWriteIndex].val = value;
    SendBufferWriteIndex = (++SendBufferWriteIndex) & (NUM_RMT_SLOTS - 1);
    ++NumUsedEntriesInSendBuffer;

    // //DEBUG_END;
}

//----------------------------------------------------------------------------
void c_OutputRmt::PauseOutput(bool PauseOutput)
{
    // //DEBUG_START;

    if (OutputIsPaused == PauseOutput)
    {
        // //DEBUG_V("no change. Ignore the call");
    }
    else if (PauseOutput)
    {
        // //DEBUG_V("stop the output");
        DisableRmtInterrupts;
        ClearRmtInterrupts;
    }

    OutputIsPaused = PauseOutput;

    // //DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
bool c_OutputRmt::StartNewFrame ()
{
    // //DEBUG_START;

    bool Response = false;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Paused");
            DisableRmtInterrupts;
            ClearRmtInterrupts;
            break;
        }

        if(InterrupsAreEnabled)
        {
            RMT_DEBUG_COUNTER(IncompleteFrame++);
        }

        DisableRmtInterrupts;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;

        ISR_ResetRmtBlockPointers ();

        // //DEBUG_V(String("NumIdleBits: ") + String(OutputRmtConfig.NumIdleBits));
        uint32_t NumInterFrameRmtSlotsCount = 0;
        while (NumInterFrameRmtSlotsCount < OutputRmtConfig.NumIdleBits)
        {
            ISR_WriteToBuffer (Intensity2Rmt[RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID].val);
            ++NumInterFrameRmtSlotsCount;
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID)]++);
        }

        // //DEBUG_V(String("NumFrameStartBits: ") + String(OutputRmtConfig.NumFrameStartBits));
        uint32_t NumFrameStartRmtSlotsCount = 0;
        while (NumFrameStartRmtSlotsCount++ < OutputRmtConfig.NumFrameStartBits)
        {
            ISR_WriteToBuffer (Intensity2Rmt[RmtDataBitIdType_t::RMT_STARTBIT_ID].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_STARTBIT_ID)]++);
        }

#ifdef USE_RMT_DEBUG_COUNTERS
        FrameStartCounter++;
        IntensityValuesSentLastFrame = IntensityValuesSent;
        IntensityValuesSent          = 0;
        IntensityBitsSentLastFrame   = IntensityBitsSent;
        IntensityBitsSent            = 0;
#endif // def USE_RMT_DEBUG_COUNTERS

        // set up to send a new frame
        ISR_StartNewDataFrame ();
        // DEBUG_V();

        ThereIsDataToSend = ISR_MoreDataToSend();
        // DEBUG_V();

        // this fills the send buffer
        ISR_CreateIntensityData ();

        // Fill the RMT Hardware buffer
        ISR_TransferIntensityDataToRMT(NUM_RMT_SLOTS - 1);

        // this refills the send buffer
        ISR_CreateIntensityData ();

        // At this point we have 191 bits of data created and ready to send
        RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit = MaxNumRmtSlotsPerInterrupt;

        ClearRmtInterrupts;
        EnableRmtInterrupts;

        // DEBUG_V("start the transmitter");
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 1;

        Response = true;
    } while(false);

    // //DEBUG_END;
    return Response;

} // StartNewFrame

#endif // def ARDUINO_ARCH_ESP32
