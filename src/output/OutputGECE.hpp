#pragma once
/*
* OutputGECE.h - GECE driver code for ESPixelStick
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

#include "OutputCommon.hpp"

class c_OutputGECE: public c_OutputCommon
{
public:
    c_OutputGECE (c_OutputMgr::e_OutputChannelIds OutputChannelId, 
                  gpio_num_t outputGpio, 
                  uart_port_t uart,
                  c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGECE ();

    // functions to be provided by the derived class
    void      Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool      SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void      GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void      Render ();                                        ///< Call from loop(),  renders output data
    void      GetDriverName (String & sDriverName) { sDriverName = String (F ("GECE")); }
    void      GetStatus (ArduinoJson::JsonObject & jsonStatus) { c_OutputCommon::GetStatus (jsonStatus); }
    uint16_t  GetNumChannelsNeeded ();

    void IRAM_ATTR ISR_Handler (); ///< UART ISR

private:

#define GECE_PIXEL_LIMIT        63  ///< Total pixel limit
#define GECE_DEFAULT_BRIGHTNESS 0xCC

    // JSON configuration parameters
    uint8_t         pixel_count = GECE_PIXEL_LIMIT;
    uint8_t         brightness  = GECE_DEFAULT_BRIGHTNESS;

    bool validate();

    typedef struct OutputFrame_t
    {
        uint32_t CurrentPixelID;
        uint8_t* pCurrentInputData;
    };
    OutputFrame_t OutputFrame;
};

// Cycle counter
static uint32_t _getCycleCount (void) __attribute__ ((always_inline));
static inline uint32_t _getCycleCount (void) {
    uint32_t ccount;
    __asm__ __volatile__ ("rsr %0,ccount":"=a" (ccount));
    return ccount;
}
