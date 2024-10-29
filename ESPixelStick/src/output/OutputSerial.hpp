#pragma once
/*
* OutputSerial.h - Pixel driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)

class c_OutputSerial : public c_OutputCommon
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSerial (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                   gpio_num_t outputGpio,
                   uart_port_t uart,
                   c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputSerial ();

    // functions to be provided by the derived class
    virtual void        Begin ();
    virtual bool        SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    virtual void        GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
            void        GetDriverName (String& sDriverName);
    virtual void        GetStatus (ArduinoJson::JsonObject & jsonStatus);
            uint32_t    GetNumOutputBufferBytesNeeded () { return OutputBufferSize; };
            uint32_t    GetNumOutputBufferChannelsServiced () { return OutputBufferSize; };
            void        SetOutputBufferSize (uint32_t NumChannelsAvailable);
            uint32_t    Poll = 0;
            void        StartNewFrame();

    bool IRAM_ATTR   ISR_GetNextIntensityToSend(uint32_t &DataToSend);
    bool IRAM_ATTR   ISR_MoreDataToSend() { return (SerialFrameState_t::SerialIdle != SerialFrameState); }

protected:
    void SetFrameDurration();

#define GS_CHANNEL_LIMIT 2048

    enum class BaudRate
    {
        BR_MIN = 38400,
        BR_DMX = 250000,
        BR_MAX = 460800,
        BR_DEF = 57600,
    };

    uint32_t CurrentBaudrate = uint32_t(BaudRate::BR_DEF); // current transmit rate

    /* DMX minimum timings per E1.11 */
    const uint32_t  DMX_BREAK_US     = uint32_t(((1.0 / float(BaudRate::BR_DMX)) * 23.0) * float(MicroSecondsInASecond));  // 23 bits = 92us
    const uint32_t  DMX_MAB_US       = uint32_t(((1.0 / float(BaudRate::BR_DMX)) *  3.0) * float(MicroSecondsInASecond));  //  3 bits = 12us
    uint32_t InterFrameGapInMicroSec = DMX_BREAK_US + DMX_MAB_US;

private:

    const uint32_t    MAX_HDR_SIZE         = 10;      // Max generic serial header size
    const uint32_t    MAX_FOOTER_SIZE      = 10;      // max generic serial footer size
    const uint32_t    MAX_CHANNELS         = 1024;
    const uint16_t    DEFAULT_NUM_CHANNELS = 64;
    const uint32_t    BUF_SIZE             = (MAX_CHANNELS + MAX_HDR_SIZE + MAX_FOOTER_SIZE);
    const uint32_t    DMX_BITS_PER_BYTE    = (1.0 + 8.0 + 2.0);
    const uint32_t    DMX_MaxFrameSize     = 512;

    uint32_t      Num_Channels = DEFAULT_NUM_CHANNELS;       // Number of data channels to transmit

    uint8_t*      NextIntensityToSend = nullptr;
    uint32_t      intensity_count = 0;
    uint32_t      SentIntensityCount = 0;

    float         IntensityBitTimeInUs = 0.0;
    uint32_t      NumBitsPerIntensity = 1 + 8 + 2;   // Start. 8 Data, Stop

    String        GenericSerialHeader;
    uint32_t      SerialHeaderSize  = 0;
    uint32_t      SerialHeaderIndex = 0;

    String        GenericSerialFooter;
    uint32_t      SerialFooterSize  = 0;
    uint32_t      SerialFooterIndex = 0;

#ifdef USE_SERIAL_DEBUG_COUNTERS
    uint32_t   IntensityBytesSent = 0;
    uint32_t   IntensityBytesSentLastFrame = 0;
    uint32_t   FrameStartCounter = 0;
    uint32_t   FrameEndCounter = 0;
    uint32_t   AbortFrameCounter = 0;
    uint32_t   LastDataSent = 0;
    uint32_t   DmxFrameStart = 0;
    uint32_t   DmxSendData = 0;
    uint32_t   Serialidle = 0;
#define SERIAL_DEBUG_COUNTER(p) p

#else

#define SERIAL_DEBUG_COUNTER(p)

#endif // def USE_SERIAL_DEBUG_COUNTERS

    bool validate ();        ///< confirm that the current configuration is valid

    enum RenardFrameDefinitions_t
    {
        CMD_DATA_START   = 0x80,
        ESC_CHAR         = 0x7F,
        FRAME_START_CHAR = 0x7E,
        FRAME_PAD_CHAR   = 0x7D,
        ESCAPED_OFFSET   = 0x4E,
        MIN_VAL_TO_ESC   = FRAME_PAD_CHAR,
        MAX_VAL_TO_ESC   = ESC_CHAR
    };

    enum SerialFrameState_t
    {
        RenardFrameStart,
        RenardDataStart,
        RenardSendData,
        RenardSendEscapedData,
        DMXSendFrameStart,
        DMXSendData,
        GenSerSendHeader,
        GenSerSendData,
        GenSerSendFooter,
        SerialIdle
    };
    SerialFrameState_t SerialFrameState = SerialIdle;

}; // c_OutputSerial

#endif // defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
