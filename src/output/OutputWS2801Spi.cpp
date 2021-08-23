/*
* OutputWS2801Spi.cpp - WS2801 driver code for ESPixelStick SPI Channel
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

#include "../ESPixelStick.h"
#ifdef USE_WS2801
#include "OutputWS2801Spi.hpp"
#include <driver/spi_master.h>

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR ws2801_transfer_callback (spi_transaction_t * param)
{
    reinterpret_cast <c_OutputWS2801Spi*>(param->user)->CB_Handler_SendIntensityData ();
} // ws2801_transfer_callback

//----------------------------------------------------------------------------
c_OutputWS2801Spi::c_OutputWS2801Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputWS2801 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;


    // DEBUG_END;
} // c_OutputWS2801Spi

//----------------------------------------------------------------------------
c_OutputWS2801Spi::~c_OutputWS2801Spi ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    if (spi_device_handle)
    {
        spi_device_release_bus (spi_device_handle);
        spi_bus_remove_device (spi_device_handle);
    }
    spi_bus_free (VSPI_HOST);

    // DEBUG_END;

} // ~c_OutputWS2801Spi

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2801Spi::Begin ()
{
    // DEBUG_START;

    spi_bus_config_t SpiBusConfiguration;
    memset ((void*)&SpiBusConfiguration, 0x00, sizeof (SpiBusConfiguration));
    SpiBusConfiguration.miso_io_num = -1;
    SpiBusConfiguration.mosi_io_num = DataPin;
    SpiBusConfiguration.sclk_io_num = ClockPin;
    SpiBusConfiguration.quadwp_io_num = -1;
    SpiBusConfiguration.quadhd_io_num = -1;
    SpiBusConfiguration.max_transfer_sz = OM_MAX_NUM_CHANNELS;
    SpiBusConfiguration.flags = SPICOMMON_BUSFLAG_MASTER;

    spi_device_interface_config_t SpiDeviceConfiguration;
    memset ((void*)&SpiDeviceConfiguration, 0x00, sizeof (SpiDeviceConfiguration));
    // SpiDeviceConfiguration.command_bits = 0; // No command to send
    // SpiDeviceConfiguration.address_bits = 0; // No bus address to send
    // SpiDeviceConfiguration.dummy_bits = 0; // No dummy bits to send
    // SpiDeviceConfiguration.duty_cycle_pos = 0; // 50% Duty cycle
    SpiDeviceConfiguration.clock_speed_hz = WS2801_SPI_MASTER_FREQ_1M;
    SpiDeviceConfiguration.mode = 1;                                // SPI mode 1
    SpiDeviceConfiguration.spics_io_num = -1;                       // we will NOT use CS pin
    SpiDeviceConfiguration.queue_size = WS2801_NUM_TRANSACTIONS;    // We want to be able to queue 2 transactions at a time
    // SpiDeviceConfiguration.pre_cb = nullptr;                     // Specify pre-transfer callback to handle D/C line
    // SpiDeviceConfiguration.post_cb = ws2801_transfer_callback;      // Specify post-transfer callback to handle D/C line
    // SpiDeviceConfiguration.flags = 0;

    ESP_ERROR_CHECK (spi_bus_initialize (WS2801_SPI_HOST, &SpiBusConfiguration, WS2801_SPI_DMA_CHANNEL));
    ESP_ERROR_CHECK (spi_bus_add_device (WS2801_SPI_HOST, &SpiDeviceConfiguration, &spi_device_handle));
    ESP_ERROR_CHECK (spi_device_acquire_bus (spi_device_handle, portMAX_DELAY));

    memset ((void*)&Transactions[0], 0x00, sizeof (Transactions));
    for (Transaction_t & TransactionToFillToFill : Transactions)
    {
        // TransactionToFillToFill.SpiTransaction.flags = 0;        ///< Bitwise OR of SPI_TRANS_* flags
        // TransactionToFillToFill.SpiTransaction.cmd = 0;          ///< No command
        // TransactionToFillToFill.SpiTransaction.ddr = 0;          ///< No Address
        // TransactionToFillToFill.SpiTransaction.length = 0;       ///< Total data length, in bits
        // TransactionToFillToFill.SpiTransaction.rxlength = 0;     ///< Total data length received, should be not greater than ``length`` in full-duplex mode (0 defaults this to the value of ``length``).
        TransactionToFillToFill.SpiTransaction.user = this;         ///< User-defined variable. Can be used to store eg transaction ID.
        TransactionToFillToFill.SpiTransaction.tx_buffer = TransactionToFillToFill.buffer; ///< Pointer to transmit buffer
    }

    NextTransactionToFill = 0;

    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
void c_OutputWS2801Spi::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputWS2801::GetConfig (jsonConfig);

    jsonConfig[CN_clock_pin] = ClockPin;

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputWS2801Spi::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputWS2801::GetStatus (jsonStatus);

    // jsonStatus["FrameStartCounter"] = FrameStartCounter;
    jsonStatus["DataISRcounter"] = DataISRcounter;

} // GetStatus

//----------------------------------------------------------------------------
bool c_OutputWS2801Spi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    gpio_num_t OldDataPin = DataPin;
    gpio_num_t OldClockPin = ClockPin;

    bool response = c_OutputWS2801::SetConfig (jsonConfig);
    
    setFromJSON (ClockPin, jsonConfig, CN_clock_pin);
    
    if ((OldDataPin != DataPin) || (OldClockPin != ClockPin))
    {
        // terminat the spi interface
        if (spi_device_handle)
        {
            spi_bus_remove_device (spi_device_handle);
        }
        spi_bus_free (VSPI_HOST);

        // start over.
        Begin ();
    }

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
/*
 * Fill the buffer with a fixed number of intensity values.
 */
void IRAM_ATTR c_OutputWS2801Spi::CB_Handler_SendIntensityData ()
{
    DataISRcounter++;

    if (MoreDataToSend)
    {
        Transaction_t & TransactionToFill = Transactions[NextTransactionToFill];
        uint32_t NumEmptyIntensitySlots = sizeof (Transactions[0].buffer);
        byte* pMem = &TransactionToFill.buffer[0];

        while ((NumEmptyIntensitySlots) && (MoreDataToSend))
        {
            *pMem++ = GetNextIntensityToSend ();
            --NumEmptyIntensitySlots;
        } // end while there is space in the buffer

        TransactionToFill.SpiTransaction.length = WS2801_BITS_PER_INTENSITY * (sizeof (Transactions[0].buffer) - NumEmptyIntensitySlots);
        if (!MoreDataToSend)
        {
            TransactionToFill.SpiTransaction.length++;
        }
        
        if (++NextTransactionToFill >= WS2801_NUM_TRANSACTIONS)
        {
            NextTransactionToFill = 0;
        }

        ESP_ERROR_CHECK (spi_device_queue_trans (spi_device_handle, &(TransactionToFill.SpiTransaction), TickType_t (portMAX_DELAY)));
    }

} // CB_Handler_SendIntensityData

//----------------------------------------------------------------------------
void c_OutputWS2801Spi::Render ()
{
    // DEBUG_START;

    if (canRefresh ())
    {
        StartNewFrame ();
        ReportNewFrame ();

        // fill all the available buffers
        NextTransactionToFill = 0;
        for (Transaction_t & TransactionToFillToFill : Transactions)
        {
            CB_Handler_SendIntensityData ();
        }
    }

    // DEBUG_END;

} // render

#endif // def USE_WS2801
