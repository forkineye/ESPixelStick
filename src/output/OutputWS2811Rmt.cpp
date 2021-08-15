/*
* OutputWS2811Rmt.cpp - WS2811 driver code for ESPixelStick RMT Channel
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

#include "../ESPixelStick.h"
#include <driver/rmt.h>
#include "OutputWS2811Rmt.hpp"

#define RmtChannelId                              rmt_channel_t(UartId)
#define NumBitsPerByte                            8
#define MAX_NUM_INTENSITY_BIT_SLOTS_PER_INTERRUPT (sizeof(RMTMEM.chan[0].data32) / sizeof (rmt_item32_t))
#define NUM_FRAME_START_SLOTS                     6

#define RMT__INT_TX_END     (1)
#define RMT__INT_RX_END     (2)
#define RMT__INT_ERROR      (4)
#define RMT__INT_THR_EVNT   (1<<24)

#define RMT_INT_TX_END(channel)     (RMT__INT_TX_END   << (uint32_t(channel)*3))
#define RMT_INT_RX_END(channel)     (RMT__INT_RX_END   << (uint32_t(channel)*3))
#define RMT_INT_ERROR(channel)      (RMT__INT_ERROR    << (uint32_t(channel)*3))
#define RMT_INT_THR_EVNT(channel)  ((RMT__INT_THR_EVNT)<< (uint32_t(channel)))

#define RMT_ClockRate                               80000000.0
#define RMT_Clock_Divisor                           2.0
#define RMT_TickLengthNS                            float((1/(RMT_ClockRate/RMT_Clock_Divisor))*1000000000.0)

#define WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH_WS2812    uint16_t (WS2811_PIXEL_NS_BIT_0_HIGH_WS2812 / RMT_TickLengthNS)
#define WS2811_PIXEL_RMT_TICKS_BIT_0_LOW_WS2812     uint16_t (WS2811_PIXEL_NS_BIT_0_LOW_WS2812  / RMT_TickLengthNS)
#define WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH_WS2812    uint16_t (WS2811_PIXEL_NS_BIT_1_HIGH_WS2812 / RMT_TickLengthNS)
#define WS2811_PIXEL_RMT_TICKS_BIT_1_LOW_WS2812     uint16_t (WS2811_PIXEL_NS_BIT_1_LOW_WS2812  / RMT_TickLengthNS)
#define WS2811_PIXEL_RMT_TICKS_IDLE_WS2812          uint16_t (WS2811_PIXEL_NS_IDLE_WS2812       / RMT_TickLengthNS)

#define INTERFRAME_GAP_ID   2
#define STARTBIT_ID         3
#define STOPBIT_ID          4

// forward declaration for the isr handler
static void IRAM_ATTR rmt_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::c_OutputWS2811Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputWS2811 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    Rgb2Rmt[0].duration0 = WS2811_PIXEL_RMT_TICKS_BIT_0_LOW_WS2812;
    Rgb2Rmt[0].level0 = 0;
    Rgb2Rmt[0].duration1 = WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH_WS2812;
    Rgb2Rmt[0].level1 = 1;

    Rgb2Rmt[1].duration0 = WS2811_PIXEL_RMT_TICKS_BIT_1_LOW_WS2812;
    Rgb2Rmt[1].level0 = 0;
    Rgb2Rmt[1].duration1 = WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH_WS2812;
    Rgb2Rmt[1].level1 = 1;

    // 300us Interframe gap
    Rgb2Rmt[INTERFRAME_GAP_ID].duration0 = WS2811_PIXEL_RMT_TICKS_IDLE_WS2812 / 2;
    Rgb2Rmt[INTERFRAME_GAP_ID].level0 = 0;
    Rgb2Rmt[INTERFRAME_GAP_ID].duration1 = WS2811_PIXEL_RMT_TICKS_IDLE_WS2812 / 2;
    Rgb2Rmt[INTERFRAME_GAP_ID].level1 = 0;

    // Start Bit
    Rgb2Rmt[STARTBIT_ID].duration0 = WS2811_PIXEL_RMT_TICKS_BIT_0_LOW_WS2812;
    Rgb2Rmt[STARTBIT_ID].level0 = 1;
    Rgb2Rmt[STARTBIT_ID].duration1 = WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH_WS2812;
    Rgb2Rmt[STARTBIT_ID].level1 = 1;

    // Stop Bit
    Rgb2Rmt[STOPBIT_ID].duration0 = 0;
    Rgb2Rmt[STOPBIT_ID].level0 = 0;
    Rgb2Rmt[STOPBIT_ID].duration1 = 0;
    Rgb2Rmt[STOPBIT_ID].level1 = 0;

    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH_WS2812: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_0_HIGH_WS2812, HEX));
    // DEBUG_V (String (" WS2811_PIXEL_RMT_TICKS_BIT_0_LOW_WS2812: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_0_LOW_WS2812,  HEX));
    // DEBUG_V (String ("WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH_WS2812: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_1_HIGH_WS2812, HEX));
    // DEBUG_V (String (" WS2811_PIXEL_RMT_TICKS_BIT_1_LOW_WS2812: 0x") + String (WS2811_PIXEL_RMT_TICKS_BIT_1_LOW_WS2812,  HEX));

    // DEBUG_END;
} // c_OutputWS2811Rmt

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::~c_OutputWS2811Rmt ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    // Disable all interrupts for this RMT Channel.
    // DEBUG_V ("");

    // Clear all pending interrupts in the RMT Channel
    // DEBUG_V ("");
    RMT.int_ena.val &= ~RMT_INT_TX_END (RmtChannelId);
    RMT.int_ena.val &= ~RMT_INT_THR_EVNT (RmtChannelId);

    esp_intr_free (RMT_intr_handle);

    // make sure no existing low level driver is running
    // DEBUG_V ("");

    // DEBUG_V (String("RmtChannelId: ") + String(RmtChannelId));
    // rmtEnd (RmtObject);
    // DEBUG_END;
} // ~c_OutputWS2811Rmt

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR rmt_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2811Rmt*>(param)->ISR_Handler ();
} // rmt_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811Rmt::Begin ()
{
    // DEBUG_START;
    // RmtChannelId = 0;
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
    RmtConfig.tx_config.idle_level = rmt_idle_level_t::RMT_IDLE_LEVEL_HIGH;
    RmtConfig.tx_config.idle_output_en = true;

    RmtStartAddr = &RMTMEM.chan[RmtChannelId].data32[0];
    RmtEndAddr   = &RMTMEM.chan[RmtChannelId].data32[63];

    // the math here results in a modulo 8 of the maximum number of slots to fill at frame start time.
    NumIntensityValuesPerInterrupt = ((MAX_NUM_INTENSITY_BIT_SLOTS_PER_INTERRUPT - NUM_FRAME_START_SLOTS) / NumBitsPerByte);
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
bool c_OutputWS2811Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputWS2811::SetConfig (jsonConfig);

    uint32_t ifgNS = (InterFrameGapInMicroSec * 1000);
    uint32_t ifgTicks = ifgNS / RMT_TickLengthNS;

    /*
        Interframe gap object gets sent one time and each transmit sends 5
        gap periods
    */

    // Default is 100us * 3
    Rgb2Rmt[INTERFRAME_GAP_ID].duration0 = ifgTicks / 10;
    Rgb2Rmt[INTERFRAME_GAP_ID].level0 = 0;
    Rgb2Rmt[INTERFRAME_GAP_ID].duration1 = ifgTicks / 10;
    Rgb2Rmt[INTERFRAME_GAP_ID].level1 = 0;

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputWS2811::GetStatus (jsonStatus);

    // jsonStatus["FrameStartCounter"] = FrameStartCounter;
    // jsonStatus["DataISRcounter"] = DataISRcounter;
    // jsonStatus["FrameDoneCounter"] = FrameDoneCounter;
    // jsonStatus["FrameEndISRcounter"] = FrameEndISRcounter;

} // GetStatus

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811Rmt::ISR_Handler ()
{
    if (RMT.int_st.val & RMT_INT_TX_END (RmtChannelId))
    {
        // FrameEndISRcounter++;

        RMT.int_clr.val = RMT_INT_TX_END (RmtChannelId);
        RMT.int_clr.val = RMT_INT_THR_EVNT (RmtChannelId);

        RMT.int_ena.val &= ~RMT_INT_TX_END (RmtChannelId);
        RMT.int_ena.val &= ~RMT_INT_THR_EVNT (RmtChannelId);

        // ISR_Handler_StartNewFrame ();
    }
    else if (RMT.int_st.val & RMT_INT_THR_EVNT (RmtChannelId))
    {
        // DataISRcounter++;

        // RMT.int_ena.val &= ~RMT_INT_THR_EVNT (RmtChannelId);
        RMT.int_clr.val = RMT_INT_THR_EVNT (RmtChannelId);

        if (MoreDataToSend)
        {
            ISR_Handler_SendIntensityData ();
            RMT.conf_ch[RmtChannelId].conf1.tx_start = 1;
        }
        else
        {
            RMT.int_ena.val &= ~RMT_INT_THR_EVNT (RmtChannelId);
        }
    }
} // ISR_Handler

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2811Rmt::ISR_Handler_StartNewFrame ()
{
    FrameStartCounter++;

    RMT.conf_ch[RmtChannelId].conf1.mem_rd_rst = 1; // set the internal pointer to the start of the mem block
    RMT.conf_ch[RmtChannelId].conf1.mem_rd_rst = 0;

    uint32_t* pMem = (uint32_t*)RmtStartAddr;

    // Need to build up a backlog of entries in the buffer
    // so that there is still plenty of data to send when the isr fires.
    // This is reflected in the constant: NUM_FRAME_START_SLOTS
    *pMem++ = Rgb2Rmt[INTERFRAME_GAP_ID].val; // 60 us
    *pMem++ = Rgb2Rmt[INTERFRAME_GAP_ID].val; // 60 us
    *pMem++ = Rgb2Rmt[INTERFRAME_GAP_ID].val; // 60 us
    *pMem++ = Rgb2Rmt[INTERFRAME_GAP_ID].val; // 60 us
    *pMem++ = Rgb2Rmt[INTERFRAME_GAP_ID].val; // 60 us
    *pMem++ = Rgb2Rmt[STARTBIT_ID].val;       // Start bit
    RmtCurrentAddr = (volatile rmt_item32_t*)pMem;

    RMT.int_clr.val  = RMT_INT_THR_EVNT (RmtChannelId);
    RMT.int_ena.val |= RMT_INT_THR_EVNT (RmtChannelId);

    StartNewFrame ();
    ISR_Handler_SendIntensityData ();

    RMT.tx_lim_ch[RmtChannelId].limit = NumIntensityBitsPerInterrupt;

    RMT.int_clr.val  = RMT_INT_TX_END (RmtChannelId);
    RMT.int_ena.val |= RMT_INT_TX_END (RmtChannelId);
    RMT.conf_ch[RmtChannelId].conf1.tx_start = 1;

} // ISR_Handler_StartNewFrame

//----------------------------------------------------------------------------
/*
 * Fill the MEMBLK with a fixed number of intensity values.
 */
void IRAM_ATTR c_OutputWS2811Rmt::ISR_Handler_SendIntensityData ()
{
    uint32_t* pMem = (uint32_t*)RmtCurrentAddr;
    register uint32_t OneValue  = Rgb2Rmt[1].val;
    register uint32_t ZeroValue = Rgb2Rmt[0].val;
    uint32_t NumEmptyIntensitySlots = NumIntensityValuesPerInterrupt;

    while ((NumEmptyIntensitySlots--) && (MoreDataToSend))
    {
        uint8_t IntensityValue = GetNextIntensityToSend ();

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
    *pMem = Rgb2Rmt[STOPBIT_ID].val;

    RmtCurrentAddr = (volatile rmt_item32_t*)pMem;

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::Render ()
{
    // DEBUG_START;

    if ( 0 == (RMT.int_ena.val & (RMT_INT_TX_END (RmtChannelId) | RMT_INT_THR_EVNT (RmtChannelId))))
    {
        ISR_Handler_StartNewFrame ();
    }

    // DEBUG_END;

} // render

#endif // def ARDUINO_ARCH_ESP32
