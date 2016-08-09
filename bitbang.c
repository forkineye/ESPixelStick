/*
* bitbang.c - Bitbang routines for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
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

#ifdef ESP8266

#include <Arduino.h>
#include "eagle_soc.h"

/* 
  Timings  
    Start Bit - 10us high
    Data bit - 30us
      0 Bit - 10us low / 20us high
      1 Bit - 20us low / 10us high
    Line Reset - 30us low
*/

#define CYCLES_GECE_T0L  (F_CPU / 100000) // 10us
#define CYCLES_GECE_T0H  (F_CPU /  50000) // 20us
#define CYCLES_GECE_T1L  (F_CPU /  50000) // 20us
#define CYCLES_GECE_T1H  (F_CPU / 100000) // 10us
#define CYCLES_GECE      CYCLES_GECE_T0L + CYCLES_GECE_T0H // 30us per bit

static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

/*   
  Each GECE frame is 26 bits long and has the following format:
    Start bit (10us)
    6-Bit Bulb Address, MSB first
    8-Bit Brightness, MSB first
    4-Bit Blue, MSB first
    4-Bit Green, MSB first
    4-Bit Red, MSB first
*/

/* Bit-bang one GECE packet - bit bang method dervied from the NeoPixel 8266 code */
void ICACHE_RAM_ATTR doGECE(uint8_t pin, uint32_t packet) {
    uint32_t t, time0, time1, period, c, startTime, pinMask, mask;
    static uint32_t resetDelay = 0;

    pinMask   = _BV(pin);
    mask      = 0x02000000;     /* Start at bit 26 (MSB) and work our way through the packet */
    startTime = 0;

    time0  = CYCLES_GECE_T0L;
    time1  = CYCLES_GECE_T1L;
    period = CYCLES_GECE;

    /* Wait for required inter-packet idle time */
    while ((micros() - resetDelay) < 35);

    /* 10us Start Bit */
    c = _getCycleCount();
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask);             // Set high
    while ((_getCycleCount() - c) < CYCLES_GECE_T1H);           // Wait 10us
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask);             // Set low

    /* Rest of packet */
    for (t = time0;; t = time0) {
        WDT_FEED();
        noInterrupts();
        if (packet & mask) t = time1;                           // Bit low duration
        while (((c = _getCycleCount()) - startTime) < period);  // Wait for bit start
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask);         // Set low
        startTime = c;                                          // Save start time
        while (((c = _getCycleCount()) - startTime) < t);       // Wait high duration
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask);         // Set high
        interrupts();
        if (!(mask >>= 1)) break;
    }
    while ((_getCycleCount() - startTime) < period);            // Wait for last bit
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask);             // Set low
    resetDelay = micros();
}

#endif
