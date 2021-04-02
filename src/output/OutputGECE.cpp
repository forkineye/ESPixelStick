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

#include <ArduinoJson.h>

#include "OutputGECE.hpp"
#include "../ESPixelStick.h"
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

#define GECE_DATA_ZERO 0b01111100
#define GECE_DATA_ONE  0b01100000

static char LOOKUP_GECE[] = 
{
    0b01111100,     // 0 - (0)00 111 11(1)
    0b01100000      // 1 - (0)00 000 11(1)
};

/*
output looks like this

Start bit = High for 10us
26 data bits. 
    Each bit is 30us
    0 = 10 us low, 20 us high
    1 = 20 us low, 10 us high
stop bit = low for at least 30us
*/

#define GECE_uSec_PER_GECE_BIT      30.0
// #define GECE_uSec_PER_GECE_BIT      300.0
#define GECE_UART_BITS_PER_GECE_BIT (1.0 + 7.0 + 1.0)
#define GECE_UART_uSec_PER_BIT      (GECE_uSec_PER_GECE_BIT / GECE_UART_BITS_PER_GECE_BIT)
#define GECE_BAUDRATE               uint32_t( (1.0/(GECE_UART_uSec_PER_BIT / 1000000))   )

#define GECE_FRAME_TIME            (GECE_PACKET_SIZE * GECE_uSec_PER_GECE_BIT) /* 790us packet frame time */
#define GECE_IDLE_TIME             (2 * GECE_uSec_PER_GECE_BIT)     /* 45us idle time - should be 30us */

#define CPU_ClockTimeNS             ((1.0 / float(F_CPU)) * 1000000000)
#define NUM_GECE_BITS_TO_WAIT       (2.0 + 3.0 + 2.0) // 2 being sent + 3 for interframe gap + 1 spare
#define NUM_NS_TO_WAIT              (GECE_uSec_PER_GECE_BIT * NUM_GECE_BITS_TO_WAIT * 100)
#define GECE_CCOUNT_DELAY           uint32_t(NUM_NS_TO_WAIT / CPU_ClockTimeNS)

#define GECE_DEFAULT_BRIGHTNESS 0xCC

#define GECE_ADDRESS_MASK       0x3F
#define GECE_ADDRESS_SHIFT      20

#define GECE_BRIGHTNESS_MASK    0xFF
#define GECE_BRIGHTNESS_SHIFT   12

#define GECE_BLUE_MASK          0xF
#define GECE_BLUE_SHIFT         8

#define GECE_GREEN_MASK         0xF
#define GECE_GREEN_SHIFT        4

#define GECE_RED_MASK           0xF
#define GECE_RED_SHIFT          0

#define GECE_GET_ADDRESS(packet)    (((packet) >> GECE_ADDRESS_SHIFT)    & GECE_ADDRESS_MASK)
#define GECE_GET_BRIGHTNESS(packet) (((packet) >> GECE_BRIGHTNESS_SHIFT) & GECE_BRIGHTNESS_MASK)
#define GECE_GET_BLUE(packet)       (((packet) >> GECE_BLUE_SHIFT)       & GECE_BLUE_MASK)
#define GECE_GET_GREEN(packet)      (((packet) >> GECE_GREEN_SHIFT)      & GECE_GREEN_MASK)
#define GECE_GET_RED(packet)        (((packet) >> GECE_RED_SHIFT )       & GECE_RED_MASK)

#define GECE_SET_ADDRESS(value)     ((uint32_t(value) & GECE_ADDRESS_MASK)    << GECE_ADDRESS_SHIFT)
#define GECE_SET_BRIGHTNESS(value)  ((uint32_t(value) & GECE_BRIGHTNESS_MASK) << GECE_BRIGHTNESS_SHIFT) 
#define GECE_SET_BLUE(value)        ((uint32_t(value) & GECE_BLUE_MASK)       << GECE_BLUE_SHIFT)       
#define GECE_SET_GREEN(value)       ((uint32_t(value) & GECE_GREEN_MASK)      << GECE_GREEN_SHIFT)     
#define GECE_SET_RED(value)         ((uint32_t(value) & GECE_RED_MASK)        << GECE_RED_SHIFT )       

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

    // FrameMinDurationInMicroSec = (GECE_FRAME_TIME + GECE_IDLE_TIME) * pixel_count;
    FrameMinDurationInMicroSec = GECE_FRAME_TIME + GECE_IDLE_TIME;
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

    // enums need to be converted to uints for json
    temp = uint (DataPin);
    setFromJSON(temp, jsonConfig, CN_data_pin);
    DataPin = gpio_num_t (temp);

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

    // enums need to be converted to uints for json
    jsonConfig[CN_data_pin]    = uint (DataPin);

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
        packet  = GECE_SET_BRIGHTNESS (brightness); // <= This clears the other fields
        packet |= GECE_SET_ADDRESS (OutputFrame.CurrentPixelID);
        packet |= GECE_SET_RED (*OutputFrame.pCurrentInputData++);
        packet |= GECE_SET_GREEN (*OutputFrame.pCurrentInputData++);
        packet |= GECE_SET_BLUE (*OutputFrame.pCurrentInputData++);

        // DEBUG_V ("");

        // wait until all of the data has been clocked out 
        // (hate waits but there are no status bits to watch)
        while((_getCycleCount () - StartingCycleCount) < GECE_CCOUNT_DELAY) {}

        // this ensures a minimum break time and a Start bit
        enqueue (GECE_DATA_ONE);

        // now convert the bits into a byte stream
        for (uint32_t currentShiftMask = bit (GECE_PACKET_SIZE - 1);
            0 != currentShiftMask; currentShiftMask >>= 1)
        {
            enqueue (((packet & currentShiftMask) == 0) ? GECE_DATA_ZERO : GECE_DATA_ONE);
        }

        // Clear all interrupts flags for this uart
        WRITE_PERI_REG (UART_INT_CLR (UartId), UART_INTR_MASK);

        // enable the send (uart is stopped while sending a break. 
        // Output does not return to '1' prior to sending first data byte.
        CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
        // enable the stop bit after the last data is sent
        SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);

        // have we sent all of the pixel data?
        if (++OutputFrame.CurrentPixelID >= pixel_count)
        {
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

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK (UART_INT_ENA (UartId), UART_TXFIFO_EMPTY_INT_ENA);
#else
//     (*((volatile uint32_t*)(UART_FIFO_AHB_REG (UART_NUM_0)))) = (uint32_t)('7');
    ESP_ERROR_CHECK (uart_enable_tx_intr (UartId, 1, 0));
#endif

    // DEBUG_END;

} // render
