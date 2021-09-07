#pragma once
/*
* OutputRmt.hpp - RMT driver code for ESPixelStick RMT Channel
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
#include "OutputPixel.hpp"
#include <driver/rmt.h>

class c_OutputRmt
{
public:
    c_OutputRmt ();
    ~c_OutputRmt ();

    void Begin (rmt_channel_t ChannelId, gpio_num_t DataPin, c_OutputPixel * OutputPixel, rmt_idle_level_t idle_level);
    bool Render ();
    void set_pin (gpio_num_t _DataPin) { DataPin = _DataPin; rmt_set_gpio (RmtChannelId, rmt_mode_t::RMT_MODE_TX, DataPin, false); }

#define RMT_ClockRate       80000000.0
#define RMT_Clock_Divisor   2.0
#define RMT_TickLengthNS    float ( (1/ (RMT_ClockRate/RMT_Clock_Divisor)) * 1000000000.0)

    enum RmtFrameType_t
    {
        RMT_DATA_BIT_ZERO_ID = 0,
        RMT_DATA_BIT_ONE_ID,
        RMT_INTERFRAME_GAP_ID,
        RMT_STARTBIT_ID,
        RMT_STOPBIT_ID,
    };
    void SetRgb2Rmt (rmt_item32_t NewValue, RmtFrameType_t ID) { Rgb2Rmt[ID] = NewValue; }

    void IRAM_ATTR ISR_Handler ();
    void IRAM_ATTR ISR_Handler_StartNewFrame ();
    void IRAM_ATTR ISR_Handler_SendIntensityData ();

private:
    c_OutputPixel* OutputPixel = nullptr;
    rmt_channel_t RmtChannelId = rmt_channel_t (-1);
    gpio_num_t  DataPin = gpio_num_t (-1);
    rmt_item32_t  Rgb2Rmt[5];

    volatile rmt_item32_t* RmtStartAddr = nullptr;
    volatile rmt_item32_t* RmtCurrentAddr = nullptr;
    volatile rmt_item32_t* RmtEndAddr = nullptr;
    intr_handle_t RMT_intr_handle = NULL;
    uint8_t NumIntensityValuesPerInterrupt = 0;
    uint8_t NumIntensityBitsPerInterrupt = 0;

    // debug counters
    // uint32_t DataISRcounter = 0;
    // uint32_t FrameEndISRcounter = 0;
    // uint32_t FrameStartCounter = 0;


};


#endif // def ARDUINO_ARCH_ESP32
