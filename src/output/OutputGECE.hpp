#pragma once
/*
* OutputGECE.h - GECE driver code for ESPixelStick
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

#include "OutputCommon.hpp"
#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputGECE: public c_OutputCommon
{
public:
    c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId, 
                  gpio_num_t outputGpio, uart_port_t uart);
    virtual ~c_OutputGECE ();

    // functions to be provided by the derived class
    void      Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool      SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void      GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void      Render ();                                        ///< Call from loop(),  renders output data
    void      GetDriverName (String& sDriverName) { sDriverName = String (F ("GECE")); }
    c_OutputMgr::e_OutputType GetOutputType () { return c_OutputMgr::e_OutputType::OutputType_GECE; } ///< Have the instance report its type.

private:

#define GECE_PIXEL_LIMIT                        63  ///< Total pixel limit
#define GECE_NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define GECE_BITS_PER_INTENSITY                 8
#define GECE_NUM_DATA_BYTES_PER_INTENSITY_BYTE  (1 * GECE_BITS_PER_INTENSITY)
#define GECE_OVERHEAD_BYTES                     2
#define GECE_PACKET_SIZE                        ((GECE_NUM_INTENSITY_BYTES_PER_PIXEL * GECE_NUM_DATA_BYTES_PER_INTENSITY_BYTE) + GECE_OVERHEAD_BYTES) //   26
#define GECE_OUTPUT_BUFF_SIZE                   ()

    // JSON configuration parameters
    uint8_t         pixel_count = 0;
    uint8_t         brightness  = 0;

    // Internal variables
    uint32_t        startTime        = 0;              ///< When the last frame TX started
    uint32_t        refreshTime      = 0;              ///< Time until we can refresh after starting a TX
    HardwareSerial *pSerialInterface = nullptr;
    /* Drop the update if our refresh rate is too high */
    inline bool canRefresh() 
    {
        return (micros() - startTime) >= refreshTime;
    }

    bool validate();
};

// Cycle counter
static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

