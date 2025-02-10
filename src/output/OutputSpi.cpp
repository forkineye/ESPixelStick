/*
* OutputSpi.cpp - SPI driver code for ESPixelStick SPI Channel
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
#include "ESPixelStick.h"
#ifdef SUPPORT_SPI_OUTPUT

#include "output/OutputSpi.hpp"
#include "driver/spi_master.h"
// #include <esp_heap_alloc_caps.h>

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static bool spi_transfer_callback_enabled = false;
static void IRAM_ATTR spi_transfer_callback (spi_transaction_t * param)
{
    if ((spi_transfer_callback_enabled) && (param))
    {
        if (param->user)
        {
            reinterpret_cast <c_OutputSpi*> (param->user)->DataCbCounter++;
            TaskHandle_t Handle = reinterpret_cast <c_OutputSpi*> (param->user)->GetTaskHandle ();
            if (Handle)
            {
                vTaskResume (Handle);
            }
        }
        else
        {
            LOG_PORT.println (F ("SPI User Parm is null."));
        }
    }
} // spi_transfer_callback

//----------------------------------------------------------------------------
static void SendSpiIntensityDataTask (void* pvParameters)
{
    // DEBUG_START; Needs extra stack space to run this
    do
    {
        // we start suspended
        vTaskSuspend (NULL); //Suspend Own Task

        c_OutputSpi* OutputSpi = reinterpret_cast <c_OutputSpi*> (pvParameters);
        OutputSpi->DataTaskcounter++;
        OutputSpi->SendIntensityData ();

    } while (true);
    // DEBUG_END;

} // SendSpiIntensityDataTask

//----------------------------------------------------------------------------
c_OutputSpi::c_OutputSpi ()
{
    // DEBUG_START;

    for (auto & TransactionBuffer : TransactionBuffers)
    {
        TransactionBuffer = nullptr;
    }

    // DEBUG_END;
} // c_OutputSpi

//----------------------------------------------------------------------------
c_OutputSpi::~c_OutputSpi ()
{
    // DEBUG_START;

    if(HasBeenInitialized)
    {
        spi_transfer_callback_enabled = false;
        logcon(CN_stars + String(F(" SPI Interface Shutdown requires a reboot ")) + CN_stars);
        RequestReboot(100000);
    }
    // DEBUG_END;

} // ~c_OutputSpi

#if defined(SUPPORT_OutputType_GRINCH)
void c_OutputSpi::Begin (c_OutputGrinch* _OutputGrinch)
{
    OutputGrinch = _OutputGrinch;
    Begin ((c_OutputPixel*)nullptr);
}
#endif // defined(SUPPORT_OutputType_GRINCH)

//----------------------------------------------------------------------------
void c_OutputSpi::Begin (c_OutputPixel* _OutputPixel)
{
    // DEBUG_START;

    OutputPixel = _OutputPixel;

    NextTransactionToFill = 0;
    for (auto & TransactionBufferToSet : TransactionBuffers)
    {
        TransactionBuffers[NextTransactionToFill] = (byte*)pvPortMalloc (SPI_NUM_INTENSITY_PER_TRANSACTION); ///< Pointer to transmit buffer
        // DEBUG_V (String ("tx_buffer: 0x") + String (uint32_t (TransactionBuffers[NextTransactionToFill]), HEX));
        NextTransactionToFill++;
    }

    NextTransactionToFill = 0;

    xTaskCreate (SendSpiIntensityDataTask, "SPITask", 2000, this, ESP_TASK_PRIO_MIN + 4, &SendIntensityDataTaskHandle);

    spi_bus_config_t SpiBusConfiguration;
    memset ( (void*)&SpiBusConfiguration, 0x00, sizeof (SpiBusConfiguration));
    SpiBusConfiguration.miso_io_num = -1;
    SpiBusConfiguration.mosi_io_num = DataPin;
    SpiBusConfiguration.sclk_io_num = ClockPin;
    SpiBusConfiguration.quadwp_io_num = -1;
    SpiBusConfiguration.quadhd_io_num = -1;
    SpiBusConfiguration.max_transfer_sz = OM_MAX_NUM_CHANNELS;
    SpiBusConfiguration.flags = SPICOMMON_BUSFLAG_MASTER;

    spi_device_interface_config_t SpiDeviceConfiguration;
    memset ( (void*)&SpiDeviceConfiguration, 0x00, sizeof (SpiDeviceConfiguration));
    // SpiDeviceConfiguration.command_bits = 0; // No command to send
    // SpiDeviceConfiguration.address_bits = 0; // No bus address to send
    // SpiDeviceConfiguration.dummy_bits = 0; // No dummy bits to send
    // SpiDeviceConfiguration.duty_cycle_pos = 0; // 50% Duty cycle
    SpiDeviceConfiguration.clock_speed_hz = SPI_SPI_MASTER_FREQ_1M;
    SpiDeviceConfiguration.mode = 0;                                // SPI mode 0
    SpiDeviceConfiguration.spics_io_num = -1;                       // we will NOT use CS pin
    SpiDeviceConfiguration.queue_size = 1;    // We want to be able to queue 2 transactions at a time
    // SpiDeviceConfiguration.pre_cb = nullptr;                     // Specify pre-transfer callback to handle D/C line
    SpiDeviceConfiguration.post_cb = spi_transfer_callback;      // Specify post-transfer callback to handle D/C line
    // SpiDeviceConfiguration.flags = 0;

    ESP_ERROR_CHECK (spi_bus_initialize (SPI_SPI_HOST, &SpiBusConfiguration, SPI_SPI_DMA_CHANNEL));
    ESP_ERROR_CHECK (spi_bus_add_device (SPI_SPI_HOST, &SpiDeviceConfiguration, &spi_device_handle));
    ESP_ERROR_CHECK (spi_device_acquire_bus (spi_device_handle, portMAX_DELAY));

    spi_transfer_callback_enabled = true;

    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputSpi::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    bool response = true;
    JsonObject SpiConfig = jsonConfig["dataspi"];
    response |= setFromJSON(CsPin,    SpiConfig, CN_cs_pin);

/*
    response |= setFromJSON(DataPin,  SpiConfig, CN_data_pin);
    response |= setFromJSON(ClockPin, SpiConfig, CN_clock_pin);
*/
    // DEBUG_END;

    return response;
} // SetConfig

//----------------------------------------------------------------------------
void c_OutputSpi::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    JsonObject SpiConfig = jsonConfig["dataspi"].to<JsonObject>();
    JsonWrite(SpiConfig, CN_cs_pin,    CsPin);
    JsonWrite(SpiConfig, CN_data_pin,  DataPin);
    JsonWrite(SpiConfig, CN_clock_pin, ClockPin);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputSpi::ISR_MoreDataToSend()
{
    bool response = false;
    if(OutputPixel)
    {
        response = OutputPixel->ISR_MoreDataToSend ();
    }
#if defined(SUPPORT_OutputType_GRINCH)
    else if(OutputGrinch)
    {
        response = OutputGrinch->ISR_MoreDataToSend ();
    }
#endif // defined(SUPPORT_OutputType_GRINCH)
    return response;
}

//----------------------------------------------------------------------------
bool c_OutputSpi::ISR_GetNextIntensityToSend(uint32_t& Data)
{
    bool response = false;

    if(OutputPixel)
    {
        response = OutputPixel->ISR_GetNextIntensityToSend (Data);
    }
#if defined(SUPPORT_OutputType_GRINCH)
    else if(OutputGrinch)
    {
        response = OutputGrinch->ISR_GetNextIntensityToSend (Data);
    }
#endif // defined(SUPPORT_OutputType_GRINCH)

    return response;
} // ISR_GetNextIntensityToSend

//----------------------------------------------------------------------------
void c_OutputSpi::SendIntensityData ()
{
    // DEBUG_START;
    SendIntensityDataCounter++;

    if (ISR_MoreDataToSend ())
    {
        spi_transaction_t & TransactionToFill = Transactions[NextTransactionToFill];
        memset ( (void*)&Transactions[NextTransactionToFill], 0x00, sizeof (spi_transaction_t));

        TransactionToFill.user = this;         ///< User-defined variable. Can be used to store eg transaction ID.
        byte * pMem = &TransactionBuffers[NextTransactionToFill][0];
        TransactionToFill.tx_buffer = pMem;
        uint32_t NumEmptyIntensitySlots = SPI_NUM_INTENSITY_PER_TRANSACTION;
        uint32_t IntensityData = 0;

        while ( (NumEmptyIntensitySlots) && (ISR_MoreDataToSend ()))
        {
            ISR_GetNextIntensityToSend (IntensityData);
            *pMem++ = byte(IntensityData);
            --NumEmptyIntensitySlots;
        } // end while there is space in the buffer

        TransactionToFill.length = SPI_BITS_PER_INTENSITY * (SPI_NUM_INTENSITY_PER_TRANSACTION - NumEmptyIntensitySlots);
        if (!ISR_MoreDataToSend ())
        {
            TransactionToFill.length++;
        }

        if(gpio_num_t(-1) != CsPin)
        {
            // turn on the output strobe (latch data)
            digitalWrite(CsPin, LOW);
        }

        ESP_ERROR_CHECK (spi_device_queue_trans (spi_device_handle, &Transactions[NextTransactionToFill], portMAX_DELAY));

        if (++NextTransactionToFill >= SPI_NUM_TRANSACTIONS)
        {
            NextTransactionToFill = 0;
        }

        if(gpio_num_t(-1) != CsPin)
        {
            if (!ISR_MoreDataToSend ())
            {
                spi_transaction_t * pspi_transaction = &TransactionToFill;
                spi_device_get_trans_result(spi_device_handle, &pspi_transaction, 100);

                // turn on the output strobe (latch data)
                digitalWrite(CsPin, HIGH);
            }
        }
    }

    // DEBUG_END;

} // SendIntensityData

//----------------------------------------------------------------------------
void c_OutputSpi::StartNewFrame()
{
    bool response = false;

    if(OutputPixel)
    {
        OutputPixel->StartNewFrame ();
    }
#if defined(SUPPORT_OutputType_GRINCH)
    else if(OutputGrinch)
    {
        OutputGrinch->StartNewFrame ();
    }
#endif // defined(SUPPORT_OutputType_GRINCH)

} // StartNewFrame

//----------------------------------------------------------------------------
bool c_OutputSpi::Poll ()
{
    bool Response = false;

    // DEBUG_START;

    StartNewFrame ();

    if(gpio_num_t(-1) != CsPin)
    {
        // turn on the output strobe (latch data)
        ResetGpio(CsPin);
        pinMode(CsPin, OUTPUT);
    }

    // fill all the available buffers
    NextTransactionToFill = 0;
    for (auto& TransactionToFill : Transactions)
    {
        if (SendIntensityDataTaskHandle)
        {
            vTaskResume (SendIntensityDataTaskHandle);
        }
    }

    spi_transfer_callback_enabled = true;
    Response = true;

    // DEBUG_END;

    return Response;
} // render

#endif // def SUPPORT_SPI_OUTPUT
