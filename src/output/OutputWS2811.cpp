/*
* WS2811.cpp - WS2811 driver code for ESPixelStick
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

#include "OutputWS2811.hpp"

#if defined(ARDUINO_ARCH_ESP8266)
extern "C" {
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
}
#elif defined(ARDUINO_ARCH_ESP32)

// Define ESP8266 style macro conversions to limit changes in the rest of the code.
#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN
#endif

#define WS2811_DATA_SPEED    (800000)

#ifndef UART_INV_MASK
#   define UART_INV_MASK  (0x3f << 19)
#endif // ndef UART_INV_MASK

#define WS2811_MICRO_SEC_PER_INTENSITY  10L     // ((1/800000) * 8 bits) = 10us
#define WS2811_MIN_IDLE_TIME            300L    ///< 300us idle time

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
c_OutputWS2811::c_OutputWS2811 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon (OutputChannelId, outputGpio, uart, outputType),
    color_order ("rgb"),
    pixel_count (100),
    zig_size (0),
    group_size (1),
    gamma (1.0),
    brightness (1.0),
    pNextIntensityToSend (nullptr),
    RemainingIntensityCount (0),
    numIntensityBytesPerPixel(3),
    InterFrameGapInMicroSec(WS2811_MIN_IDLE_TIME)
{
    // DEBUG_START;
    ColorOffsets.offset.r = 0;
    ColorOffsets.offset.g = 1;
    ColorOffsets.offset.b = 2;
    ColorOffsets.offset.w = 3;
    // DEBUG_END;
} // c_OutputWS2811

//----------------------------------------------------------------------------
c_OutputWS2811::~c_OutputWS2811()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

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
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP32
    // uart_isr_free (UartId);
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // ~c_OutputWS2811

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2811*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811::Begin()
{
    // DEBUG_START;
    LOG_PORT.println (String (F ("** WS281x Initialization for Chan: ")) + String (OutputChannelId) + " **");

#ifdef ARDUINO_ARCH_ESP8266
    InitializeUart (WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE * WS2811_DATA_SPEED,
                    SERIAL_6N1,
                    SERIAL_TX_ONLY,
                    PIXEL_FIFO_TRIGGER_LEVEL);
#else
        /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate           = WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE * WS2811_DATA_SPEED;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_6_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;
    InitializeUart (uart_config, PIXEL_FIFO_TRIGGER_LEVEL);
#endif

    // Atttach interrupt handler
#ifdef ARDUINO_ARCH_ESP8266
    ETS_UART_INTR_ATTACH (uart_intr_handler, this);
#else
    uart_isr_register (UartId, uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr);
#endif

    // invert the output
    CLEAR_PERI_REG_MASK (UART_CONF0(UartId), UART_INV_MASK);
    SET_PERI_REG_MASK   (UART_CONF0(UartId), (BIT(22)));

} // init

//----------------------------------------------------------------------------
void c_OutputWS2811::GetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[F ("color_order")]    = color_order;
    jsonConfig[F ("pixel_count")]    = pixel_count;
    jsonConfig[F ("group_size")]     = group_size;
    jsonConfig[F ("zig_size")]       = zig_size;
    jsonConfig[F ("gamma")]          = gamma;
    jsonConfig[F ("brightness")]     = uint8_t(brightness * 100.0); // save as a 1 - 100 percentage
    jsonConfig[F ("interframetime")] = InterFrameGapInMicroSec;
    // enums need to be converted to uints for json
    jsonConfig[F ("data_pin")]       = uint (DataPin);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811::SetOutputBufferSize (uint16_t NumChannelsAvailable)
{
    // DEBUG_START;
    // DEBUG_V (String ("NumChannelsAvailable: ") + String (NumChannelsAvailable));
    // DEBUG_V (String ("   GetBufferUsedSize: ") + String (c_OutputCommon::GetBufferUsedSize ()));
    // DEBUG_V (String ("         pixel_count: ") + String (pixel_count));
    // DEBUG_V (String ("       BufferAddress: ") + String ((uint32_t)(c_OutputCommon::GetBufferAddress ())));

    do // once
    {
        // are we changing size?
        if (NumChannelsAvailable == OutputBufferSize)
        {
            // DEBUG_V ("NO Need to change the ISR buffer");
            break;
        }

        // DEBUG_V ("Need to change the ISR buffer");

        // Stop current output operation
#ifdef ARDUINO_ARCH_ESP8266
        CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
#else
        ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
#endif
        c_OutputCommon::SetOutputBufferSize (NumChannelsAvailable);

        if (nullptr != pIsrOutputBuffer)
        {
            // DEBUG_V ("Free ISR Buffer");
            free (pIsrOutputBuffer);
            pIsrOutputBuffer = nullptr;
        }

        if (0 == NumChannelsAvailable)
        {
            // DEBUG_V ("Dont need an ISR Buffer");
            break;
        }

        if (nullptr == (pIsrOutputBuffer = (uint8_t*)malloc (NumChannelsAvailable)))
        {
            // DEBUG_V ("malloc failed");
            LOG_PORT.println ("ERROR: WS2811 driver failed to allocate an IsrOutputBuffer. Shutting down output.");
            c_OutputCommon::SetOutputBufferSize ((uint16_t)0);
            FrameRefreshTimeInMicroSec = InterFrameGapInMicroSec;
            break;
        }

        // DEBUG_V ("malloc ok");

        memset (pIsrOutputBuffer, 0x0, NumChannelsAvailable);
        // Calculate our refresh time
        FrameRefreshTimeInMicroSec = (WS2811_MICRO_SEC_PER_INTENSITY * NumChannelsAvailable) + InterFrameGapInMicroSec;

    } while (false);

    // DEBUG_END;
} // SetBufferSize

//----------------------------------------------------------------------------
/*
 * Fill the FIFO with as many intensity values as it can hold.
 */
void IRAM_ATTR c_OutputWS2811::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        // Fill the FIFO with new data
        // free space in the FIFO divided by the number of data bytes per intensity
        // gives the max number of intensities we can add to the FIFO
        uint16_t IntensitySpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE;

        // determine how many intensities we can process right now.
        uint16_t IntensitiesToSend = (IntensitySpaceInFifo < RemainingIntensityCount) ? (IntensitySpaceInFifo) : RemainingIntensityCount;

        // only read from ram once per intensity
        uint8_t subpix = 0;

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
    if (nullptr == pIsrOutputBuffer) { return; }

    // set up pointers into the pixel data space
    uint8_t *pSourceData = pOutputBuffer; // source buffer (owned by base class)
    uint8_t *pTargetData = pIsrOutputBuffer;              // target buffer
    uint16_t OutputPixelCount = OutputBufferSize / numIntensityBytesPerPixel;

    // what type of copy are we making?
    if (!zig_size)
    {
        // Normal / group copy

        // for each destination pixel
        size_t CurrentDestinationPixelIndex = 0;
        while (CurrentDestinationPixelIndex < OutputPixelCount)
        {
            // for each output pixel in the group (minimum of 1)
            for (size_t CurrentGroupIndex = 0;
                CurrentGroupIndex < group_size;
                ++CurrentGroupIndex, ++CurrentDestinationPixelIndex)
            {
                // write data to the output buffer
                for (int colorOffsetId = 0; colorOffsetId < numIntensityBytesPerPixel; ++colorOffsetId)
                {
                    *pTargetData++ = gamma_table[pSourceData[ColorOffsets.Array[colorOffsetId]]];
                }
            } // End for each intensity in current input pixel

            // point at the next pixel in the input buffer
            pSourceData += numIntensityBytesPerPixel;

        } // end for each pixel in the output buffer

    } // end normal copy
    else
    {
        // Zigzag copy
        for (size_t CurrentDestinationPixelIndex = 0;
            CurrentDestinationPixelIndex < OutputPixelCount;
            CurrentDestinationPixelIndex++)
        {
            if (CurrentDestinationPixelIndex / zig_size % 2)
            {
                // Odd "zig"
                int group = zig_size * (CurrentDestinationPixelIndex / zig_size);
                pSourceData = pOutputBuffer + (numIntensityBytesPerPixel * ((group + zig_size - (CurrentDestinationPixelIndex % zig_size) - 1) / group_size));
            } // end zig
            else
            {
                // Even "zag"
                pSourceData = pOutputBuffer + (numIntensityBytesPerPixel * (CurrentDestinationPixelIndex / group_size));
            } // end zag

            // now that we have decided on a data source, copy one
            // pixels worth of data
                // write data to the output buffer
            for (int colorOffsetId = 0; colorOffsetId < numIntensityBytesPerPixel; ++colorOffsetId)
            {
                *pTargetData++ = gamma_table[pSourceData[ColorOffsets.Array[colorOffsetId]]];
            }
        } // end for each pixel in the output buffer
    } // end zig zag copy

    // set the intensity transmit buffer pointer and number of intensities to send
    pNextIntensityToSend    = pIsrOutputBuffer;
    RemainingIntensityCount = OutputBufferSize;
    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount);

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK(UART_INT_ENA(UartId), UART_TXFIFO_EMPTY_INT_ENA);
#else
//     (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UART_NUM_0)))) = (uint32_t)('7');
    ESP_ERROR_CHECK (uart_enable_tx_intr (UartId, 1, PIXEL_FIFO_TRIGGER_LEVEL));
#endif
    FrameStartTimeInMicroSec = micros();

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
bool c_OutputWS2811::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    uint tempDataPin = uint (DataPin);

    setFromJSON (color_order,             jsonConfig, F ("color_order"));
    setFromJSON (pixel_count,             jsonConfig, F ("pixel_count"));
    setFromJSON (group_size,              jsonConfig, F ("group_size"));
    setFromJSON (zig_size,                jsonConfig, F ("zig_size"));
    setFromJSON (gamma,                   jsonConfig, F ("gamma"));
    setFromJSON (brightness,              jsonConfig, F ("brightness"));
    setFromJSON (InterFrameGapInMicroSec, jsonConfig, F ("interframetime"));
    setFromJSON (tempDataPin,             jsonConfig, F ("data_pin"));
    DataPin = gpio_num_t (tempDataPin);

    // DEBUG_V (String ("brightness: ") + String (brightness));
    brightness /= 100.0; // turn into a 0-1.0 multiplier
    // DEBUG_V (String ("brightness: ") + String (brightness));

    bool response = validate ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811::updateGammaTable ()
{
    // DEBUG_START;
    for (int i = 0; i < 256; i++)
    {
        gamma_table[i] = (uint8_t)min ((255.0 * pow (i * brightness / 255.0, gamma) + 0.5), 255.0);
    }
    // DEBUG_END;
} // updateGammaTable

//----------------------------------------------------------------------------
void c_OutputWS2811::updateColorOrderOffsets ()
{
    // DEBUG_START;
    // make sure the color order is all lower case
    color_order.toLowerCase ();

         if (String (F ("rgbw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("grbw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("brgw")) == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("rbgw")) == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("gbrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("bgrw")) == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 4; }
    else if (String (F ("grb"))  == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 2; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("brg"))  == color_order) { ColorOffsets.offset.r = 1; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("rbg"))  == color_order) { ColorOffsets.offset.r = 0; ColorOffsets.offset.g = 2; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("gbr"))  == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 0; ColorOffsets.offset.b = 1; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else if (String (F ("bgr"))  == color_order) { ColorOffsets.offset.r = 2; ColorOffsets.offset.g = 1; ColorOffsets.offset.b = 0; ColorOffsets.offset.w = 3; numIntensityBytesPerPixel = 3; }
    else
    {
        color_order = F ("rgb");
        ColorOffsets.offset.r = 0;
        ColorOffsets.offset.g = 1;
        ColorOffsets.offset.b = 2;
        ColorOffsets.offset.w = 3;
        numIntensityBytesPerPixel = 3;
    } // default

    // DEBUG_END;
} // updateColorOrderOffsets

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
    // DEBUG_START;
    bool response = true;

    if (group_size > pixel_count)
    {
        LOG_PORT.println (String (F ("*** Requested group size count was too high. Setting to ")) + pixel_count + F (" ***"));
        group_size = pixel_count;
        response = false;
    }
    else if (group_size < 1)
    {
        LOG_PORT.println (String (F ("*** Requested group size count was too low. Setting to 1 ***")));
        group_size = 1;
        response = false;
    }

    if (zig_size > pixel_count)
    {
        LOG_PORT.println (String (F ("*** Requested ZigZag size count was too high. Setting to ")) + pixel_count + F (" ***"));
        zig_size = pixel_count;
        response = false;
    }
    else if (zig_size < 0)
    {
        LOG_PORT.println (String (F ("*** Requested ZigZag size count was too low. Setting to 0 ***")));
        zig_size = 0;
        response = false;
    }

    // Default gamma value
    if (gamma <= 0)
    {
        gamma = 2.2;
        response = false;
    }

    // Default brightness value
    if (brightness <= 0.0)
    {
        brightness = 1.0;
        // DEBUG_V (String ("brightness: ") + String (brightness));
        response = false;
    }

    updateGammaTable ();
    updateColorOrderOffsets ();

    // DEBUG_END;
    return response;

} // validate

