#pragma once
/*
* OutputI2S.hpp - I2S driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026 Shelby Merrick
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

#ifdef SUPPORT_I2S
#include "ESPixelStick.h"
#include "i2s_parallel.hpp"
#include "driver/gpio.h"

// #define USE_I2S_DEBUG_COUNTERS

class c_OutputI2S
{
public:
    // a slot is one bit in the output item
    #define I2S_NUM_SLOTA i2s_bits_per_chan_t::I2S_BITS_PER_CHAN_8BIT
    #define I2S_MAX_NUM_PORTS 8
    typedef uint8_t I2S_Item_t;

    struct OutputI2SChannelConfig_t
    {
        uint32_t    I2SChannelId;
        gpio_num_t  DataPin;
        void        *arg;
        void        (*GetNextIntensityBitSlices) (void*arg, I2S_Item_t * data, uint32_t numSlices) = nullptr;
        bool        IsActive;
    };

    // must be a multiple of 32
    #define I2S_NumSendBufferItems 1024

    struct TransmitDmaBuffer_t
    {
        alignas (4) lldesc_t   header; // must be first
        #ifdef USE_I2S_DEBUG_COUNTERS
        alignas (4) uint32_t   id;
        alignas (4) uint32_t   UsageCounter;
        #endif // def USE_I2S_DEBUG_COUNTERS
        alignas (4) I2S_Item_t data[I2S_NumSendBufferItems];
    };

private:
    // array of configuration settings
    OutputI2SChannelConfig_t OutputI2SSlotConfigs[I2S_MAX_NUM_PORTS];

    // must be more than zero
    #define I2S_NumSendBuffers 4

    // Class to manage the io device
    esp_i2s_parallel    i2sParallel;

public:
    c_OutputI2S ();
    virtual ~c_OutputI2S ();

    void        Begin               ();
    void        RegisterSlotDevice  (OutputI2SChannelConfig_t config, uint32_t * DataBit, uint32_t * DataBitMask);
    void        RemoveSlotDevice    (uint32_t I2SChannelId);
    void        GetStatus           (ArduinoJson::JsonObject& jsonStatus);
    void        GetDriverName       (String &value)  { value = F("I2S"); }
    uint32_t    GetNumTimeSlicesForTargetTimeNS    (uint32_t BitTimeInNanoSec);
    void        SetOutputState      (uint32_t I2SChannelId, bool NewState);
    void        SetGpio             (uint32_t I2SChannelId, gpio_num_t NewGpio);
    void        ISR_Handler         (void * _TransmitDmaBuffer);

#ifdef USE_I2S_DEBUG_COUNTERS
   // debug counters
    struct I2SDebugCounters_t
    {
        uint32_t DataTaskcounter = 0;
        uint32_t IntensityBitsSent = 0;
        uint32_t IntensityBitsSentLastFrame = 0;
        uint32_t I2SEntriesTransfered = 0;
        uint32_t I2SXmtFills = 0;
        uint32_t ISRpaused = 0;
        uint32_t WriteToBuffer = 0;
        uint32_t ISR_handler_count = 0;
        uint32_t ISR_FillBuffer = 0;
        uint32_t BufferOwnerFlagError = 0;
        uint32_t ISR_NullBufferPointer = 0;
    };
    I2SDebugCounters_t I2SDebugCounters;

#define I2S_DEBUG_INC_COUNTER(p) (++I2SDebugCounters.p)
#define I2S_DEBUG_COUNTER(p)  (I2SDebugCounters.p)

#else

#define I2S_DEBUG_INC_COUNTER(p)
#define I2S_DEBUG_COUNTER(p)

#endif // def USE_I2S_DEBUG_COUNTERS

};
#endif // def #ifdef SUPPORT_I2S