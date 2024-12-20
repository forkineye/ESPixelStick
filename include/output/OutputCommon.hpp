#pragma once
/*
* OutputCommon.hpp - Output base class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
*   This is a base class used to manage the output port. It provides a common API
*   for use by the factory class to manage the object.
*/

#include "ESPixelStick.h"
#include "OutputMgr.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   include <driver/uart.h>
#endif

class c_OutputCommon
{
public:
    c_OutputCommon (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                    gpio_num_t outputGpio,
                    uart_port_t uart,
                    c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputCommon ();

    typedef  c_OutputMgr::e_OutputChannelIds OID_t;
    typedef  c_OutputMgr::e_OutputType       OTYPE_t;

    // functions to be provided by the derived class
    virtual void         Begin () {}                                           ///< set up the operating environment based on the current config (or defaults)
    virtual bool         SetConfig (ArduinoJson::JsonObject & jsonConfig);     ///< Set a new config in the driver
    virtual void         GetConfig (ArduinoJson::JsonObject & jsonConfig);     ///< Get the current config used by the driver
    virtual uint32_t     Poll () = 0;                                        ///< Call from loop(),  renders output data
#ifdef ARDUINO_ARCH_ESP32
    virtual bool         RmtPoll () = 0;                                        ///< Call from loop(),  renders output data
#endif // def ARDUINO_ARCH_ESP32
    virtual void         GetDriverName (String & sDriverName) = 0;             ///< get the name for the instantiated driver
            OID_t        GetOutputChannelId () { return OutputChannelId; }     ///< return the output channel number
            uint8_t    * GetBufferAddress ()   { return pOutputBuffer;}        ///< Get the address of the buffer into which the E1.31 handler will stuff data
            uint32_t     GetBufferUsedSize ()  { return OutputBufferSize;}     ///< Get the address of the buffer into which the E1.31 handler will stuff data
            gpio_num_t   GetOutputGpio ()      { return DataPin; }
            OTYPE_t      GetOutputType ()      { return OutputType; }          ///< Have the instance report its type.
    virtual void         GetStatus (ArduinoJson::JsonObject & jsonStatus) = 0;
    virtual void         BaseGetStatus (ArduinoJson::JsonObject & jsonStatus);
            void         SetOutputBufferAddress (uint8_t* pNewOutputBuffer) { pOutputBuffer = pNewOutputBuffer; }
    virtual void         SetOutputBufferSize (uint32_t NewOutputBufferSize)  { OutputBufferSize = NewOutputBufferSize; };
    virtual uint32_t     GetNumOutputBufferBytesNeeded () = 0;
    virtual uint32_t     GetNumOutputBufferChannelsServiced () = 0;
    virtual void         PauseOutput (bool NewState) {Paused = NewState;}
    virtual void         ClearBuffer ();
    virtual void         WriteChannelData (uint32_t StartChannelId, uint32_t ChannelCount, byte *pSourceData);
    virtual void         ReadChannelData (uint32_t StartChannelId, uint32_t ChannelCount, byte *pTargetData);
    virtual bool         ValidateGpio (gpio_num_t ConsoleTxGpio, gpio_num_t ConsoleRxGpio);
    virtual bool         DriverIsSendingIntensityData() {return false;}
    virtual uint32_t     GetFrameTimeMs() {return 1 + (ActualFrameDurationMicroSec / 1000); }
    bool                 IsPaused() {return Paused;}

protected:

    gpio_num_t  DataPin                     = gpio_num_t (-1);
    uart_port_t UartId                      = uart_port_t (-1);
    OTYPE_t     OutputType                  = OTYPE_t::OutputType_Disabled;
    OID_t       OutputChannelId             = OID_t::OutputChannelId_End;
    bool        HasBeenInitialized          = false;
    uint32_t    FrameDurationInMicroSec     = 25000;
    uint32_t    ActualFrameDurationMicroSec = 50000; // Default time for relays is every 50ms
    uint8_t   * pOutputBuffer               = nullptr;
    uint32_t    OutputBufferSize            = 0;
    uint32_t    FrameCount                  = 0;
    bool        Paused = false;

    virtual void ReportNewFrame ();

    inline bool canRefresh ()
    {
        bool response = false;
        uint32_t Now = micros ();
        uint32_t FrameTimeDeltaInMicroSec = Now - FrameStartTimeInMicroSec; // how many us since the frame started

        // did the counter wrap?
        if(Now < FrameStartTimeInMicroSec)
        {
            FrameTimeDeltaInMicroSec = Now + (0 - FrameStartTimeInMicroSec);
        }

        if(FrameTimeDeltaInMicroSec > FrameDurationInMicroSec)
        {
            response = true;
        }
        return response;
    }

private:
    uint32_t    FrameStartTimeInMicroSec   = 0;

}; // c_OutputCommon
