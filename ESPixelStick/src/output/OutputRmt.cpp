/*
* OutputRmt.cpp - TM1814 driver code for ESPixelStick RMT Channel
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
#ifdef ARDUINO_ARCH_ESP32

#include "OutputRmt.hpp"

#define NumBitsPerByte                            8
#define MAX_NUM_INTENSITY_BIT_SLOTS_PER_INTERRUPT (sizeof (RMTMEM.chan[0].data32) / sizeof (rmt_item32_t))
#define NUM_FRAME_START_SLOTS                     6

// forward declaration for the isr handler
static void IRAM_ATTR rmt_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputRmt::c_OutputRmt ()
{
    // DEBUG_START;

    memset ( (void*)&Rgb2Rmt[0], 0x00, sizeof (Rgb2Rmt));

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
    RMT.int_ena.val &= ~RMT_INT_TX_END_BIT;
    RMT.int_ena.val &= ~RMT_INT_THR_EVNT_BIT;
    rmt_tx_stop (RmtChannelId);

    esp_intr_free (RMT_intr_handle);

    // make sure no existing low level driver is running
    // DEBUG_V ("");

    // DEBUG_V (String ("RmtChannelId: ") + String (RmtChannelId));
    // rmtEnd (RmtObject);
    // DEBUG_END;
} // ~c_OutputRmt

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR rmt_intr_handler (void* param)
{
    reinterpret_cast <c_OutputRmt*> (param)->ISR_Handler ();
} // rmt_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputRmt::Begin (rmt_channel_t ChannelId, 
                         gpio_num_t _DataPin, 
                         c_OutputPixel * _OutputPixel,
                         rmt_idle_level_t idle_level )
{
    // DEBUG_START;

    DataPin = _DataPin;
    RmtChannelId = ChannelId;
    OutputPixel = _OutputPixel;

    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    // DEBUG_V (String (" RmtChannelId: ") + String (RmtChannelId));

    // Configure RMT channel
    rmt_config_t RmtConfig;
    RmtConfig.rmt_mode = rmt_mode_t::RMT_MODE_TX;
    RmtConfig.channel = RmtChannelId;
    RmtConfig.clk_div = RMT_Clock_Divisor;
    RmtConfig.gpio_num = DataPin;
    RmtConfig.mem_block_num = rmt_reserve_memsize_t::RMT_MEM_64;

    RmtConfig.tx_config.loop_en = false;
    RmtConfig.tx_config.carrier_freq_hz = uint32_t (100); // cannot be zero due to a driver bug
    RmtConfig.tx_config.carrier_duty_percent = 50;
    RmtConfig.tx_config.carrier_level = rmt_carrier_level_t::RMT_CARRIER_LEVEL_LOW;
    RmtConfig.tx_config.carrier_en = false;
    RmtConfig.tx_config.idle_level = idle_level;
    RmtConfig.tx_config.idle_output_en = true;

    RmtStartAddr = &RMTMEM.chan[RmtChannelId].data32[0];
    RmtEndAddr   = &RMTMEM.chan[RmtChannelId].data32[63];

    // the math here results in a modulo 8 of the maximum number of slots to fill at frame start time.
    NumIntensityValuesPerInterrupt = ( (MAX_NUM_INTENSITY_BIT_SLOTS_PER_INTERRUPT / NumBitsPerByte) / 2);
    NumIntensityBitsPerInterrupt = NumIntensityValuesPerInterrupt * NumBitsPerByte;

    // DEBUG_V (String ("NumIntensityValuesPerInterrupt: ") + String (NumIntensityValuesPerInterrupt));
    // DEBUG_V (String ("  NumIntensityBitsPerInterrupt: ") + String (NumIntensityBitsPerInterrupt));

    ESP_ERROR_CHECK (rmt_config (&RmtConfig));
    ESP_ERROR_CHECK (rmt_set_source_clk (RmtConfig.channel, rmt_source_clk_t::RMT_BASECLK_APB));
    ESP_ERROR_CHECK (esp_intr_alloc (ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, rmt_intr_handler, this, &RMT_intr_handle));

    RMT.apb_conf.fifo_mask = 1;         // enable access to the mem blocks
    RMT.apb_conf.mem_tx_wrap_en = 1;    // allow data greater than one block

    // DEBUG_V (String ("                Rgb2Rmt[0]: 0x") + String (uint32_t (Rgb2Rmt[0].val), HEX));
    // DEBUG_V (String ("                Rgb2Rmt[1]: 0x") + String (uint32_t (Rgb2Rmt[1].val), HEX));
    // DEBUG_V (String ("Rgb2Rmt[INTERFRAME_GAP_ID]: 0x") + String (uint32_t (Rgb2Rmt[INTERFRAME_GAP_ID].val), HEX));
    // DEBUG_V (String ("      Rgb2Rmt[STARTBIT_ID]: 0x") + String (uint32_t (Rgb2Rmt[STARTBIT_ID].val), HEX));
    // DEBUG_V (String ("       Rgb2Rmt[STOPBIT_ID]: 0x") + String (uint32_t (Rgb2Rmt[STOPBIT_ID].val), HEX));

    // Start output
    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler ()
{
    register uint32_t int_st = RMT.int_st.val;

    do // once
    {
        if (!(int_st & (RMT_INT_TX_END_BIT | RMT_INT_RX_END | RMT_INT_ERROR | RMT_INT_THR_EVNT)))
        {
            IsrIsNotForUs++;
            break;
        }

        if (RMT.int_st.val & RMT_INT_THR_EVNT_BIT)
        {
            DataISRcounter++;

            // RMT.int_ena.val &= ~RMT_INT_THR_EVNT (RmtChannelId);
            RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;

            if (OutputPixel->MoreDataToSend ())
            {
                ISR_Handler_SendIntensityData ();
            }
            else
            {
                RMT.int_ena.val &= ~RMT_INT_THR_EVNT_BIT;
            }
            break;
        }

        if (int_st & RMT_INT_TX_END_BIT)
        {
            FrameEndISRcounter++;

            RMT.int_clr.val = RMT_INT_TX_END_BIT;
            RMT.int_clr.val = RMT_INT_THR_EVNT_BIT;

            RMT.int_ena.val &= ~RMT_INT_TX_END_BIT;
            RMT.int_ena.val &= ~RMT_INT_THR_EVNT_BIT;

            // ISR_Handler_StartNewFrame ();
            break;
        }

        if (int_st & RMT_INT_ERROR_BIT)
        {
            ErrorIsr++;
        }

        if (int_st & RMT_INT_RX_END_BIT)
        {
            RxIsr++;
        }

    } while (false);


} // ISR_Handler

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler_StartNewFrame ()
{
    FrameStartCounter++;

    RMT.conf_ch[RmtChannelId].conf1.mem_rd_rst = 1; // set the internal pointer to the start of the mem block
    RMT.conf_ch[RmtChannelId].conf1.mem_rd_rst = 0;

    uint32_t* pMem = (uint32_t*)RmtStartAddr;

    // Need to build up a backlog of entries in the buffer
    // so that there is still plenty of data to send when the isr fires.
    // This is reflected in the constant: NUM_FRAME_START_SLOTS
    NumIdleBitsCount = 0;
    while (NumIdleBitsCount++ < NumIdleBits)
    {
        *pMem++ = Rgb2Rmt[RmtFrameType_t::RMT_INTERFRAME_GAP_ID].val;
    }

    NumStartBitsCount = 0;
    if (NumStartBits++ < NumStartBits)
    {
        *pMem++ = Rgb2Rmt[RmtFrameType_t::RMT_STARTBIT_ID].val;       // Start bit
    }
    RmtCurrentAddr = (volatile rmt_item32_t*)pMem;

    RMT.int_clr.val  = RMT_INT_THR_EVNT_BIT;
    RMT.int_ena.val |= RMT_INT_THR_EVNT_BIT;

    OutputPixel->StartNewFrame ();
    uint8_t SavedNumIntensityValuesPerInterrupt = NumIntensityValuesPerInterrupt;
    NumIntensityValuesPerInterrupt = ( (MAX_NUM_INTENSITY_BIT_SLOTS_PER_INTERRUPT / NumBitsPerByte) - 1);
    ISR_Handler_SendIntensityData ();
    NumIntensityValuesPerInterrupt = SavedNumIntensityValuesPerInterrupt;

    RMT.tx_lim_ch[RmtChannelId].limit = NumIntensityBitsPerInterrupt;

    RMT.int_clr.val  = RMT_INT_TX_END_BIT;
    RMT.int_ena.val |= RMT_INT_TX_END_BIT;
    RMT.conf_ch[RmtChannelId].conf1.tx_start = 1;

} // ISR_Handler_StartNewFrame

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputRmt::ISR_Handler_SendIntensityData ()
{
    uint32_t* pMem = (uint32_t*)RmtCurrentAddr;
    register uint32_t OneValue  = Rgb2Rmt[RmtFrameType_t::RMT_DATA_BIT_ONE_ID].val;
    register uint32_t ZeroValue = Rgb2Rmt[RmtFrameType_t::RMT_DATA_BIT_ZERO_ID].val;
    uint32_t NumEmptyIntensitySlots = NumIntensityValuesPerInterrupt;

    while ( (NumEmptyIntensitySlots--) && (OutputPixel->MoreDataToSend ()))
    {
        uint8_t IntensityValue = OutputPixel->GetNextIntensityToSend ();

        // convert the intensity data into RMT data
        for (uint8_t bitmask = 0x80; 0 != bitmask; bitmask >>= 1)
        {
            *pMem++ = (IntensityValue & bitmask) ? OneValue : ZeroValue;
            if (pMem > (uint32_t*)RmtEndAddr)
            {
                pMem = (uint32_t*)RmtStartAddr;
            }
        }
    } // end while there is space in the buffer

    // terminate the current data in the buffer
    *pMem = Rgb2Rmt[RmtFrameType_t::RMT_STOPBIT_ID].val;

    RmtCurrentAddr = (volatile rmt_item32_t*)pMem;

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
bool c_OutputRmt::Render ()
{
    bool Response = false;
    // DEBUG_START;

    if (NoFrameInProgress())
    {
        ISR_Handler_StartNewFrame ();
        Response = true;
    }

    // DEBUG_END;

    return Response;

} // render

//----------------------------------------------------------------------------
void c_OutputRmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    jsonStatus["    DataISRcounter: "] = DataISRcounter;
    jsonStatus["FrameEndISRcounter: "] = FrameEndISRcounter;
    jsonStatus[" FrameStartCounter: "] = FrameStartCounter;
    jsonStatus["          ErrorIsr: "] = ErrorIsr;
    jsonStatus["             RxIsr: "] = RxIsr;
    jsonStatus["     IsrIsNotForUs: "] = IsrIsNotForUs;

    jsonStatus["       Raw int_ena: 0x"] = String (RMT.int_ena.val, HEX);
    jsonStatus["           int_ena: 0x"] = String (RMT.int_ena.val & (RMT_INT_TX_END_BIT | RMT_INT_THR_EVNT_BIT), HEX);
    jsonStatus["            int_st: 0x"] = String (RMT.int_st.val & (RMT_INT_TX_END_BIT | RMT_INT_THR_EVNT_BIT), HEX);
    jsonStatus["        Raw int_st: 0x"] = String (RMT.int_st.val, HEX);

} // GetStatus

#endif // def ARDUINO_ARCH_ESP32
