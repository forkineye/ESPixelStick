/*
* OutputWS2811Rmt.cpp - WS2811 driver code for ESPixelStick RMT Channel
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

#include "OutputWS2811Rmt.hpp"

#if defined(ARDUINO_ARCH_ESP8266)
#elif defined(ARDUINO_ARCH_ESP32)

#endif

// forward declaration for the isr handler
static void IRAM_ATTR uart_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::c_OutputWS2811Rmt (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputWS2811 (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputWS2811Rmt

//----------------------------------------------------------------------------
c_OutputWS2811Rmt::~c_OutputWS2811Rmt ()
{
    // DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    // Disable all interrupts for this RMT Channel.
    // DEBUG_V ("");

    // Clear all pending interrupts in the RMT Channel
    // DEBUG_V ("");

    // make sure no existing low level driver is running
    // DEBUG_V ("");

    // DEBUG_V (String("UartId: ") + String(UartId));

    // DEBUG_END;
} // ~c_OutputWS2811Rmt

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputWS2811Rmt*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputWS2811Rmt::Begin ()
{
    // DEBUG_START;

    // Configure RMT channel

    // Atttach interrupt handler

    // Start output

} // init

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::SetOutputBufferSize (uint16_t NumChannelsAvailable)
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

        // notify the base class
        c_OutputWS2811::SetOutputBufferSize (NumChannelsAvailable);

    } while (false);

    // DEBUG_END;
} // SetBufferSize

//----------------------------------------------------------------------------
/*
     * Fill the FIFO with as many intensity values as it can hold.
     */
void IRAM_ATTR c_OutputWS2811Rmt::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    // if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        // Fill the FIFO with new data
        // free space in the FIFO divided by the number of data bytes per intensity
        // gives the max number of intensities we can add to the FIFO
        uint32_t NumEmptyPixelSlots = 0; //  ((((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / WS2812_NUM_DATA_BYTES_PER_PIXEL);
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
                // enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 6) & 0x3]));
                // enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 4) & 0x3]));
                // enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 2) & 0x3]));
                // enqueue ((Convert2BitIntensityToUartDataStream[(IntensityValue >> 0) & 0x3]));
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
                // CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
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
        // WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

    } // end Our uart generated an interrupt

} // HandleWS2811Interrupt

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::Render ()
{
    // DEBUG_START;

    // DEBUG_V (String ("RemainingIntensityCount: ") + RemainingIntensityCount)

    if (gpio_num_t (-1) == DataPin) { return; }
    if (!canRefresh ()) { return; }

    // get the next frame started
    CurrentZigPixelCount = ZigPixelCount - 1;
    CurrentZagPixelCount = ZigPixelCount;
    CurrentGroupPixelCount = GroupPixelCount;
    pNextIntensityToSend = GetBufferAddress ();
    RemainingPixelCount = pixel_count;

    // enable output
    // TODO - Something

    ReportNewFrame ();

    // DEBUG_END;

} // render

//----------------------------------------------------------------------------
void c_OutputWS2811Rmt::PauseOutput ()
{
    // DEBUG_START;

    // TODO - Something

    // DEBUG_END;
} // PauseOutput
