/*
* WS2811.cpp - WS2811 driver code for ESPixelStick
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

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../ESPixelStick.h"
#include "../FileIO.h"

#include "OutputWS2811.hpp"
#include "OutputCommon.hpp"

extern "C" {
#if defined(ARDUINO_ARCH_ESP8266)
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#else

// Define ESP8266 style macro conversions to limit changes in the rest of the code.
#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN
#   define WS2811_DATA_SPEED    (800000)

#endif
}

#ifndef UART_INV_MASK
#define UART_INV_MASK  (0x3f << 19)
#endif // ndef UART_INV_MASK

#ifndef UART_INTR_MASK
#define UART_INTR_MASK 0x1ff
#endif

//#define PIXEL_TXD  (GPIO_NUM_13)
#define PIXEL_RXD  (UART_PIN_NO_CHANGE)
#define PIXEL_RTS  (UART_PIN_NO_CHANGE)
#define PIXEL_CTS  (UART_PIN_NO_CHANGE)

#define WS2811_TIME_PER_PIXEL   30L     ///< 30us frame time
#define WS2811_MIN_IDLE_TIME    300L    ///< 300us idle time

// Depricated: Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO
// TX FIFO trigger level. 40 bytes gives 100us before the FIFO goes empty
// We need to fill the FIFO at a rate faster than 0.3us per byte (1.2us/pixel)
#define PIXEL_FIFO_TRIGGER_LEVEL (40)

/*
* Inverted 6N1 UART lookup table for ws2811, first 2 bits ignored.
* Start and stop bits are part of the pixel stream.
*/
char Convert2BitIntensityToUartDataStream[] = 
{
    0b00110111,     // 00 - (1)000 100(0)
    0b00000111,     // 01 - (1)000 111(0)
    0b00110100,     // 10 - (1)110 100(0)
    0b00000100      // 11 - (1)110 111(0)
};

// forward declaration for the isr handler
static void IRAM_ATTR uart_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputWS2811::c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId, gpio_num_t outputGpio, uart_port_t uart) :
    c_OutputCommon(OutputChannelId, outputGpio, uart),
    color_order ("rgb"),
    pixel_count (0),
    zig_size (0),
    group_size (1),
    gamma (1.0),
    brightness (1.0),
    pNextIntensityToSend (nullptr),
    RemainingIntensityCount (0),
    NumIntensityBytesInOutputBuffer (0),
    startTime (0),
    refreshTime (0),
    rOffset (0),
    gOffset (1),
    bOffset (2)
{
    // DEBUG_START;
    // DEBUG_END;
} // c_OutputWS2811

//----------------------------------------------------------------------------
c_OutputWS2811::~c_OutputWS2811()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    pinMode (DataPin, INPUT);

#ifdef ARDUINO_ARCH_ESP8266
    Serial1.end ();
#else

    // make sure no existing low level driver is running
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");

    ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
    // DEBUG_V ("");

#endif

    // Disable all interrupts for this uart. It is enabled by uart.c in the SDK
    CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);
    // DEBUG_V ("");

    // Clear all pending interrupts in the UART
    WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

    // DEBUG_END;
} // ~c_OutputWS2811

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811::Begin()
{
    // DEBUG_START;
    Serial.println (String (F (" Initializing WS281x on Chan: '")) + String (OutputChannelId) + "'");

    // are we using a valid config?
    if (gpio_num_t (-1) == DataPin) { return; }
    if (true == HasBeenInitialized) { return; } else { HasBeenInitialized = true; }

    // Set output pins
    pinMode(DataPin, OUTPUT);
    digitalWrite(DataPin, LOW);

    pixel_count = 600;

    // Setup Output Buffer
    NumIntensityBytesInOutputBuffer = pixel_count * WS2812_NUM_INTENSITY_BYTES_PER_PIXEL;
    memset((char*)(&OutputBuffer[0]), 0, sizeof(OutputBuffer));

    // Calculate our refresh time
    refreshTime = (WS2811_TIME_PER_PIXEL * pixel_count) + WS2811_MIN_IDLE_TIME;

    // Initialize for WS2811 via UART
#ifdef ARDUINO_ARCH_ESP8266
    // Serial rate is 4x 800KHz for WS2811
    Serial1.begin(3200000, SERIAL_6N1, SERIAL_TX_ONLY);
    CLEAR_PERI_REG_MASK (UART_CONF0(UartId), UART_INV_MASK);
    SET_PERI_REG_MASK   (UART_CONF0(UartId), (BIT(22)));

    // Clear FIFOs
    SET_PERI_REG_MASK   (UART_CONF0(UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK (UART_CONF0(UartId), UART_RXFIFO_RST | UART_TXFIFO_RST);

    // Disable all interrupts
    ETS_UART_INTR_DISABLE();

    // Atttach interrupt handler
    ETS_UART_INTR_ATTACH(uart_intr_handler, NULL);

    // Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO
    WRITE_PERI_REG(UART_CONF1(UartId), 80 << UART_TXFIFO_EMPTY_THRHD_S);

    // Disable RX & TX interrupts. It is enabled by uart.c in the SDK
    CLEAR_PERI_REG_MASK(UART_INT_ENA(UartId), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

    // Clear all pending interrupts in the UART
    WRITE_PERI_REG(UART_INT_CLR(UartId), UART_INTR_MASK);

    // Reenable interrupts
    ETS_UART_INTR_ENABLE();

#elif defined (ARDUINO_ARCH_ESP32)

    // In the ESP32 you need to be careful which CPU is being configured 
    // to handle interrupts. These API functions are supposed to handle this 
    // selection.

    // DEBUG_V (String ("UartId  = '") + UartId + "'");
    // DEBUG_V (String ("DataPin = '") + DataPin + "'");

    // make sure no existing low level ISR is running
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");

    ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
    // DEBUG_V ("");

    /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate           = WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE * WS2811_DATA_SPEED;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_6_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;

    // start the generic UART driver.
    // NOTE: Zero for RX buffer size causes errors in the uart API. 
    // Must be at least one byte larger than the fifo size
    // Do not set ESP_INTR_FLAG_IRAM here. the driver's ISR handler is not located in IRAM
    // DEBUG_V ("");
    // ESP_ERROR_CHECK (uart_driver_install (UartId, UART_FIFO_LEN+1, 0, 0, NULL, 0));
    // DEBUG_V ("");
    ESP_ERROR_CHECK (uart_param_config   (UartId, & uart_config));
    // DEBUG_V ("");
    ESP_ERROR_CHECK (uart_set_pin        (UartId, DataPin, PIXEL_RXD, PIXEL_RTS, PIXEL_CTS));

    // release the pre registered UART handler/subroutine and install out own
    // DEBUG_V ("");
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");
    // Disable ALL interrupts. It is enabled by uart.c in the SDK
    CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);
    // DEBUG_V ("");

    // Clear all pending interrupts in the UART
    // WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);
    // DEBUG_V ("");

    uart_isr_register (UartId, uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr);

    // DEBUG_END;
#endif
} // init

//----------------------------------------------------------------------------
void c_OutputWS2811::GetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    DEBUG_START;
    jsonConfig["color_order"] = color_order;
    jsonConfig["pixel_count"] = pixel_count;
    jsonConfig["group_size"]  = group_size;
    jsonConfig["zig_size"]    = zig_size;
    jsonConfig["gamma"]       = gamma;
    jsonConfig["brightness"]  = brightness;
    // enums need to be converted to uints for json
    jsonConfig["data_pin"]    = uint (DataPin);
    DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
// Fill the FIFO with as many intensity values as it can hold.
//void ICACHE_RAM_ATTR c_OutputWS2811::HandleWS2811Interrupt ()
void IRAM_ATTR c_OutputWS2811::HandleWS2811Interrupt ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        // Fill the FIFO with new data
        // free space in the FIFO divided by the number of data bytes per intensity 
        // gives the max number of intensities we can add to the FIFO
        register uint16_t IntensitySpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE;

        // determine how many intensities we can process right now.
        register uint16_t IntensitiesToSend = (IntensitySpaceInFifo < RemainingIntensityCount) ? (IntensitySpaceInFifo) : RemainingIntensityCount;

        // only read from ram once per intensity
        register uint8_t subpix = 0;

        // reduce the remaining count of intensities by the number we are sending now.
        RemainingIntensityCount -= IntensitiesToSend;

        // are there intensity values to send?
        while (0 != IntensitiesToSend--)
        {
            // read the next intensity value from the buffer and point at the next byte
            subpix = *pNextIntensityToSend++;

            // convert the intensity data into UART data
            enqueue ((Convert2BitIntensityToUartDataStream[(subpix >> 6) & 0x3]));
            enqueue ((Convert2BitIntensityToUartDataStream[(subpix >> 4) & 0x3]));
            enqueue ((Convert2BitIntensityToUartDataStream[(subpix >> 2) & 0x3]));
            enqueue ((Convert2BitIntensityToUartDataStream[(subpix >> 0) & 0x3]));
        }; // end send one or more intensity values

        // are we done?
        if (0 == RemainingIntensityCount)
        {
            // Disable ALL interrupts when done
            CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);
        }

        // Clear all interrupts flags for this uart
        WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

    } // end Our uart generated an interrupt
} // HandleWS2811Interrupt

//----------------------------------------------------------------------------
void c_OutputWS2811::Render()
{
    // DEBUG_START;

    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount)

    if (gpio_num_t (-1) == DataPin) { return; }
    if (!canRefresh ()) { return; }
    if (0 != RemainingIntensityCount) { return; }

    // set up pointers into the pixel data space
    uint8_t *pSourceData = GetBufferAddress();  // source buffer (owned by base class)
    uint8_t *pTargetData = OutputBuffer;        // target buffer

    // what type of copy are we making?
    if (!zig_size)
    {
        // Normal / group copy

        // for each destination pixel
        for (size_t CurrentDestinationPixelIndex = 0;
            CurrentDestinationPixelIndex < pixel_count;
            CurrentDestinationPixelIndex++)
        {
            // for each output pixel in the group (minimum of 1)
            for (size_t CurrentGroupIndex = 0;
                CurrentGroupIndex < group_size;
                ++CurrentGroupIndex, ++CurrentDestinationPixelIndex)
            {
                // write data to the output buffer
                *pTargetData++ = gamma_table[pSourceData[rOffset]];
                *pTargetData++ = gamma_table[pSourceData[gOffset]];
                *pTargetData++ = gamma_table[pSourceData[bOffset]];
            } // End for each intensity in current input pixel

            // point at the next pixel in the input buffer
            pSourceData += WS2812_NUM_INTENSITY_BYTES_PER_PIXEL;

        } // end for each pixel in the output buffer
    } // end normal copy
    else
    {
        // Zigzag copy
        for (size_t CurrentDestinationPixelIndex = 0;
            CurrentDestinationPixelIndex < pixel_count;
            CurrentDestinationPixelIndex++)
        {
            if (CurrentDestinationPixelIndex / zig_size % 2)
            {
                // Odd "zig"
                int group = zig_size * (CurrentDestinationPixelIndex / zig_size);
                pSourceData = GetBufferAddress () + (WS2812_NUM_INTENSITY_BYTES_PER_PIXEL * ((group + zig_size - (CurrentDestinationPixelIndex % zig_size) - 1) / group_size));
            } // end zig
            else
            {
                // Even "zag"
                pSourceData = GetBufferAddress () + (WS2812_NUM_INTENSITY_BYTES_PER_PIXEL * (CurrentDestinationPixelIndex / group_size));
            } // end zag

            // now that we have decided on a data source, copy one 
            // pixels worth of data
            *pTargetData++ = gamma_table[pSourceData[rOffset]];
            *pTargetData++ = gamma_table[pSourceData[gOffset]];
            *pTargetData++ = gamma_table[pSourceData[bOffset]];

        } // end for each pixel in the output buffer
    } // end zig zag copy

    // set the intensity transmit buffer pointer and number of intensities to send
    pNextIntensityToSend    = OutputBuffer;
    RemainingIntensityCount = NumIntensityBytesInOutputBuffer;
    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount);

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK(UART_INT_ENA(UartId), UART_TXFIFO_EMPTY_INT_ENA);
#else
    ESP_ERROR_CHECK (uart_enable_tx_intr (UartId, true, PIXEL_FIFO_TRIGGER_LEVEL));
#endif
    startTime = micros();

    // DEBUG_END;

} // render

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputWS2811::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    DEBUG_START;
    uint temp;
    FileIO::setFromJSON (color_order, jsonConfig["color_order"]);
    FileIO::setFromJSON (pixel_count, jsonConfig["pixel_count"]);
    FileIO::setFromJSON (group_size, jsonConfig["group_size"]);
    FileIO::setFromJSON (zig_size, jsonConfig["zig_size"]);
    FileIO::setFromJSON (gamma, jsonConfig["gamma"]);
    FileIO::setFromJSON (brightness, jsonConfig["brightness"]);
    // enums need to be converted to uints for json
    temp = uint (DataPin);
    FileIO::setFromJSON (temp, jsonConfig["data_pin"]);
    DataPin = gpio_num_t (temp);

    DEBUG_END;
    return validate ();

} // SetConfig

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2811*>(param)->HandleWS2811Interrupt ();
} // uart_intr_handler

//----------------------------------------------------------------------------
void c_OutputWS2811::updateGammaTable ()
{
    DEBUG_START;
    for (int i = 0; i < 256; i++)
    {
        gamma_table[i] = (uint8_t)min ((255.0 * pow (i * brightness / 255.0, gamma) + 0.5), 255.0);
    }
    DEBUG_END;
} // updateGammaTable

//----------------------------------------------------------------------------
void c_OutputWS2811::updateColorOrder ()
{
    DEBUG_START;
    // make sure the color order is all lower case
    color_order.toLowerCase ();

    if (String ("grb") == color_order) { rOffset = 1; gOffset = 0; bOffset = 2; }
    else if (String ("brg") == color_order) { rOffset = 1; gOffset = 2; bOffset = 0; }
    else if (String ("rbg") == color_order) { rOffset = 0; gOffset = 2; bOffset = 1; }
    else if (String ("gbr") == color_order) { rOffset = 2; gOffset = 0; bOffset = 1; }
    else if (String ("bgr") == color_order) { rOffset = 2; gOffset = 1; bOffset = 0; }
    else
    {
        color_order = "rgb";
        rOffset = 0; gOffset = 1; bOffset = 2;
    } // default

    DEBUG_END;
} // updateColorOrder
 
//----------------------------------------------------------------------------
/*
*   Validate that the current values meet our needs
*
*   needs
*       data set in the class elements
*   returns
*       true - no issues found
*       false - had an issue and had to fix things
*/
bool c_OutputWS2811::validate ()
{
    DEBUG_START;
    bool response = true;

    if (pixel_count > WS2812_PIXEL_LIMIT)
    {
        // LOG_PORT.println (String (F ("*** Requested pixel count was too high. Setting to ")) + PIXEL_LIMIT + F(" ***"));
        pixel_count = WS2812_PIXEL_LIMIT;
        response = false;
    }
    else if (pixel_count < 1)
    {
        pixel_count = 170;
        response = false;
    }

    if (group_size > pixel_count)
    {
        LOG_PORT.println (String (F ("*** Requested group size count was too high. Setting to ")) + pixel_count + F (" ***"));
        group_size = pixel_count;
        response = false;
    }
    else if (group_size < 1)
    {
        group_size = 1;
        response = false;
    }

    if (zig_size > pixel_count)
    {
        zig_size = pixel_count;
        response = false;
    }
    else if (zig_size < 1)
    {
        zig_size = 1;
        response = false;
    }

    // Default gamma value
    if (gamma <= 0)
    {
        gamma = 2.2;
        response = false;
    }

    // Default brightness value
    if (brightness <= 0)
    {
        brightness = 1.0;
        response = false;
    }

    updateGammaTable ();
    updateColorOrder ();

    // Calculate our refresh time
    refreshTime = (WS2811_TIME_PER_PIXEL * pixel_count) + WS2811_MIN_IDLE_TIME;

    DEBUG_END;
    return response;

} // validate

