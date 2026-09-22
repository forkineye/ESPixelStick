/*
* OutputI2S.cpp - driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026, 2026 Shelby Merrick
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
#if defined (SUPPORT_I2S)

#include "output/OutputI2S.hpp"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include <numeric>
#include "esp_attr.h"

//----------------------------------------------------------------------------
// array of transmit buffers that get chained into a ring
static c_OutputI2S::TransmitDmaBuffer_t TransmitDmaBuffers[I2S_NumSendBuffers] DRAM_ATTR __attribute__ ( (aligned (4)));

//----------------------------------------------------------------------------
static void IRAM_ATTR IRQ_Handler (void * arg, void * buffer)
{
    static_cast<c_OutputI2S*> (arg)->ISR_Handler (buffer);
} // IRQ_Handler

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputI2S::ISR_Handler (void * _TransmitDmaBuffer)
{
    I2S_DEBUG_INC_COUNTER (ISR_handler_count);

    TransmitDmaBuffer_t * TransmitDmaBuffer = (TransmitDmaBuffer_t*)_TransmitDmaBuffer;

    #ifdef USE_I2S_DEBUG_COUNTERS
    // show we have used the buffer
    TransmitDmaBuffer->UsageCounter++;

    if (TransmitDmaBuffer->header.owner == I2sParallelDmaBufferOwnerDMA)
    {
        I2S_DEBUG_INC_COUNTER (BufferOwnerFlagError);
    }
    #endif // def USE_I2S_DEBUG_COUNTERS

    I2S_DEBUG_INC_COUNTER (ISR_FillBuffer);

    // reset the buffer management fields
    TransmitDmaBuffer->header.length = TransmitDmaBuffer->header.size;
    TransmitDmaBuffer->header.offset = 0; // Always start to the begining of the buffer

    // fill in the new data
    for (auto & CurrentPort : OutputI2SSlotConfigs)
    {
        if (CurrentPort.IsActive)
        {
            CurrentPort.GetNextIntensityBitSlices (CurrentPort.arg, (I2S_Item_t *) (TransmitDmaBuffer->header.buf), TransmitDmaBuffer->header.size);
        }
    }

    // hand the buffer back to the dma engine
    TransmitDmaBuffer->header.eof = true; // cause an end of buffer interrupt
    TransmitDmaBuffer->header.owner = I2sParallelDmaBufferOwnerDMA;

} // ISR_Handler

//----------------------------------------------------------------------------
c_OutputI2S::c_OutputI2S ()
{
    // DEBUG_START;

    int id = 0;
    for (auto & CurrentChannel : OutputI2SSlotConfigs)
    {
        CurrentChannel.I2SChannelId = id++;
        CurrentChannel.DataPin = gpio_num_t (I2S_PIN_NO_CHANGE);
        CurrentChannel.arg = nullptr;
        CurrentChannel.GetNextIntensityBitSlices = nullptr;
        CurrentChannel.IsActive = false;
    }

    uint BufferId = 0;
    for (auto &CurrentDmaBuffer : TransmitDmaBuffers)
    {
        #ifdef USE_I2S_DEBUG_COUNTERS
        CurrentDmaBuffer.id = BufferId;
        CurrentDmaBuffer.UsageCounter = 0;
        #endif // def USE_I2S_DEBUG_COUNTERS
        CurrentDmaBuffer.header.size = sizeof (CurrentDmaBuffer.data);
        CurrentDmaBuffer.header.length = sizeof (CurrentDmaBuffer.data);
        CurrentDmaBuffer.header.offset = 0; // Always start to the begining of the buffer
        CurrentDmaBuffer.header.sosf = false;
        CurrentDmaBuffer.header.eof = true; // cause an end of buffer interrupt
        CurrentDmaBuffer.header.buf = (uint8_t*)&CurrentDmaBuffer.data[0];
        CurrentDmaBuffer.header.qe.stqe_next = & (TransmitDmaBuffers[BufferId + 1].header);
        CurrentDmaBuffer.header.owner = I2sParallelDmaBufferOwnerDMA;

        // clear the data space
        memset (CurrentDmaBuffer.data, 0xff, CurrentDmaBuffer.header.size);

        // next buffer ID
        ++BufferId;
    }

    // link last buffer to the first buffer
    TransmitDmaBuffers[I2S_NumSendBuffers-1].header.qe.stqe_next = & (TransmitDmaBuffers[0].header);

    // DEBUG_END;
} // c_OutputI2S

//----------------------------------------------------------------------------
c_OutputI2S::~c_OutputI2S ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputI2S

//----------------------------------------------------------------------------
void c_OutputI2S::Begin ()
{
    // DEBUG_START;

    do // once
    {
        // 1. Configure the core I2S settings
        i2s_parallel_config_t i2s_config;
            i2s_config.port = i2s_port_t::I2S_NUM_1;
            i2s_config.gpio_clk = I2S_PIN_NO_CHANGE;
            i2s_config.sample_width = i2s_parallel_sample_width_t::I2S_PARALLEL_WIDTH_8;
            i2s_config.sample_rate = 800000 * 4; // 800khz divided into 4 time slices
            i2s_config.invert_clk = false;
            i2s_config.irq_hndlr = IRQ_Handler;
            i2s_config.arg1 = this;
            memset (i2s_config.gpios_bus, I2S_PIN_NO_CHANGE, sizeof (i2s_config.gpios_bus));
    
        // DEBUG_V (String ("Install driver onto I2S Port ") + String (I2S_DeviceID));
        if (ESP_OK != i2sParallel.driver_install (i2s_config))
        {
            String FailReason = (F ("Failed to install I2S driver"));
            RequestReboot (FailReason, 10000, true);
            break;
        }
        // DEBUG_V ();
    
        // start the outputs
        if (ESP_OK != i2sParallel.send_dma (& (TransmitDmaBuffers[0].header)))
        {
            logcon (F ("Could not start the I2S DMA channel"));
        }
    
        // DEBUG_V ();

    } while (false);

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
void c_OutputI2S::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;

    #ifdef USE_I2S_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["I2S Debug"].to<JsonObject> ();

    debugStatus["ISR_handler_count"]     = I2S_DEBUG_COUNTER (ISR_handler_count);
    debugStatus["ISR_NullBufferPointer"] = String (I2S_DEBUG_COUNTER (ISR_NullBufferPointer));
    debugStatus["BufferOwnerFlagError"]  = I2S_DEBUG_COUNTER (BufferOwnerFlagError);
    debugStatus["ISR_FillBuffer"]        = I2S_DEBUG_COUNTER (ISR_FillBuffer);
    debugStatus["IntensityBitsSent"]     = I2S_DEBUG_COUNTER (IntensityBitsSent);
    debugStatus["I2SEntriesTransfered"]  = I2S_DEBUG_COUNTER (I2SEntriesTransfered);
    debugStatus["I2SXmtFills"]           = I2S_DEBUG_COUNTER (I2SXmtFills);
    #endif // def USE_I2S_DEBUG_COUNTERS
    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
void c_OutputI2S::RegisterSlotDevice  (OutputI2SChannelConfig_t config, uint32_t * DataBit, uint32_t * DataBitMask)
{
    // DEBUG_START;

    do // once
    {
        if (config.I2SChannelId >= I2S_MAX_NUM_PORTS)
        {
            logcon (F ("Invalid I2S channel ID"));
            break;
        }
        if (nullptr == config.GetNextIntensityBitSlices)
        {
            logcon (F ("Invalid GetNextIntensityBit function pointer"));
            break;
        }

        auto & currentConfig = OutputI2SSlotConfigs[config.I2SChannelId];
        currentConfig.arg = config.arg;
        currentConfig.GetNextIntensityBitSlices = config.GetNextIntensityBitSlices;

        *DataBit = 1 << config.I2SChannelId;
        *DataBitMask = ~(1 << config.I2SChannelId);
        // DEBUG_V (String ("DataBit: 0x") + String (*DataBit, HEX));
        SetGpio (config.I2SChannelId, config.DataPin);
    } while (false);

    // DEBUG_END;
} // RegisterSlotDevice

//----------------------------------------------------------------------------
void c_OutputI2S::RemoveSlotDevice  (uint32_t I2SChannelId)
{
    // DEBUG_START;

    do // once
    {
        if (I2SChannelId >= I2S_MAX_NUM_PORTS)
        {
            logcon (F ("Invalid I2S channel ID"));
            break;
        }
        auto & currentConfig = OutputI2SSlotConfigs[I2SChannelId];

        // must be first
        currentConfig.IsActive = false;
        currentConfig.arg = nullptr;
        currentConfig.GetNextIntensityBitSlices = nullptr;

        SetGpio (I2SChannelId, gpio_num_t (I2S_PIN_NO_CHANGE));

    } while (false);

    // DEBUG_END;
} // RemoveSlotDevice

//----------------------------------------------------------------------------
void c_OutputI2S::SetGpio (uint32_t I2SChannelId, gpio_num_t NewGpio)
{
    // DEBUG_START;

    do // once
    {
        if (I2SChannelId >= I2S_MAX_NUM_PORTS)
        {
            logcon (F ("ERROR: Invalid I2S channel ID"));
            break;
        }

        // DEBUG_V (String ("I2SChannelId: ") + String (I2SChannelId));
        // DEBUG_V (String ("     NewGpio: ") + String (NewGpio));
        // DEBUG_V (String ("     DataPin: ") + String (OutputI2SSlotConfigs[I2SChannelId].DataPin));

        if (NewGpio == OutputI2SSlotConfigs[I2SChannelId].DataPin)
        {
            // DEBUG_V ("GPIO did not change. Ignore the request");
            break;
        }

        if (I2S_PIN_NO_CHANGE != OutputI2SSlotConfigs[I2SChannelId].DataPin)
        {
            // DEBUG_V ("release the old GPIO from the matrix");
            ResetGpio (OutputI2SSlotConfigs[I2SChannelId].DataPin);
        }

        // DEBUG_V ("attach the new GPIO");
        OutputI2SSlotConfigs[I2SChannelId].DataPin = NewGpio;
        i2sParallel.iomux_set_signal (NewGpio, I2S1O_DATA_OUT0_IDX + I2SChannelId);

    } while (false);

    // DEBUG_END;
} // SetGpio

//----------------------------------------------------------------------------
void c_OutputI2S::SetOutputState (uint32_t I2SChannelId, bool NewState)
{
    // DEBUG_START;

    // DEBUG_V (String ("I2SChannelId: ") + String (I2SChannelId));
    // DEBUG_V (String ("    NewState: ") + String (NewState));
    OutputI2SSlotConfigs[I2SChannelId].IsActive = NewState;

    // DEBUG_END;
} // SetOutputState

//----------------------------------------------------------------------------
uint32_t c_OutputI2S::GetNumTimeSlicesForTargetTimeNS (uint32_t TargetTimeInNanoSec)
{
    // DEBUG_START;

    uint32_t Result = 0;
    double I2S_TickTimeInNS = i2sParallel.get_clockNS ();

    // round up
    Result = ( (TargetTimeInNanoSec + int (I2S_TickTimeInNS)) - 1) / int (I2S_TickTimeInNS);
    if (0 == Result) Result = 1;

    // DEBUG_V (String ("TargetTimeInNanoSec: ") + String (TargetTimeInNanoSec));
    // DEBUG_V (String ("   I2S_TickTimeInNS: ") + String (I2S_TickTimeInNS));
    // DEBUG_V (String ("             Result: ") + String (Result));
    
    // DEBUG_END;
    return Result;
}

#endif // defined (SUPPORT_I2S)
