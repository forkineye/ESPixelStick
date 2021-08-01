/*
* WS2811Uart.cpp - WS2811 driver code for ESPixelStick UART
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

#include "OutputWS2811Uart.hpp"

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
#define PIXEL_FIFO_TRIGGER_LEVEL (16)

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
c_OutputWS2811Uart::c_OutputWS2811Uart (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon (OutputChannelId, outputGpio, uart, outputType),
    color_order ("rgb"),
    pixel_count (100),
    zig_size (0),
    group_size (1),
    gamma (1.0),
    brightness (100),
    pNextIntensityToSend (nullptr),
    RemainingPixelCount (0),
    numIntensityBytesPerPixel (3),
    InterFrameGapInMicroSec (WS2811_MIN_IDLE_TIME)
{
    // DEBUG_START;
    ColorOffsets.offset.r = 0;
    ColorOffsets.offset.g = 1;
    ColorOffsets.offset.b = 2;
    ColorOffsets.offset.w = 3;

    // DEBUG_END;
} // c_OutputWS2811Uart

//----------------------------------------------------------------------------
c_OutputWS2811Uart::~c_OutputWS2811Uart ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    // Disable all interrupts for this uart.
    CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_INTR_MASK);
    // DEBUG_V ("");

    // Clear all pending interrupts in the UART
    WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP8266
    Serial1.end ();
#else

    // make sure no existing low level driver is running
    ESP_ERROR_CHECK (uart_disable_tx_intr (UartId));
    // DEBUG_V ("");

    ESP_ERROR_CHECK (uart_disable_rx_intr (UartId));
    // DEBUG_V (String("UartId: ") + String(UartId));

    // todo: put back uart_isr_free (UartId);

#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // ~c_OutputWS2811Uart

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2811Uart*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811Uart::Begin ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    InitializeUart (WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE * WS2811_DATA_SPEED,
        SERIAL_6N1,
        SERIAL_TX_ONLY,
        PIXEL_FIFO_TRIGGER_LEVEL);
#else
        /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate = WS2812_NUM_DATA_BYTES_PER_INTENSITY_BYTE * WS2811_DATA_SPEED;
    uart_config.data_bits = uart_word_length_t::UART_DATA_6_BITS;
    uart_config.flow_ctrl = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick = false;
    InitializeUart (uart_config, PIXEL_FIFO_TRIGGER_LEVEL);
#endif

    // Atttach interrupt handler
#ifdef ARDUINO_ARCH_ESP8266
    ETS_UART_INTR_ATTACH (uart_intr_handler, this);
#else
    uart_isr_register (UartId, uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr);
#endif

    // invert the output
    CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_INV_MASK);
    SET_PERI_REG_MASK (UART_CONF0 (UartId), (BIT (22)));

} // init

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    jsonConfig[CN_color_order] = color_order;
    jsonConfig[CN_pixel_count] = pixel_count;
    jsonConfig[CN_group_size] = group_size;
    jsonConfig[CN_zig_size] = zig_size;
    jsonConfig[CN_gamma] = gamma;
    jsonConfig[CN_brightness] = brightness; // save as a 0 - 100 percentage
    jsonConfig[CN_interframetime] = InterFrameGapInMicroSec;

    c_OutputCommon::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputCommon::GetStatus (jsonStatus);
    // uint32_t UartIntSt = GET_PERI_REG_MASK (UART_INT_ST (UartId), UART_TXFIFO_EMPTY_INT_ENA);
    // uint16_t SpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getWS2811FifoLength));

    // jsonStatus["pNextIntensityToSend"] = uint32_t(pNextIntensityToSend);
    // jsonStatus["RemainingIntensityCount"] = uint32_t(RemainingIntensityCount);
    // jsonStatus["UartIntSt"] = UartIntSt;
    // jsonStatus["SpaceInFifo"] = SpaceInFifo;

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::SetOutputBufferSize (uint16_t NumChannelsAvailable)
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

        // Calculate our refresh time
        FrameMinDurationInMicroSec = (WS2811_MICRO_SEC_PER_INTENSITY * NumChannelsAvailable) + InterFrameGapInMicroSec;

        } while (false);

        // DEBUG_END;
} // SetBufferSize

//----------------------------------------------------------------------------
/*
     * Fill the FIFO with as many intensity values as it can hold.
     */
void IRAM_ATTR c_OutputWS2811Uart::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        // Fill the FIFO with new data
        // free space in the FIFO divided by the number of data bytes per intensity
        // gives the max number of intensities we can add to the FIFO
        uint32_t NumEmptyPixelSlots = ((((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / WS2812_NUM_DATA_BYTES_PER_PIXEL);
        while (NumEmptyPixelSlots--)
        {
            for (uint8_t CurrentIntensityIndex = 0;
                CurrentIntensityIndex < numIntensityBytesPerPixel;
                CurrentIntensityIndex++)
            {
                uint8_t IntensityValue = (pNextIntensityToSend[ColorOffsets.Array[CurrentIntensityIndex]]);

                IntensityValue = gamma_table[IntensityValue];

                // adjust intensity
                // IntensityValue = uint8_t( (uint32_t(IntensityValue) * uint32_t(AdjustedBrightness)) >> 8);

                // convert the intensity data into UART data
                enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 6) & 0x3]));
                enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 4) & 0x3]));
                enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 2) & 0x3]));
                enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 0) & 0x3]));
            }

            // has the group completed?
            if (--CurrentGroupPixelCount)
            {
                // not finished with the group yet
                continue;
            }
            // refresh the group count
            CurrentGroupPixelCount = GroupPixelCount;

            --RemainingPixelCount;
            if (0 == RemainingPixelCount)
            {
                CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
                break;
            }

            // have we completed the forward traverse
            if (CurrentZigPixelCount)
            {
                --CurrentZigPixelCount;
                // not finished with the set yet.
                pNextIntensityToSend += numIntensityBytesPerPixel;
                continue;
            }

            if (CurrentZagPixelCount == ZigPixelCount)
            {
                // first backward pixel
                pNextIntensityToSend += numIntensityBytesPerPixel * (ZigPixelCount + 1);
            }

            // have we completed the backward traverse
            if (CurrentZagPixelCount)
            {
                --CurrentZagPixelCount;
                // not finished with the set yet.
                pNextIntensityToSend -= numIntensityBytesPerPixel;
                continue;
            }

            // move to next forward pixel
            pNextIntensityToSend += numIntensityBytesPerPixel * (ZigPixelCount);

            // refresh the zigZag
            CurrentZigPixelCount = ZigPixelCount - 1;
            CurrentZagPixelCount = ZigPixelCount;
        }

        // Clear all interrupts flags for this uart
        WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

    } // end Our uart generated an interrupt

} // HandleWS2811Interrupt

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::Render ()
{
    // DEBUG_START;

    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount)

    if (gpio_num_t (-1) == DataPin) { return; }
    if (!canRefresh ()) { return; }

    // get the next frame started
    CurrentZigPixelCount   = ZigPixelCount - 1;
    CurrentZagPixelCount   = ZigPixelCount;
    CurrentGroupPixelCount = GroupPixelCount;
    pNextIntensityToSend   = GetBufferAddress ();
    RemainingPixelCount    = pixel_count;

    // enable interrupts
    WRITE_PERI_REG (UART_CONF1 (UartId), PIXEL_FIFO_TRIGGER_LEVEL << UART_TXFIFO_EMPTY_THRHD_S);
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

    ReportNewFrame ();

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
bool c_OutputWS2811Uart::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    // enums need to be converted to uints for json
    setFromJSON (color_order, jsonConfig, CN_color_order);
    setFromJSON (pixel_count, jsonConfig, CN_pixel_count);
    setFromJSON (group_size, jsonConfig, CN_group_size);
    setFromJSON (zig_size, jsonConfig, CN_zig_size);
    setFromJSON (gamma, jsonConfig, CN_gamma);
    setFromJSON (brightness, jsonConfig, CN_brightness);
    setFromJSON (InterFrameGapInMicroSec, jsonConfig, CN_interframetime);

    c_OutputCommon::SetConfig (jsonConfig);

    bool response = validate ();

    AdjustedBrightness = map (brightness, 0, 100, 0, 255);
    // DEBUG_V (String ("brightness: ") + String (brightness));
    // DEBUG_V (String ("AdjustedBrightness: ") + String (AdjustedBrightness));

    updateGammaTable ();
    updateColorOrderOffsets ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    ZigPixelCount   = (2 > zig_size)   ? pixel_count : zig_size;
    GroupPixelCount = (2 > group_size) ? 1           : group_size;

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::updateGammaTable ()
{
    // DEBUG_START;
    double tempBrightness = double (brightness) / 100.0;
    // DEBUG_V (String ("tempBrightness: ") + String (tempBrightness));

    for (int i = 0; i < sizeof(gamma_table); ++i)
    {
        // ESP.wdtFeed ();
        gamma_table[i] = (uint8_t)min ((255.0 * pow (i * tempBrightness / 255, gamma) + 0.5), 255.0);
        // DEBUG_V (String ("i: ") + String (i));
        // DEBUG_V (String ("gamma_table[i]: ") + String (gamma_table[i]));
    }

    // DEBUG_END;
} // updateGammaTable

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::updateColorOrderOffsets ()
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
bool c_OutputWS2811Uart::validate ()
{
    // DEBUG_START;
    bool response = true;

    if (zig_size > pixel_count)
    {
        LOG_PORT.println (String (F ("*** Requested ZigZag size count was too high. Setting to ")) + pixel_count + F (" ***"));
        zig_size = pixel_count;
        response = false;
    }

    // Default gamma value
    if (gamma <= 0)
    {
        gamma = 2.2;
        response = false;
    }

    // Max brightness value
    if (brightness > 100)
    {
        brightness = 100;
        // DEBUG_V (String ("brightness: ") + String (brightness));
        response = false;
    }

    // DEBUG_END;
    return response;

} // validate

//----------------------------------------------------------------------------
void c_OutputWS2811Uart::PauseOutput ()
{
    // DEBUG_START;

    CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);

    // DEBUG_END;
} // PauseOutput
