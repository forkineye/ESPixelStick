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

static char LOOKUP_GECE[] = 
{
    0b01111100,     // 0 - (0)00 111 11(1)
    0b01100000      // 1 - (0)00 000 11(1)
};


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

#define GECE_FRAME_TIME     uint32_t(((1.0/GECE_BAUDRATE) * 1000000.0) * (1 + 7 + 1) * GECE_PACKET_SIZE) // 790L    /* 790us frame time */
#define GECE_IDLE_TIME      45     /* 45us idle time - should be 30us */

#define CYCLES_GECE_START   (F_CPU / 100000) // 10us

#define GECE_NUM_CHAN_PER_PIXEL 3

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
void c_OutputGECE::Begin()
{
    // DEBUG_START;

    if (gpio_num_t (-1) == DataPin) { return; }

    SetOutputBufferSize (pixel_count * GECE_NUM_CHAN_PER_PIXEL);

    // Serial rate is 3x 100KHz for GECE
#ifdef ARDUINO_ARCH_ESP8266
    InitializeUart (GECE_BAUDRATE,
                    SERIAL_7N1,
                    SERIAL_TX_ONLY,
                    OM_CMN_NO_CUSTOM_ISR);
#else
    uart_config_t uart_config;
    uart_config.baud_rate           = GECE_BAUDRATE;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_7_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;
    InitializeUart (uart_config, OM_CMN_NO_CUSTOM_ISR);
#endif

    // FrameMinDurationInMicroSec = (GECE_FRAME_TIME + GECE_IDLE_TIME) * pixel_count;
    FrameMinDurationInMicroSec = GECE_FRAME_TIME + GECE_IDLE_TIME;
    DEBUG_V (String ("FrameMinDurationInMicroSec: ") + String (FrameMinDurationInMicroSec));
    // SET_PERI_REG_MASK(UART_CONF0(UartId), UART_TXD_BRK);
    // delayMicroseconds(GECE_IDLE_TIME);

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
    setFromJSON(brightness,  jsonConfig, CN_brightness);
    // enums need to be converted to uints for json
    temp = uint (DataPin);
    setFromJSON(temp,        jsonConfig, CN_data_pin);
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
    jsonConfig[CN_brightness]  = brightness;

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

    SetOutputBufferSize (pixel_count * GECE_NUM_CHAN_PER_PIXEL);

    OutputFrame.CurrentPixelID = 0;
    OutputFrame.pCurrentInputData = pOutputBuffer;

    // DEBUG_END;

    return response;

} // validate

//----------------------------------------------------------------------------
void c_OutputGECE::Render()
{
    // DEBUG_START;

    if (gpio_num_t (-1) == DataPin) { return; }
    if (!canRefresh ()) { return; }

    // enqueue (char('P' + OutputFrame.CurrentPixelID));

    // DEBUG_V ("");

    uint32_t packet = 0;

    // Build a GECE packet
    // DEBUG_V ("");
    packet  = GECE_SET_BRIGHTNESS (brightness); // <= This clears the other fields
    packet |= GECE_SET_ADDRESS    (OutputFrame.CurrentPixelID);
    packet |= GECE_SET_RED        (*OutputFrame.pCurrentInputData++);
    packet |= GECE_SET_GREEN      (*OutputFrame.pCurrentInputData++);
    packet |= GECE_SET_BLUE       (*OutputFrame.pCurrentInputData++);

    // DEBUG_V ("");
    // 10us start bit
    GenerateBreak (10);
    // DEBUG_V ("");

    ReportNewFrame ();

    // now convert the bits into a byte stream
    for (uint8_t currentShiftCount = GECE_PACKET_SIZE; 0 != currentShiftCount; --currentShiftCount)
    {
        enqueue (LOOKUP_GECE[(packet >> (currentShiftCount - 1)) & 0x1]);
    }

    // have we sent all of the pixel data?
    if (++OutputFrame.CurrentPixelID >= pixel_count)
    {
        OutputFrame.CurrentPixelID    = 0;
        OutputFrame.pCurrentInputData = pOutputBuffer;
    }

    // DEBUG_END;

} // render
