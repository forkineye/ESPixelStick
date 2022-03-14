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
#ifdef ARDUINO_ARCH_ESP8266
#   define GECE_IDLE_TIME               (45 + GECE_uSec_PER_GECE_BIT) /* 45us */
#else
#   define GECE_IDLE_TIME               45.0 /* 45us */
#endif

#define TIMER_FREQUENCY                 80000000
#define CPU_ClockTimeNS                 ((1.0 / float(F_CPU)) * 1000000000)
#define TIMER_ClockTimeNS               ((1.0 / float(TIMER_FREQUENCY)) * 1000000000)
#define GECE_CCOUNT_IDLETIME            uint32_t((GECE_IDLE_TIME * 1000) / CPU_ClockTimeNS)
#define GECE_CCOUNT_STARTBIT            uint32_t((GECE_uSec_PER_GECE_START_BIT * 1000) / CPU_ClockTimeNS) // 10us (min) start bit

#define GECE_NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define GECE_BITS_PER_INTENSITY                 4
#define GECE_BITS_BRIGHTNESS                    8
#define GECE_BITS_ADDRESS                       6
#define GECE_OVERHEAD_BITS                      (GECE_BITS_BRIGHTNESS + GECE_BITS_ADDRESS)
#define GECE_PACKET_SIZE                        ((GECE_NUM_INTENSITY_BYTES_PER_PIXEL * GECE_BITS_PER_INTENSITY) + GECE_OVERHEAD_BITS) //   26

#define GECE_FRAME_TIME_USEC    ((GECE_PACKET_SIZE * GECE_uSec_PER_GECE_BIT) + 90)
#define GECE_FRAME_TIME_NSEC    (GECE_FRAME_TIME_USEC * 1000)
#define GECE_CCOUNT_FRAME_TIME  uint32_t((GECE_FRAME_TIME_NSEC / TIMER_ClockTimeNS))
#define GECE_UART_BREAK_BITS    uint32_t((GECE_IDLE_TIME / GECE_UART_uSec_PER_BIT) + 1)

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

// Static arrays are initialized to zero at boot time
static c_OutputGECE* GECE_OutputChanArray[c_OutputMgr::e_OutputChannelIds::OutputChannelId_End];

#ifdef ARDUINO_ARCH_ESP32
static hw_timer_t * pHwTimer = nullptr;
static portMUX_TYPE timerMux;
#endif

//----------------------------------------------------------------------------
c_OutputGECE::c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                            gpio_num_t outputGpio,
                            uart_port_t uart,
                            c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    GECE_OutputChanArray[OutputChannelId] = nullptr;

    // DEBUG_END;

} // c_OutputGECE

//----------------------------------------------------------------------------
c_OutputGECE::~c_OutputGECE ()
{
    // DEBUG_START;

    GECE_OutputChanArray[OutputChannelId] = nullptr;

    // clean up the timer ISR
    bool foundActiveChannel = false;
    for (auto currentChannel : GECE_OutputChanArray)
    {
        // DEBUG_V (String ("currentChannel: ") + String (uint(currentChannel), HEX));
        if (nullptr != currentChannel)
        {
            // DEBUG_V ("foundActiveChannel");
            foundActiveChannel = true;
        }
    }

    // DEBUG_V ();

    // have all of the GECE channels been killed?
    if (!foundActiveChannel)
    {
        // DEBUG_V ("Detach Interrupts");
#ifdef ARDUINO_ARCH_ESP8266
        timer1_detachInterrupt ();
#elif defined(ARDUINO_ARCH_ESP32)
        if (pHwTimer)
        {
            timerAlarmDisable (pHwTimer);
        }
#endif
    }

    // DEBUG_END;
} // ~c_OutputGECE

//----------------------------------------------------------------------------
/* shell function to set the 'this' pointer of the real ISR
   This allows me to use non static variables in the ISR.
 */
static void IRAM_ATTR timer_intr_handler ()
{
#ifdef ARDUINO_ARCH_ESP32
    portENTER_CRITICAL_ISR (&timerMux);
#endif
    // (*((volatile uint32_t*)(UART_FIFO_AHB_REG (0)))) = (uint32_t)('.');
    for (auto currentChannel : GECE_OutputChanArray)
    {
        if (nullptr != currentChannel)
        {
            // U0F = '.';
            currentChannel->ISR_Handler ();
        }
    }
#ifdef ARDUINO_ARCH_ESP32
    portEXIT_CRITICAL_ISR (&timerMux);
#endif

} // timer_intr_handler

//----------------------------------------------------------------------------
void c_OutputGECE::Begin ()
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
    memset ((void*)&uart_config, 0x00, sizeof (uart_config));
    uart_config.baud_rate = GECE_BAUDRATE;
    uart_config.data_bits = uart_word_length_t::UART_DATA_7_BITS;
    uart_config.stop_bits = uart_stop_bits_t::UART_STOP_BITS_1;
    InitializeUart (uart_config, 0);

    SET_PERI_REG_BITS (UART_IDLE_CONF_REG (UartId), UART_TX_BRK_NUM_V, GECE_UART_BREAK_BITS, UART_TX_BRK_NUM_S);

#endif

    // DEBUG_V (String ("       TIMER_FREQUENCY: ") + String (TIMER_FREQUENCY));
    // DEBUG_V (String ("     TIMER_ClockTimeNS: ") + String (TIMER_ClockTimeNS));
    // DEBUG_V (String ("                 F_CPU: ") + String (F_CPU));
    // DEBUG_V (String ("       CPU_ClockTimeNS: ") + String (CPU_ClockTimeNS));
    // DEBUG_V (String ("  GECE_FRAME_TIME_USEC: ") + String (GECE_FRAME_TIME_USEC));
    // DEBUG_V (String ("  GECE_FRAME_TIME_NSEC: ") + String (GECE_FRAME_TIME_NSEC));
    // DEBUG_V (String ("GECE_CCOUNT_FRAME_TIME: ") + String (GECE_CCOUNT_FRAME_TIME));

#ifdef ARDUINO_ARCH_ESP8266

    // DEBUG_V ();

    timer1_attachInterrupt (timer_intr_handler); // Add ISR Function
    timer1_enable (TIM_DIV1, TIM_EDGE, TIM_LOOP);
    /* Dividers:
        TIM_DIV1 = 0,   // 80MHz (80 ticks/us - 104857.588 us max)
        TIM_DIV16 = 1,  // 5MHz (5 ticks/us - 1677721.4 us max)
        TIM_DIV256 = 3  // 312.5Khz (1 tick = 3.2us - 26843542.4 us max)
    Reloads:
        TIM_SINGLE	0 //on interrupt routine you need to write a new value to start the timer again
        TIM_LOOP	1 //on interrupt the counter will start with the same value again
    */

    // Arm the Timer for our Interval
    timer1_write (GECE_CCOUNT_FRAME_TIME);

#elif defined(ARDUINO_ARCH_ESP32)

    // Configure Prescaler to 1, as our timer runs @ 80Mhz
    // Giving an output of 80,000,000 / 80 = 1,000,000 ticks / second
    if (nullptr == pHwTimer)
    {
        // Prescaler to 1 (min value) reuslts in a divide by 2 on the clock.
        pHwTimer = timerBegin (0, 1, true);
        timerAttachInterrupt (pHwTimer, &timer_intr_handler, true);
        // ESP32 is a multi core / multi processing chip. It is mandatory to disable task switches during ISR
        timerMux = portMUX_INITIALIZER_UNLOCKED;
    }

    // compensate for prescale divide by two.
    timerAlarmWrite (pHwTimer, GECE_CCOUNT_FRAME_TIME / 2, true);
    // timerAlarmWrite (pHwTimer, GECE_CCOUNT_FRAME_TIME, true);
    timerAlarmEnable (pHwTimer);

#endif
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

#ifdef ARDUINO_ARCH_ESP32
    ESP_ERROR_CHECK (uart_set_pin (UartId, DataPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#endif

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
size_t c_OutputGECE::GetNumChannelsNeeded ()
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
#ifdef ARDUINO_ARCH_ESP8266
    // begin start bit
    CLEAR_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);
    uint32_t StartingCycleCount = _getCycleCount ();
#endif

    // Build a GECE packet
    uint32_t packet = 0;
    packet |= GECE_SET_BRIGHTNESS (brightness);
    packet |= GECE_SET_ADDRESS (OutputFrame.CurrentPixelID);
    packet |= GECE_SET_RED     (OutputFrame.pCurrentInputData[0]);
    packet |= GECE_SET_GREEN   (OutputFrame.pCurrentInputData[1]);
    packet |= GECE_SET_BLUE    (OutputFrame.pCurrentInputData[2]);
    OutputFrame.pCurrentInputData += GECE_NUM_INTENSITY_BYTES_PER_PIXEL;

#ifdef ARDUINO_ARCH_ESP8266
    // finish  start bit
    while ((_getCycleCount () - StartingCycleCount) < (GECE_CCOUNT_STARTBIT - 10)) {}
#endif
    // now convert the bits into a byte stream
    for (uint32_t currentShiftMask = bit (GECE_PACKET_SIZE - 1);
        0 != currentShiftMask; currentShiftMask >>= 1)
    {
        enqueueUart (((packet & currentShiftMask) == 0) ? GECE_DATA_ZERO : GECE_DATA_ONE);
    }

    // enable the stop bit after the last data is sent
    SET_PERI_REG_MASK (UART_CONF0 (UartId), UART_TXD_BRK);

    // have we sent all of the pixel data?
    if (++OutputFrame.CurrentPixelID >= pixel_count)
    {
        OutputFrame.CurrentPixelID = 0;
        OutputFrame.pCurrentInputData = pOutputBuffer;
    }

} // ISR_Handler

//----------------------------------------------------------------------------
void c_OutputGECE::Render()
{
    // DEBUG_START;

    // start processing the timer interrupts
    if (nullptr != pOutputBuffer)
    {
        GECE_OutputChanArray[OutputChannelId] = this;
    }

    // DEBUG_END;

} // render
