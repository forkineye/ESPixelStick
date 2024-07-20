/*
* OutputRmt.cpp - driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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
#include "../ESPixelStick.h"
#ifdef ARDUINO_ARCH_ESP32
// #include <gpio.h>
#include "OutputRmt.hpp"

// forward declaration for the isr handler
static void IRAM_ATTR   rmt_intr_handler (void* param);
static rmt_isr_handle_t RMT_intr_handle = NULL;
static c_OutputRmt *    rmt_isr_ThisPtrs[8];
static bool             InIsr = false;

#ifdef USE_RMT_DEBUG_COUNTERS
static uint32_t RawIsrCounter = 0;
#endif // def USE_RMT_DEBUG_COUNTERS

//----------------------------------------------------------------------------
c_OutputRmt::c_OutputRmt()
{
    // DEBUG_START;

    memset((void *)&Intensity2Rmt[0],   0x00, sizeof(Intensity2Rmt));
    memset((void *)&SendBuffer[0], 0x00, sizeof(SendBuffer));

    if(NULL == RMT_intr_handle)
    {
        for(auto & currentThisPtr : rmt_isr_ThisPtrs)
        {
            currentThisPtr = nullptr;
        }
    }

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
#ifdef USE_RMT_DEBUG_COUNTERS
    RawIsrCounter++;
#endif // def USE_RMT_DEBUG_COUNTERS
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
void c_OutputRmt::Begin (OutputRmtConfig_t config )
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
        ESP_ERROR_CHECK(rmt_config(&RmtConfig));

        // DEBUG_V();
        // ESP_ERROR_CHECK(rmt_set_source_clk(RmtConfig.channel, rmt_source_clk_t::RMT_BASECLK_APB));
        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = this;

        if(NULL == RMT_intr_handle)
        {
            // DEBUG_V();
            // ESP_ERROR_CHECK (esp_intr_alloc (ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, rmt_intr_handler, this, &RMT_intr_handle));
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

        WaitFrameDone = xSemaphoreCreateBinary();

#ifdef RMT_USE_ISR_TASK
        // This is for debugging only
        xTaskCreate(SendRmtIntensityDataTask, "RMTTask", 4000, this, ESP_TASK_PRIO_MIN + 4, &SendIntensityDataTaskHandle);
#endif // def RMT_USE_ISR_TASK

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputRmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // //DEBUG_START;
    
    jsonStatus[F("NumRmtSlotOverruns")] = NumRmtSlotOverruns;
#ifdef USE_RMT_DEBUG_COUNTERS 
    jsonStatus[F("OutputIsPaused")] = OutputIsPaused;
    JsonObject debugStatus = jsonStatus["RMT Debug"].to<JsonObject>();
    debugStatus["RmtChannelId"]                 = OutputRmtConfig.RmtChannelId;
    debugStatus["GPIO"]                         = OutputRmtConfig.DataPin;
    debugStatus["conf0"]                        = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf0.val, HEX);
    debugStatus["conf1"]                        = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.val, HEX);
    debugStatus["tx_lim_ch"]                    = String(RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit);

    debugStatus["FrameStartCounter"]            = FrameStartCounter;
    debugStatus["RawIsrCounter"]                = RawIsrCounter;
    debugStatus["ISRcounter"]                   = ISRcounter;
    debugStatus["SendBlockIsrCounter"]          = SendBlockIsrCounter;
    debugStatus["FrameEndISRcounter"]           = FrameEndISRcounter;
    debugStatus["UnknownISRcounter"]            = UnknownISRcounter;
    debugStatus["RanOutOfData"]                 = RanOutOfData;
    debugStatus["IntTxEndIsrCounter"]           = IntTxEndIsrCounter;
    debugStatus["IntTxThrIsrCounter"]           = IntTxThrIsrCounter;
    debugStatus["ErrorIsr"]                     = ErrorIsr;
    debugStatus["RxIsr"]                        = RxIsr;
    debugStatus["IncompleteFrame"]              = IncompleteFrame;
    debugStatus["RMT_INT_TX_END_BIT"]           = "0x" + String (RMT_INT_TX_END_BIT, HEX);
    debugStatus["RMT_INT_THR_EVNT_BIT"]         = "0x" + String (RMT_INT_THR_EVNT_BIT, HEX);
    uint32_t temp = RMT.int_ena.val;
    debugStatus["Raw int_ena"]                  = "0x" + String (temp, HEX);
    debugStatus["TX END int_ena"]               = "0x" + String (temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX THRSH int_ena"]             = "0x" + String (temp & (RMT_INT_THR_EVNT_BIT), HEX);
    temp = RMT.int_st.val;
    debugStatus["Raw int_st"]                   = "0x" + String (temp, HEX);
    debugStatus["TX END int_st"]                = "0x" + String (temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX THRSH int_st"]              = "0x" + String (temp & (RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["IntensityValuesSent"]          = IntensityValuesSent;
    debugStatus["IntensityValuesSentLastFrame"] = IntensityValuesSentLastFrame;
    debugStatus["IntensityBitsSent"]            = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"]   = IntensityBitsSentLastFrame;
    debugStatus["NumIdleBits"]                  = OutputRmtConfig.NumIdleBits;
    debugStatus["NumFrameStartBits"]            = OutputRmtConfig.NumFrameStartBits;
    debugStatus["NumFrameStopBits"]             = OutputRmtConfig.NumFrameStopBits;
    debugStatus["SendInterIntensityBits"]       = OutputRmtConfig.SendInterIntensityBits;
    debugStatus["SendEndOfFrameBits"]           = OutputRmtConfig.SendEndOfFrameBits;
    debugStatus["NumRmtSlotsPerIntensityValue"] = NumRmtSlotsPerIntensityValue;
    debugStatus["RmtEntriesTransfered"]         = RmtEntriesTransfered;
    debugStatus["RmtXmtFills"]                  = RmtXmtFills;
    debugStatus["ZeroBitValue"]                 = String (Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val, HEX);
    debugStatus["OneBitValue"]                  = String (Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val,  HEX);

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
    // //DEBUG_END;
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
#ifdef USE_RMT_DEBUG_COUNTERS
        IntensityValuesSent++;
#endif // def USE_RMT_DEBUG_COUNTERS

        // convert the intensity data into RMT slot data
        uint32_t bitmask = TxIntensityDataStartingMask;
        for (uint32_t BitCount = OutputRmtConfig.IntensityDataWidth; 0 < BitCount; --BitCount)
        {
#ifdef USE_RMT_DEBUG_COUNTERS
            IntensityBitsSent++;
#endif // def USE_RMT_DEBUG_COUNTERS
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
#ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_END_OF_FRAME)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
        }
        else if (OutputRmtConfig.SendInterIntensityBits)
        {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_STOP_START_BIT_ID].val);
#ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_STOP_START_BIT_ID)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
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

#ifdef USE_RMT_DEBUG_COUNTERS
    ++ISRcounter;

    if(isrFlags & RMT_INT_TX_END_BIT)
    {
        ++IntTxEndIsrCounter;
    }

    if(isrFlags & RMT_INT_THR_EVNT_BIT )
    {
        ++IntTxThrIsrCounter;
    }

    if (isrFlags & RMT_INT_ERROR_BIT)
    {
        ErrorIsr++;
    }

    if (isrFlags & RMT_INT_RX_END_BIT)
    {
        RxIsr++;
    }
#endif // def USE_RMT_DEBUG_COUNTERS

    if (isrFlags & RMT_ISR_BITS)
    {
        if(NumUsedEntriesInSendBuffer)
        {
#ifdef USE_RMT_DEBUG_COUNTERS
            ++SendBlockIsrCounter;
#endif // def USE_RMT_DEBUG_COUNTERS

            // transfer any prefetched data to the hardware transmitter
            ISR_TransferIntensityDataToRMT();

            // refill the buffer
            ISR_CreateIntensityData();

            // is there any data left to enqueue?
            if (!ThereIsDataToSend && 0 == NumUsedEntriesInSendBuffer)
            {
#ifdef USE_RMT_DEBUG_COUNTERS
                ++RanOutOfData;
#endif // def USE_RMT_DEBUG_COUNTERS
                // DisableRmtInterrupts;

                // terminate the data stream
                RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = 0x0;
                // xSemaphoreGive(WaitFrameDone);
            }
        }
        else
        {
            DisableRmtInterrupts;
            xSemaphoreGive(WaitFrameDone);
#ifdef USE_RMT_DEBUG_COUNTERS
            FrameEndISRcounter++;
#endif // def USE_RMT_DEBUG_COUNTERS
        }
    }
#ifdef USE_RMT_DEBUG_COUNTERS
    else
    {
        UnknownISRcounter++;
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
void IRAM_ATTR c_OutputRmt::ISR_TransferIntensityDataToRMT ()
{
    // //DEBUG_START;

    uint32_t NumEntriesToTransfer = min((NUM_RMT_SLOTS/2), NumUsedEntriesInSendBuffer);

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
}

//----------------------------------------------------------------------------
bool c_OutputRmt::StartNewFrame (uint32_t FrameDurationInMicroSec)
{
    // //DEBUG_START;

    bool Response = false;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Paused");
            break;
        }

#ifdef USE_RMT_DEBUG_COUNTERS
        if(InterrupsAreEnabled)
        {
            IncompleteFrame++;
        }
#endif // def USE_RMT_DEBUG_COUNTERS

        DisableRmtInterrupts;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;

        // rmt_tx_memory_reset(OutputRmtConfig.RmtChannelId);
        ISR_ResetRmtBlockPointers ();

        // //DEBUG_V(String("NumIdleBits: ") + String(OutputRmtConfig.NumIdleBits));
        uint32_t NumInterFrameRmtSlotsCount = 0;
        while (NumInterFrameRmtSlotsCount < OutputRmtConfig.NumIdleBits)
        {
            ISR_WriteToBuffer (Intensity2Rmt[RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID].val);
            ++NumInterFrameRmtSlotsCount;
    #ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID)]++;
    #endif // def USE_RMT_DEBUG_COUNTERS
        }

        // //DEBUG_V(String("NumFrameStartBits: ") + String(OutputRmtConfig.NumFrameStartBits));
        uint32_t NumFrameStartRmtSlotsCount = 0;
        while (NumFrameStartRmtSlotsCount++ < OutputRmtConfig.NumFrameStartBits)
        {
            ISR_WriteToBuffer (Intensity2Rmt[RmtDataBitIdType_t::RMT_STARTBIT_ID].val);
    #ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_STARTBIT_ID)]++;
    #endif // def USE_RMT_DEBUG_COUNTERS
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

        // transfer 1st 32 entries of buffer data to the RMT hardware
        ISR_TransferIntensityDataToRMT();

        // this refills the send buffer
        ISR_CreateIntensityData ();

        // transfer 2nd 32 entries of buffer data to the RMT hardware
        ISR_TransferIntensityDataToRMT();

        // top off the send buffer
        ISR_CreateIntensityData ();

        // At this point we have 128 bits of data created and ready to send
        RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit = MaxNumRmtSlotsPerInterrupt;

        ClearRmtInterrupts;
        EnableRmtInterrupts;

        // DEBUG_V("start the transmitter");
        rmt_set_gpio(OutputRmtConfig.RmtChannelId, RMT_MODE_TX, OutputRmtConfig.DataPin, false);
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 1;

        // wait for the ISR to finish
        xSemaphoreTake(WaitFrameDone, pdMS_TO_TICKS((FrameDurationInMicroSec+2000)/1000));
        Response = true;
    } while(false);

    // //DEBUG_END;
    return Response;

} // StartNewFrame

#endif // def ARDUINO_ARCH_ESP32
