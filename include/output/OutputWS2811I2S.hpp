#pragma once
/*
* OutputWS2811I2S.h - WS2811 driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2025 Shelby Merrick
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
#include "ESPixelStick.h"
#if defined(SUPPORT_OutputProtocol_WS2811) && defined(SUPPORT_I2S)

#include "OutputWS2811.hpp"
#include "OutputI2S.hpp"

class c_OutputWS2811I2S : public c_OutputWS2811
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2811I2S (OM_OutputPortDefinition_t & OutputPortDefinition,
                       c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputWS2811I2S ();

    // functions to be provided by the derived class
    void    Begin ();                                        ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Set a new config in the driver
    uint32_t Poll () {return 0;}                             ///< Call from loop (),  renders output data
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);
    void    ISR_GetNextDataSlicesToSend (c_OutputI2S::I2S_Item_t * DataToSend, uint32_t numSlices);
    void    ISR_StartNewDataFrame();

    // to be removed when the I2S driver is fully implemented
    virtual bool RmtPoll() {return false;}

private:
    void    CalculateFrameBitSlices ();

    // The adjustments compensate for rounding errors in the calculations
    c_OutputI2S     *I2Sdriver;

    uint32_t        DataBitMask;
    uint32_t        DataBit;

    uint32_t        ZeroHighBitSliceCount;
    uint32_t        ZeroLowBitSliceCount;

    uint32_t        OneHighBitSliceCount;
    uint32_t        OneLowBitSliceCount;

    uint32_t        HighBitCurrentSliceCount;
    uint32_t        LowBitCurrentSliceCount;

    uint32_t        FrameResetSliceCount;
    uint32_t        FrameResetCurrentSliceCount;

    uint32_t        IdleSliceCount;
    uint32_t        IdleCurrentSliceCount;

    uint32_t        DataPattern;
    uint32_t        DataPatternMask;

    #define WS2811_I2S_DEBUG_COUNTERS
    #ifdef WS2811_I2S_DEBUG_COUNTERS
    #define INC_WS2811_I2S_DEBUG_COUNTERS(c) (++I2SDebugCounters.c)
    struct
    {
        uint32_t GetDataSlices;
        uint32_t FrameStarts;
        uint32_t FrameEnds;
        uint32_t IdleBitSlices;
        uint32_t FrameResetBitSlices;
        uint32_t DataBitSlices;
        uint32_t DataBytes;
        uint32_t BitSliceHigh;
        uint32_t BitSliceLow;
        uint32_t NextDataBit;
        uint32_t DataBits;
        uint32_t DataBitEnd;
        uint32_t DataByteEnd;
    } I2SDebugCounters;
    #else
    #define INC_WS2811_I2S_DEBUG_COUNTERS(c)
    #endif // def WS2811_I2S_DEBUG_COUNTERS

}; // c_OutputWS2811I2S

#endif // defined(SUPPORT_OutputProtocol_WS2811) && defined(SUPPORT_I2S)
