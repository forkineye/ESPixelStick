/*
* GECE.cpp - GECE driver code for ESPixelStick
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

#include <ArduinoJson.h>

#include "OutputGECE.hpp"
#include "../ESPixelStick.h"
#include "../FileIO.h"

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

// #define GECE_INTENSITY_MASK     0xF

#define GECE_GET_ADDRESS(packet)    (((packet) >> GECE_ADDRESS_SHIFT)    & GECE_ADDRESS_MASK)
#define GECE_GET_BRIGHTNESS(packet) (((packet) >> GECE_BRIGHTNESS_SHIFT) & GECE_BRIGHTNESS_MASK)
#define GECE_GET_BLUE(packet)       (((packet) >> GECE_BLUE_SHIFT)       & GECE_BLUE_MASK)
#define GECE_GET_GREEN(packet)      (((packet) >> GECE_GREEN_SHIFT)      & GECE_GREEN_MASK)
#define GECE_GET_RED(packet)        (((packet) >> GECE_RED_SHIFT )       & GECE_RED_MASK)

#define GECE_SET_ADDRESS(value)    (((value) & GECE_ADDRESS_MASK)    >> GECE_ADDRESS_SHIFT)
#define GECE_SET_BRIGHTNESS(value) (((value) & GECE_BRIGHTNESS_MASK) >> GECE_BRIGHTNESS_SHIFT) 
#define GECE_SET_BLUE(value)       (((value) & GECE_BLUE_MASK)       >> GECE_BLUE_SHIFT)       
#define GECE_SET_GREEN(value)      (((value) & GECE_GREEN_MASK)      >> GECE_GREEN_SHIFT)     
#define GECE_SET_RED(value)        (((value) & GECE_RED_MASK)        >> GECE_RED_SHIFT )       


#define GECE_FRAME_TIME     790L    /* 790us frame time */
#define GECE_IDLE_TIME      45L     /* 45us idle time - should be 30us */

#define CYCLES_GECE_START   (F_CPU / 100000) // 10us

c_OutputGECE::c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId, gpio_num_t outputGpio, uart_port_t uart) :
    c_OutputCommon (OutputChannelId, outputGpio, uart)
{
    DEBUG_START;
    brightness = GECE_DEFAULT_BRIGHTNESS;

    // determine uart to use based on channel id
    switch (OutputChannelId)
    {
        case c_OutputMgr::e_OutputChannelIds::OutputChannelId_1:
        {
            pSerialInterface = &Serial1;
            break;
        }
#ifdef ARDUINO_ARCH_ESP32
        case c_OutputMgr::e_OutputChannelIds::OutputChannelId_2:
        {
            pSerialInterface = &Serial2;
            break;
        }
#endif // def ARCUINO_ARCH_32

        default:
        {
            LOG_PORT.println (String (F ("EEEEEE ERROR: Port '")) + int (OutputChannelId) + String (F ("' is not a valid GECE port. EEEEEE")));
            break;
        }

    } // end switch on channel ID
    DEBUG_END;
} // c_OutputGECE

c_OutputGECE::~c_OutputGECE () 
{
    DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }

    // shut down the interface
    pSerialInterface->end ();

    // put the pin into a safe state
    pinMode (DataPin, INPUT);

    DEBUG_END;
} // ~c_OutputGECE

void c_OutputGECE::Begin() 
{
    DEBUG_START;
    Serial.println (String (F ("** GECE Initialization for Chan: ")) + String (OutputChannelId) + " **");

    if (gpio_num_t (-1) == DataPin) { return; }

    // first make sure the interface is off
    pSerialInterface->end ();

    // Set output pins
    pinMode(DataPin, OUTPUT);
    digitalWrite(DataPin, LOW);

    refreshTime = (GECE_FRAME_TIME + GECE_IDLE_TIME) * pixel_count;

    // Serial rate is 3x 100KHz for GECE
#ifdef ARDUINO_ARCH_ESP8266
    pSerialInterface->begin(300000, SERIAL_7N1, SERIAL_TX_ONLY);
#elif defined ARDUINO_ARCH_ESP32
    pSerialInterface->begin (300000, SERIAL_7N1);
#endif
    SET_PERI_REG_MASK(UART_CONF0(UartId), UART_TXD_BRK);
    delayMicroseconds(GECE_IDLE_TIME);
    DEBUG_END;
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
    DEBUG_START;

    uint temp;
    FileIO::setFromJSON(pixel_count, jsonConfig["pixel_count"]);
    FileIO::setFromJSON(brightness,  jsonConfig["brightness"]);
    // enums need to be converted to uints for json
    temp = uint (DataPin);
    FileIO::setFromJSON(temp,        jsonConfig["data_pin"]);
    DataPin = gpio_num_t (temp);

    DEBUG_END;
    return validate ();

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputGECE::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    DEBUG_START;
    String DriverName = ""; GetDriverName (DriverName);

    jsonConfig["type"]        = DriverName;
    jsonConfig["pixel_count"] = pixel_count;
    jsonConfig["brightness"]  = brightness;

    // enums need to be converted to uints for json
    jsonConfig["data_pin"]    = uint (DataPin);

    DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
bool c_OutputGECE::validate ()
{
    DEBUG_START;

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

    DEBUG_END;
    return response;

} // validate

void c_OutputGECE::Render() 
{
    DEBUG_START;
    if (gpio_num_t (-1) == DataPin) { return; }
    if (!canRefresh()) return;

    uint32_t packet = 0;
    uint32_t pTime  = 0;

    // Build a GECE packet
    startTime = micros();
    uint8_t * pCurrentInputData = GetBufferAddress();

    for (uint8_t CurrentAddress = 0; CurrentAddress < pixel_count; ++CurrentAddress)
    {
        packet  = GECE_SET_BRIGHTNESS (brightness); // <= This clears the other fields
        packet |= GECE_SET_ADDRESS    (CurrentAddress);
        packet |= GECE_SET_RED        (*pCurrentInputData++);
        packet |= GECE_SET_GREEN      (*pCurrentInputData++);
        packet |= GECE_SET_BLUE       (*pCurrentInputData++);

        // now convert the bits into a byte stream
        uint8_t  OutputBuffer[GECE_PACKET_SIZE + 1]; ///< GECE Packet Buffer
        uint8_t* pCurrentOutputData = OutputBuffer;
        for (uint8_t currentShiftCount = GECE_PACKET_SIZE-1; currentShiftCount != -1; --currentShiftCount)
        {
            *pCurrentOutputData++ = LOOKUP_GECE[(packet >> currentShiftCount) & 0x1];
        }

        // Wait until ready to send
        while ((micros() - pTime) < (GECE_FRAME_TIME + GECE_IDLE_TIME)) {}

        // 10us start bit
        pTime = micros();
        uint32_t c = _getCycleCount();
        CLEAR_PERI_REG_MASK(UART_CONF0(UartId), UART_TXD_BRK);
        while ((_getCycleCount() - c) < CYCLES_GECE_START - 100) {}

        // Send the packet and then idle low (break)
        pSerialInterface->write(OutputBuffer, GECE_PACKET_SIZE);
        SET_PERI_REG_MASK(UART_CONF0(UartId), UART_TXD_BRK);

    } // for each intensity to send
    DEBUG_END;
} // render
