/*
* GECE.cpp - GECE driver code for ESPixelStick
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
#include "OutputGECE.hpp"
#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif


extern "C" {
#ifdef ARDUINO_ARCH_ESP8266
#   include <eagle_soc.h>
#   include <ets_sys.h>
#   include <uart.h>
#   include <uart_register.h>
#elif defined(ARDUINO_ARCH_ESP32)
#   include <esp32-hal-uart.h>
#   include <soc/soc.h>
#   include <soc/uart_reg.h>
#   include <rom/ets_sys.h>
#   include <driver/uart.h>

#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define SERIAL_TX_ONLY       UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN

#endif
}

/*
* 7N1 UART lookup table for GECE, first bit is ignored.
* Start bit and stop bits are part of the packet.
* Bits are backwards since we need MSB out.
*/

#define GECE_DATA_ZERO 0b01111110
#define GECE_DATA_ONE  0b01000000

static char LOOKUP_GECE[] =
{
    GECE_DATA_ZERO,    // 0 - (0)01 111 11(1)
    GECE_DATA_ONE      // 1 - (0)00 000 01(1)
};

/*
output looks like this

Start bit = High for 8us
26 data bits.
    Each bit is 30us
    0 = 6 us low, 25 us high
    1 = 23 us low, 6 us high
stop bit = low for at least 45us
*/

#define GECE_uSec_PER_GECE_BIT          31.0
#define GECE_uSec_PER_GECE_START_BIT    8.0
#define GECE_UART_BITS_PER_GECE_BIT     (1.0 + 7.0 + 1.0)
#define GECE_UART_uSec_PER_BIT          (GECE_uSec_PER_GECE_BIT / GECE_UART_BITS_PER_GECE_BIT)
#define GECE_BAUDRATE                   uint32_t( (1.0/(GECE_UART_uSec_PER_BIT / 1000000))   )

#define GECE_FRAME_TIME                 (GECE_PACKET_SIZE * GECE_uSec_PER_GECE_BIT) /* 790us packet frame time */
#define GECE_IDLE_TIME                  (45 + GECE_uSec_PER_GECE_BIT)               /* 45us not 30us idle time */

#define CPU_ClockTimeNS                 ((1.0 / float(F_CPU)) * 1000000000)
#define GECE_CCOUNT_IDLETIME            uint32_t((GECE_IDLE_TIME * 1000) / CPU_ClockTimeNS)
#define GECE_CCOUNT_STARTBIT            uint32_t((GECE_uSec_PER_GECE_START_BIT * 1000) / CPU_ClockTimeNS) // 10us (min) start bit

#define GECE_NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define GECE_BITS_PER_INTENSITY                 4
#define GECE_BITS_BRIGHTNESS                    8
#define GECE_BITS_ADDRESS                       6
#define GECE_OVERHEAD_BITS                      (GECE_BITS_BRIGHTNESS + GECE_BITS_ADDRESS)
#define GECE_PACKET_SIZE                        ((GECE_NUM_INTENSITY_BYTES_PER_PIXEL * GECE_BITS_PER_INTENSITY) + GECE_OVERHEAD_BITS) //   26

// frame layout: 0x0AAIIBGR (26 bits)
#define GECE_ADDRESS_MASK       0x03F00000
#define GECE_ADDRESS_SHIFT      20

#define GECE_INTENSITY_MASK     0x000FF000
#define GECE_INTENSITY_SHIFT    12

#define GECE_BLUE_MASK          0x00000F00
#define GECE_BLUE_SHIFT         8

#define GECE_GREEN_MASK         0x000000F0
#define GECE_GREEN_SHIFT        0

#define GECE_RED_MASK           0x0000000F
#define GECE_RED_SHIFT          4

#define GECE_SET_ADDRESS(value)     ((uint32_t(value) << GECE_ADDRESS_SHIFT)   & GECE_ADDRESS_MASK)
#define GECE_SET_BRIGHTNESS(value)  ((uint32_t(value) << GECE_INTENSITY_SHIFT) & GECE_INTENSITY_MASK)
#define GECE_SET_BLUE(value)        ((uint32_t(value) << GECE_BLUE_SHIFT)      & GECE_BLUE_MASK)
#define GECE_SET_GREEN(value)       ((uint32_t(value)                   )      & GECE_GREEN_MASK)
#define GECE_SET_RED(value)         ((uint32_t(value) >> GECE_RED_SHIFT )      & GECE_RED_MASK)

// forward declaration for the isr handler
static void IRAM_ATTR uart_intr_handler (void* param);

//----------------------------------------------------------------------------
c_OutputGECE::c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                            gpio_num_t outputGpio,
                            uart_port_t uart,
                            c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    brightness = GECE_DEFAULT_BRIGHTNESS;

    // DEBUG_END;

} // c_OutputGECE

//----------------------------------------------------------------------------
c_OutputGECE::~c_OutputGECE ()
{
    // DEBUG_START;
    // DEBUG_END;
} // ~c_OutputGECE

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR uart_intr_handler (void* param)
{
    reinterpret_cast <c_OutputGECE*>(param)->ISR_Handler ();
} // uart_intr_handler

//----------------------------------------------------------------------------
void c_OutputGECE::Begin()
{
    // DEBUG_START;

    if (gpio_num_t (-1) == DataPin) { return; }

    SetOutputBufferSize (pixel_count * GECE_NUM_INTENSITY_BYTES_PER_PIXEL);

    // DEBUG_V (String ("GECE_BAUDRATE: ") + String (GECE_BAUDRATE));

    // Serial rate is 3x 100KHz for GECE
#ifdef ARDUINO_ARCH_ESP8266
    InitializeUart (GECE_BAUDRATE,
                    SERIAL_7N1,
                    SERIAL_TX_ONLY,
                    0);
#else
    uart_config_t uart_config;
    uart_config.baud_rate           = GECE_BAUDRATE;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_7_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;
    InitializeUart (uart_config, 0);
#endif

    // Atttach interrupt handler
#ifdef ARDUINO_ARCH_ESP8266
    ETS_UART_INTR_ATTACH (uart_intr_handler, this);
#else
    uart_isr_register (UartId, uart_intr_handler, this, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr);
#endif

    // DEBUG_V (String ("GECE_CCOUNT_DELAY: ") + String (GECE_CCOUNT_DELAY));

    FrameMinDurationInMicroSec = min(uint32_t(25000), uint32_t((GECE_FRAME_TIME + GECE_IDLE_TIME) * pixel_count));
    // FrameMinDurationInMicroSec = GECE_FRAME_TIME + GECE_IDLE_TIME;
    // DEBUG_V (String ("FrameMinDurationInMicroSec: ") + String (FrameMinDurationInMicroSec));

    SET_PERI_REG_MASK(UART_CONF0(UartId), UART_TXD_BRK);
    // ReportNewFrame (); // starts a timeout that include IDLE time

    // DEBUG_END;
} // begin

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputGECE::SetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    uint temp;
    setFromJSON(pixel_count, jsonConfig, CN_pixel_count);

    temp = map(brightness, 0, 255, 0, 100);
    setFromJSON(temp,  jsonConfig, CN_brightness);
    brightness = map (temp, 0, 100, 0, 255);

    c_OutputCommon::SetConfig (jsonConfig);

    bool response = validate ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGECE::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;
    jsonConfig[CN_pixel_count] = pixel_count;
    jsonConfig[CN_brightness]  = map (brightness, 0, 255, 0, 100);

    c_OutputCommon::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
uint16_t c_OutputGECE::GetNumChannelsNeeded ()
{
    return pixel_count * GECE_NUM_INTENSITY_BYTES_PER_PIXEL;

} // GetNumChannelsNeeded

//----------------------------------------------------------------------------
bool c_OutputGECE::validate ()
{
    // DEBUG_START;

    bool response = true;

    if (pixel_count > GECE_PIXEL_LIMIT)
    {
        pixel_count = GECE_PIXEL_LIMIT;
        response = false;
    }
    else if (pixel_count < 1)
    {
        pixel_count = GECE_PIXEL_LIMIT;
        response = false;
    }

    // DEBUG_V (String ("pixel_count: ") + String (pixel_count));

    SetOutputBufferSize (pixel_count * GECE_NUM_INTENSITY_BYTES_PER_PIXEL);

    OutputFrame.CurrentPixelID = 0;
    OutputFrame.pCurrentInputData = pOutputBuffer;

    // DEBUG_END;

    return response;

} // validate

//----------------------------------------------------------------------------
/*
 * Fill the FIFO with as many intensity values as it can hold.
 */
void IRAM_ATTR c_OutputGECE::ISR_Handler ()
{
    // Process if the desired UART has raised an interrupt
    if (READ_PERI_REG (UART_INT_ST (UartId)))
    {
        uint32_t StartingCycleCount = _getCycleCount ();

        uint32_t packet = 0;

        // Build a GECE packet
        // DEBUG_V ("");
        packet |= GECE_SET_BRIGHTNESS (brightness);
        packet |= GECE_SET_ADDRESS (OutputFrame.CurrentPixelID);
        packet |= GECE_SET_RED     (OutputFrame.pCurrentInputData[0]);
        packet |= GECE_SET_GREEN   (OutputFrame.pCurrentInputData[1]);
        packet |= GECE_SET_BLUE    (OutputFrame.pCurrentInputData[2]);
        OutputFrame.pCurrentInputData += GECE_NUM_INTENSITY_BYTES_PER_PIXEL;

        // DEBUG_V ("");

        // wait until all of the data has been clocked out
        // (hate waits but there are no status bits to watch)
        while((_getCycleCount () - StartingCycleCount) < GECE_CCOUNT_IDLETIME) {}

        // 10us start bit
        CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
        StartingCycleCount = _getCycleCount ();
        while ((_getCycleCount () - StartingCycleCount) < (GECE_CCOUNT_STARTBIT - 10)) {}

        // now convert the bits into a byte stream
        for (uint32_t currentShiftMask = bit (GECE_PACKET_SIZE - 1);
            0 != currentShiftMask; currentShiftMask >>= 1)
        {
            enqueue (((packet & currentShiftMask) == 0) ? GECE_DATA_ZERO : GECE_DATA_ONE);
        }

        // enable the stop bit after the last data is sent
        SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);

        // Clear all interrupt flags for this uart
        WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

        // have we sent all of the pixel data?
        if (++OutputFrame.CurrentPixelID >= pixel_count)
        {
            // CLEAR_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
            OutputFrame.CurrentPixelID = 0;
            OutputFrame.pCurrentInputData = pOutputBuffer;
        }

    } // end Our uart generated an interrupt

} // HandleWS2811Interrupt
//----------------------------------------------------------------------------
void c_OutputGECE::Render()
{
    // DEBUG_START;

    if (gpio_num_t (-1) == DataPin) { return; }
    // if (!canRefresh ()) { return; }
    // if (0 != (READ_PERI_REG (UART_INT_ENA (UartId)) & UART_TXFIFO_EMPTY_INT_ENA)) { return; }

    // ReportNewFrame ();

    // OutputFrame.CurrentPixelID = 0;
    // OutputFrame.pCurrentInputData = pOutputBuffer;

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
#else
//     (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UART_NUM_0)))) = (uint32_t)('7');
    ESP_ERROR_CHECK (uart_enable_tx_intr (UartId, 1, 0));
#endif

    // DEBUG_END;

} // render
