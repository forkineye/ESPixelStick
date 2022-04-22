/*
* OutputRmt.cpp - TM1814 driver code for ESPixelStick RMT Channel
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
#ifdef SUPPORT_RMT_OUTPUT

#include "OutputRmt.hpp"

// forward declaration for the isr handler
static void IRAM_ATTR rmt_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputRmt::c_OutputRmt ()
{
    // DEBUG_START;

    memset((void *)&Intensity2Rmt[0],   0x00, sizeof(Intensity2Rmt));
#ifdef USE_RMT_DEBUG_COUNTERS
    memset((void *)&BitTypeCounters[0], 0x00, sizeof(BitTypeCounters));
#endif // def USE_RMT_DEBUG_COUNTERS

    // DEBUG_END;
} // c_OutputRmt

//----------------------------------------------------------------------------
c_OutputRmt::~c_OutputRmt ()
{
    // DEBUG_START;

    // Disable all interrupts for this RMT Channel.
    // DEBUG_V ("");

    // Clear all pending interrupts in the RMT Channel
    // DEBUG_V ("");
    if (HasBeenInitialized)
    {
        DisableInterrupts;
        rmt_tx_stop (OutputRmtConfig.RmtChannelId);

        rmt_isr_deregister (RMT_intr_handle);

        EnableInterrupts;
    }

    // DEBUG_END;
} // ~c_OutputRmt

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR rmt_intr_handler (void* param)
{
    if (param)
    {
        reinterpret_cast<c_OutputRmt *>(param)->ISR_Handler();
    }

} // rmt_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
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
            reboot = true;
            break;
        }

        SetIntensityDataWidth(8);

        // DEBUG_V (String ("DataPin: ") + String (DataPin));
        // DEBUG_V (String (" RmtChannelId: ") + String (OutputRmtConfig.RmtChannelId));

        // Configure RMT channel
        rmt_config_t RmtConfig;
        RmtConfig.rmt_mode = rmt_mode_t::RMT_MODE_TX;
        RmtConfig.channel = OutputRmtConfig.RmtChannelId;
        RmtConfig.clk_div = RMT_Clock_Divisor;
        RmtConfig.gpio_num = OutputRmtConfig.DataPin;
        RmtConfig.mem_block_num = rmt_reserve_memsize_t::RMT_MEM_64;

        RmtConfig.tx_config.loop_en = false;
        RmtConfig.tx_config.carrier_freq_hz = uint32_t(100); // cannot be zero due to a driver bug
        RmtConfig.tx_config.carrier_duty_percent = 50;
        RmtConfig.tx_config.carrier_level = rmt_carrier_level_t::RMT_CARRIER_LEVEL_LOW;
        RmtConfig.tx_config.carrier_en = false;
        RmtConfig.tx_config.idle_level = OutputRmtConfig.idle_level;
        RmtConfig.tx_config.idle_output_en = true;

        ESP_ERROR_CHECK(rmt_config(&RmtConfig));
        ESP_ERROR_CHECK(rmt_set_source_clk(RmtConfig.channel, rmt_source_clk_t::RMT_BASECLK_APB));
        // ESP_ERROR_CHECK (esp_intr_alloc (ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, rmt_intr_handler, this, &RMT_intr_handle));
        ESP_ERROR_CHECK(rmt_isr_register(rmt_intr_handler, this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED, &RMT_intr_handle));

        RMT.apb_conf.fifo_mask = 1;      // enable access to the mem blocks
        RMT.apb_conf.mem_tx_wrap_en = 1; // allow data greater than one block

        // reset the internal and external pointers to the start of the mem block
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 1;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 0;

        RmtStartAddr = &RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0];
        RmtEndAddr = &RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[63];
        RmtCurrentAddr = RmtStartAddr;

        RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit = NumRmtSlotsPerInterrupt;
        NumAvailableRmtSlotsToFill = NUM_RMT_SLOTS;

        // create a delay before starting to send data
        LastFrameStartTime = millis();

        // DEBUG_V (String ("                Intensity2Rmt[0]: 0x") + String (uint32_t (Intensity2Rmt[0].val), HEX));
        // DEBUG_V (String ("                Intensity2Rmt[1]: 0x") + String (uint32_t (Intensity2Rmt[1].val), HEX));
        // DEBUG_V (String ("Intensity2Rmt[INTERFRAME_GAP_ID]: 0x") + String (uint32_t (Intensity2Rmt[INTERFRAME_GAP_ID].val), HEX));
        // DEBUG_V (String ("      Intensity2Rmt[STARTBIT_ID]: 0x") + String (uint32_t (Intensity2Rmt[STARTBIT_ID].val), HEX));
        // DEBUG_V (String ("       Intensity2Rmt[STOPBIT_ID]: 0x") + String (uint32_t (Intensity2Rmt[STOPBIT_ID].val), HEX));

#ifdef RMT_USR_ISR_TASK
        xTaskCreate(SendRmtIntensityDataTask, "RMTTask", 4000, this, ESP_TASK_PRIO_MIN + 4, &SendIntensityDataTaskHandle);
#endif // def RMT_USR_ISR_TASK

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler ()
{
    // DEBUG_START;

    uint32_t int_st = RMT.int_raw.val;
    // DEBUG_V(String("              int_st: 0x") + String(int_st, HEX));
    // DEBUG_V(String("  RMT_INT_TX_END_BIT: 0x") + String(RMT_INT_TX_END_BIT, HEX));
    // DEBUG_V(String("RMT_INT_THR_EVNT_BIT: 0x") + String(RMT_INT_THR_EVNT_BIT, HEX));

    do // once
    {
        if (int_st & RMT_INT_TX_END_BIT)
        {
#ifdef USE_RMT_DEBUG_COUNTERS
            FrameEndISRcounter++;
#endif // def USE_RMT_DEBUG_COUNTERS

            RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;

            DisableInterrupts;

            RMT.int_clr.val = RMT_INT_TX_END_BIT;
            RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;

            // ISR_Handler_StartNewFrame ();
            break;
        }

        if (int_st & RMT_INT_THR_EVNT_BIT)
        {
            // DEBUG_V("RMT_INT_THR_EVNT_BIT");

#ifdef USE_RMT_DEBUG_COUNTERS
            DataISRcounter++;
#endif // def USE_RMT_DEBUG_COUNTERS

            // RMT.int_ena.val &= ~RMT_INT_THR_EVNT (OutputRmtConfig.RmtChannelId);
            RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;

            NumAvailableRmtSlotsToFill += NumRmtSlotsPerInterrupt;
            if (NUM_RMT_SLOTS < NumAvailableRmtSlotsToFill)
            {
                NumAvailableRmtSlotsToFill = NUM_RMT_SLOTS;
            }

            if (MoreDataToSend())
            {
                EnableInterrupts;
                ISR_Handler_SendIntensityData();
            }
            else
            {
#ifdef USE_RMT_DEBUG_COUNTERS
                FrameThresholdCounter++;
#endif // def USE_RMT_DEBUG_COUNTERS
                RMT.int_ena.val &= ~RMT_INT_THR_EVNT_BIT;
            }
            break;
        }

#ifdef USE_RMT_DEBUG_COUNTERS
        if (int_st & RMT_INT_ERROR_BIT)
        {
            // DEBUG_V("RMT_INT_ERROR_BIT");
            ErrorIsr++;
        }

        if (int_st & RMT_INT_RX_END_BIT)
        {
            // DEBUG_V("RMT_INT_RX_END_BIT");
            RxIsr++;
        }
#endif // def USE_RMT_DEBUG_COUNTERS
    } while (false);

    // DEBUG_END;

} // ISR_Handler

//----------------------------------------------------------------------------
void c_OutputRmt::PauseOutput(bool PauseOutput)
{
    // DEBUG_START;

    if (OutputIsPaused == PauseOutput)
    {
        // DEBUG_V("no change. Ignore the call");
    }
    else if (PauseOutput)
    {
        // DEBUG_V("stop the output");
        DisableInterrupts;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;

        RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;
        RMT.int_clr.val = RMT_INT_TX_END_BIT;
    }

    OutputIsPaused = PauseOutput;

    // DEBUG_END;
}

//----------------------------------------------------------------------------
inline void IRAM_ATTR c_OutputRmt::ISR_EnqueueData(uint32_t value)
{
    // DEBUG_START;

    // write the data
    RmtCurrentAddr->val = value;

    --NumAvailableRmtSlotsToFill;

    // increment address, check for address wrap around
    if (++RmtCurrentAddr > RmtEndAddr)
    {
        RmtCurrentAddr = RmtStartAddr;
    }

    // DEBUG_END;
}

//----------------------------------------------------------------------------
void c_OutputRmt::StartNewFrame ()
{
    // DEBUG_START;

#ifdef USE_RMT_DEBUG_COUNTERS
    FrameStartCounter++;
#endif // def USE_RMT_DEBUG_COUNTERS

    // Need to build up a backlog of entries in the buffer
    // so that there is still plenty of data to send when the isr fires.
    // DEBUG_V(String("NumInterFrameRmtSlots: ") + String(NumInterFrameRmtSlots));
    size_t NumInterFrameRmtSlotsCount = 0;
    while (NumInterFrameRmtSlotsCount < NumInterFrameRmtSlots)
    {
        ISR_EnqueueData (Intensity2Rmt[RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID].val);
        ++NumInterFrameRmtSlotsCount;
#ifdef USE_RMT_DEBUG_COUNTERS
        BitTypeCounters[int(RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
    }

    // DEBUG_V(String("NumFrameStartRmtSlots: ") + String(NumFrameStartRmtSlots));
    size_t NumFrameStartRmtSlotsCount = 0;
    while (NumFrameStartRmtSlotsCount++ < NumFrameStartRmtSlots)
    {
        ISR_EnqueueData (Intensity2Rmt[RmtDataBitIdType_t::RMT_STARTBIT_ID].val);
#ifdef USE_RMT_DEBUG_COUNTERS
        BitTypeCounters[int(RmtDataBitIdType_t::RMT_STARTBIT_ID)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
    }

#ifdef USE_RMT_DEBUG_COUNTERS
    IntensityValuesSentLastFrame = IntensityValuesSent;
    IntensityValuesSent          = 0;
    IntensityBitsSentLastFrame   = IntensityBitsSent;
    IntensityBitsSent            = 0;
#endif // def USE_RMT_DEBUG_COUNTERS

    // set up to send a new frame
    StartNewDataFrame ();
    // DEBUG_V();

    ISR_Handler_SendIntensityData ();
    // DEBUG_V();
/*
    // dump the intensity data we just wrote
    for (uint8_t bitIndex = 0; NUM_RMT_SLOTS > bitIndex; bitIndex++)
    {
        // DEBUG_V(String("Data at '" + String(bitIndex) + "': 0x") + String(((uint32_t *)RmtStartAddr)[bitIndex], HEX));
    }
*/
    // DEBUG_END;

} // ISR_Handler_StartNewFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler_SendIntensityData ()
{
    // DEBUG_START;

    uint32_t OneBitValue  = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val;
    uint32_t ZeroBitValue = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val;

    while ((NumAvailableRmtSlotsToFill > NumRmtSlotsPerIntensityValue) && MoreDataToSend())
    {
        uint32_t IntensityValue = GetNextIntensityToSend() * IntensityMapMultiplier;
//        uint32_t IntensityValue = map(GetNextIntensityToSend(), 0, 255, 0, IntensityMapDstMax);
#ifdef USE_RMT_DEBUG_COUNTERS
        IntensityValuesSent++;
#endif // def USE_RMT_DEBUG_COUNTERS

        // convert the intensity data into RMT slot data
        for (uint32_t bitmask = TxIntensityDataStartingMask; 0 != bitmask; bitmask >>= 1)
        {
#ifdef USE_RMT_DEBUG_COUNTERS
            IntensityBitsSent++;
#endif // def USE_RMT_DEBUG_COUNTERS
            ISR_EnqueueData((IntensityValue & bitmask) ? OneBitValue : ZeroBitValue);
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

        if (SendEndOfFrameBits && !MoreDataToSend())
        {
            ISR_EnqueueData(Intensity2Rmt[RmtDataBitIdType_t::RMT_END_OF_FRAME].val);
#ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_END_OF_FRAME)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
        }
        else if (SendInterIntensityBits)
        {
            ISR_EnqueueData(Intensity2Rmt[RmtDataBitIdType_t::RMT_STOP_START_BIT_ID].val);
#ifdef USE_RMT_DEBUG_COUNTERS
            BitTypeCounters[int(RmtDataBitIdType_t::RMT_STOP_START_BIT_ID)]++;
#endif // def USE_RMT_DEBUG_COUNTERS
        }
    } // end while there is space in the buffer

    // terminate the current data in the buffer
    // gets overwritten on next refill
    RmtCurrentAddr->val = 0;

    // DEBUG_END;

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
bool c_OutputRmt::Render ()
{
    // DEBUG_START;
    bool Response = false;

    do // once
    {
        if(OutputIsPaused)
        {
            break;
        }

        if (!NoFrameInProgress ())
        {
            break;
        }

        if ((millis() - LastFrameStartTime) < MIN_FRAME_TIME_MS)
        {
            break;
        }

#ifdef USE_RMT_DEBUG_COUNTERS
        if (MoreDataToSend())
        {
            // DEBUG_V ("ERROR: Starting a new frame before the previous frame was completly sent");
            IncompleteFrame++;
        }
#endif // def USE_RMT_DEBUG_COUNTERS

        // DEBUG_V("Stop old Frame");
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;
        DisableInterrupts;
        RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;
        RMT.int_clr.val = RMT_INT_TX_END_BIT;

        // reset the internal and external pointers to the start of the mem block
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 1;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 0;
        RmtCurrentAddr = RmtStartAddr;
        NumAvailableRmtSlotsToFill = NUM_RMT_SLOTS;
        RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit = NumRmtSlotsPerInterrupt;

        // DEBUG_V("Set up a new Frame");
        StartNewFrame();

        // DEBUG_V("Start Transmit");
        // enable the threshold event interrupt
        EnableInterrupts;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 1;
        LastFrameStartTime = millis ();
        // DEBUG_V("Transmit Started");
        Response = true;

    } while (false);

    // DEBUG_END;

    return Response;

} // render

//----------------------------------------------------------------------------
void c_OutputRmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // DEBUG_START;

    jsonStatus[F("NumRmtSlotOverruns")] = NumRmtSlotOverruns;
#ifdef USE_RMT_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus.createNestedObject("RMT Debug");
    debugStatus["RmtChannelId"]                 = OutputRmtConfig.RmtChannelId;
    debugStatus["DataISRcounter"]               = DataISRcounter;
    debugStatus["FrameEndISRcounter"]           = FrameEndISRcounter;
    debugStatus["FrameStartCounter"]            = FrameStartCounter;
    debugStatus["ErrorIsr"]                     = ErrorIsr;
    debugStatus["RxIsr"]                        = RxIsr;
    debugStatus["FrameThresholdCounter"]        = FrameThresholdCounter;
    debugStatus["IsrIsNotForUs"]                = IsrIsNotForUs;
    debugStatus["IncompleteFrame"]              = IncompleteFrame;
    debugStatus["Raw int_ena"]                  = String (RMT.int_ena.val, HEX);
    debugStatus["int_ena"]                      = String (RMT.int_ena.val & (RMT_INT_TX_END_BIT | RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["RMT_INT_TX_END_BIT"]           = String (RMT_INT_TX_END_BIT, HEX);
    debugStatus["RMT_INT_THR_EVNT_BIT"]         = String (RMT_INT_THR_EVNT_BIT, HEX);
    debugStatus["Raw int_st"]                   = String (RMT.int_st.val, HEX);
    debugStatus["int_st"]                       = String (RMT.int_st.val & (RMT_INT_TX_END_BIT | RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["IntensityValuesSent"]           = IntensityValuesSent;
    debugStatus["IntensityValuesSentLastFrame"]  = IntensityValuesSentLastFrame;
    debugStatus["IntensityBitsSent"]            = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"]   = IntensityBitsSentLastFrame;
    debugStatus["NumInterFrameRmtSlots"]        = NumInterFrameRmtSlots;
    debugStatus["NumFrameStartRmtSlots"]        = NumFrameStartRmtSlots;
    debugStatus["NumFrameStopRmtSlots"]         = NumFrameStopRmtSlots;
    debugStatus["SendInterIntensityBits"]       = SendInterIntensityBits;
    debugStatus["SendEndOfFrameBits"]           = SendEndOfFrameBits;
    debugStatus["NumRmtSlotsPerIntensityValue"] = NumRmtSlotsPerIntensityValue;

    uint32_t index = 0;
    for (auto CurrentCounter : BitTypeCounters)
    {
        debugStatus[String("RMT TYPE Counter ") + String(index++)] = CurrentCounter;
    }

#endif // def USE_RMT_DEBUG_COUNTERS
    // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputRmt::SetIntensityDataWidth(uint32_t DataWidth)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V(String("                     DataWidth: ") + String(DataWidth));

        if ((0 == DataWidth) || (DataWidth >= (sizeof(TxIntensityDataStartingMask)-1) * 8))
        {
            // DEBUG_V(String(F("Invalid DataWidth: ")) + String(DataWidth));
            break;
        }
       
        noInterrupts();
        NumRmtSlotsPerIntensityValue = DataWidth + ((SendInterIntensityBits) ? 1 : 0 );
        uint32_t IntensityMapDstMax = (1 << DataWidth) - 1;
        IntensityMapMultiplier = IntensityMapDstMax / 255;
        TxIntensityDataStartingMask = 1 << (DataWidth - 1);
        interrupts();

        // DEBUG_V(String("NumRmtSlotsPerIntensityValue: ")   + String(NumRmtSlotsPerIntensityValue));
        // DEBUG_V(String("          IntensityMapDstMax: ")   + String(IntensityMapDstMax));
        // DEBUG_V(String("      IntensityMapMultiplier: ")   + String(IntensityMapMultiplier));
        // DEBUG_V(String("     NumRmtSlotsPerInterrupt: 0x") + String(NumRmtSlotsPerInterrupt, HEX));

    } while (false);

    // DEBUG_END;

} // SetIntensityDataWidth

#endif // def SUPPORT_RMT_OUTPUT
