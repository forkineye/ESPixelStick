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
#ifdef ARDUINO_ARCH_ESP32

#include "../ESPixelStick.h"
#include "OutputWS2801Spi.hpp"


// forward declaration for the isr handler
static void IRAM_ATTR SPI_intr_handler (void* param);

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

    // Disable all interrupts for this SPI Channel.
    // DEBUG_V ("");

    // Clear all pending interrupts in the SPI Channel
    // DEBUG_V ("");
    
    // spicommon_dma_chan_free (int dma_chan);
    // spicommon_periph_free (spi_host_device_thost);
    // spicommon_bus_free_io (spi_host_device_thost);
    // esp_intr_free (SPI_intr_handle);

    // make sure no existing low level driver is running
    // DEBUG_V ("");

    // DEBUG_V (String("SPIChannelId: ") + String(SPIChannelId));
    // SPIEnd (SPIObject);
    // DEBUG_END;
} // ~c_OutputWS2801Spi

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR SPI_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2801Spi*>(param)->ISR_Handler ();
} // SPI_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2801Spi::Begin ()
{
    // DEBUG_START;
    // SPIChannelId = 0;
    // DEBUG_V (String ("DataPin: ") + String (DataPin));
    // DEBUG_V (String (" SPIChannelId: ") + String (SPIChannelId));

    // int dmach = 1;
    esp_err_t ret;
    spi_device_handle_t spi = nullptr;

    spi_bus_config_t buscfg;
    memset ((void*)&buscfg, 0x00, sizeof (buscfg));
    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = DataPin;
    buscfg.sclk_io_num = 15;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = OM_MAX_NUM_CHANNELS;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;

    spi_device_interface_config_t devcfg;
    memset ((void*)&devcfg, 0x00, sizeof (devcfg));
    // devcfg.command_bits = 0; // No command to send
    // devcfg.address_bits = 0; // No bus address to send
    // devcfg.dummy_bits = 0; // No dummy bits to send
    // devcfg.duty_cycle_pos = 0; // 50% Duty cycle
    devcfg.clock_speed_hz = SPI_MASTER_FREQ_1M;     //Initial clock out at 5 MHz
    // devcfg.mode = 0;                                //SPI mode 0
    devcfg.spics_io_num = -1;                       //we will NOT use CS pin
    devcfg.queue_size = 2;                          //We want to be able to queue 7 transactions at a time
    // devcfg.pre_cb = nullptr;                        //Specify pre-transfer callback to handle D/C line
    // devcfg.post_cb = ws2801_transfer_callback;      //Specify post-transfer callback to handle D/C line
    // devcfg.flags = 0;

    // spi_bus_initialize (spi_host_device_thost, constspi_bus_config_t * bus_config, int dma_chan);

    // spicommon_dma_chan_claim (int dma_chan)
    // spicommon_periph_claim (spi_host_device_thost);
    // spicommon_bus_initialize_io (spi_host_device_thost, constspi_bus_config_t * bus_config, int dma_chan, int flags, bool* is_native);
    // ESP_ERROR_CHECK (esp_intr_alloc (ETS_SPI_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_SHARED, SPI_intr_handler, this, &SPI_intr_handle));

    // spicommon_setup_dma_desc_links (lldesc_t * dmadesc, int len, const uint8_t * data, bool isrx);

    // Start output
    // DEBUG_END;

} // init

//----------------------------------------------------------------------------
bool c_OutputWS2801Spi::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputWS2801::SetConfig (jsonConfig);

    //     SPI_set_pin (SPIChannelId, SPI_mode_t::SPI_MODE_TX, DataPin);

    // DEBUG_END;
    return response;

} // GetStatus

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2801Spi::ISR_Handler ()
{

} // ISR_Handler

//----------------------------------------------------------------------------
void IRAM_ATTR c_OutputWS2801Spi::ISR_Handler_StartNewFrame ()
{
    FrameStartCounter++;



    StartNewFrame ();
    ISR_Handler_SendIntensityData ();

} // ISR_Handler_StartNewFrame

//----------------------------------------------------------------------------
/*
 * Fill the MEMBLK with a fixed number of intensity values.
 */
void IRAM_ATTR c_OutputWS2801Spi::ISR_Handler_SendIntensityData ()
{
    uint32_t NumEmptyIntensitySlots = NumIntensityValuesPerInterrupt;

    while ((NumEmptyIntensitySlots--) && (MoreDataToSend))
    {
        uint8_t IntensityValue = GetNextIntensityToSend ();

        // convert the intensity data into SPI data
        for (uint8_t bitmask = 0x80; 0 != bitmask; bitmask >>= 1)
        {
        }
    } // end while there is space in the buffer

    // terminate the current data in the buffer

} // ISR_Handler_SendIntensityData

//----------------------------------------------------------------------------
void c_OutputWS2801Spi::Render ()
{
    // DEBUG_START;

    ISR_Handler_StartNewFrame ();
    ReportNewFrame ();

    // DEBUG_END;

} // render

#endif // def ARDUINO_ARCH_ESP32
