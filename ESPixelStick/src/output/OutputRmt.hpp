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
#include <driver/rmt.h>

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
#define RMT_Clock_Divisor                           2
#define RMT_TickLengthNS                            float((1/(RMT_ClockRate/RMT_Clock_Divisor))*1000000000.0)

#endif // def ARDUINO_ARCH_ESP32
