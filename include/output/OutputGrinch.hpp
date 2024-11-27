#pragma once
/*
* OutputGrinch.h - Grinch driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2024 Shelby Merrick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#include "OutputCommon.hpp"
#ifdef SUPPORT_OutputType_GRINCH

class c_OutputGrinch : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputGrinch (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputGrinch ();

    // functions to be provided by the derived class
    virtual bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String & sDriverName) { sDriverName = F("Grinch"); }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);
    uint32_t     GetNumOutputBufferBytesNeeded () { return (NumberOfGrinchChannels); }
    uint32_t     GetNumOutputBufferChannelsServiced () { return (NumberOfGrinchChannels); }

             void           StartNewFrame();
    inline   bool IRAM_ATTR ISR_MoreDataToSend () {return SpiOutputDataByteIndex > 0;}
             bool IRAM_ATTR ISR_GetNextIntensityToSend (uint32_t &DataToSend);

protected:

#define MAX_NUM_SUPPORTED_GRINCHES 4
#define DATA_CHANNELS_PER_GRINCH 64

private:
    uint8_t NumberOfGrinchControllers = 1;
    uint8_t NumberOfGrinchChannels =  NumberOfGrinchControllers * DATA_CHANNELS_PER_GRINCH;
    uint8_t NumberOfGrinchDataBytes = NumberOfGrinchChannels / 8;
    uint8_t SpiOutputDataByteIndex = 0;

    union GrinchData_s
    {
        struct GrinchLatchData
        {
            uint8_t LatchChan_01_08 = 0;
            uint8_t LatchChan_09_16 = 0;
            uint8_t LatchChan_17_24 = 0;
            uint8_t LatchChan_25_32 = 0;
        };
        uint8_t Data[4];
    };
    GrinchData_s dataBuffer[MAX_NUM_SUPPORTED_GRINCHES];


}; // c_OutputGrinch
#endif // def SUPPORT_OutputType_GRINCH
