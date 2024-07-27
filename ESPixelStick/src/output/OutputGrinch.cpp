/*
* OutputGRINCH.cpp - GRINCH driver code for ESPixelStick UART
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
*/

#include "../ESPixelStick.h"
#ifdef SUPPORT_OutputType_GRINCH

#include "OutputGrinch.hpp"

//----------------------------------------------------------------------------
c_OutputGrinch::c_OutputGrinch (c_OutputMgr::e_OutputChannelIds OutputChannelId,
    gpio_num_t outputGpio,
    uart_port_t uart,
    c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon (OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    // InterFrameGapInMicroSec = GRINCH_MIN_IDLE_TIME_US;

    // DEBUG_END;
} // c_OutputGrinch

//----------------------------------------------------------------------------
c_OutputGrinch::~c_OutputGrinch ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputGrinch

//----------------------------------------------------------------------------
void c_OutputGrinch::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputCommon::GetConfig (jsonConfig);
    jsonConfig[CN_cs_pin] = uint8_t (DataStrobe);
    jsonConfig[CN_count] = NumberOfGrinchControllers;

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputGrinch::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    c_OutputCommon::GetStatus (jsonStatus);

} // GetStatus

//----------------------------------------------------------------------------
void c_OutputGrinch::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputCommon::SetOutputBufferSize (NumChannelsAvailable);

    // Calculate our refresh time
    // SetFrameDurration (((1.0 / float (GRINCH_BIT_RATE)) * MicroSecondsInASecond), BlockSize, BlockDelay);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputGrinch::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputCommon::SetConfig (jsonConfig);
    response |= setFromJSON (DataStrobe, jsonConfig, CN_cs_pin);
    response |= setFromJSON (NumberOfGrinchControllers, jsonConfig, CN_count);

    NumberOfGrinchChannels = NumberOfGrinchControllers * DATA_CHANNELS_PER_GRINCH;
    NumberOfGrinchDataBytes = NumberOfGrinchChannels / (sizeof(uint8_t) * 8);

    // DEBUG_V(String("NumberOfGrinchControllers: ") + String(NumberOfGrinchControllers));
    // DEBUG_V(String("   NumberOfGrinchChannels: ") + String(NumberOfGrinchChannels));
    // DEBUG_V(String("  NumberOfGrinchDataBytes: ") + String(NumberOfGrinchDataBytes));
    SetOutputBufferSize(NumberOfGrinchChannels);

    // turn off the outout strobe (latch data)
    pinMode(DataStrobe, OUTPUT);
    digitalWrite(DataStrobe, HIGH);

    // Calculate our refresh time
    // SetFrameDurration (((1.0 / float (GRINCH_BIT_RATE)) * MicroSecondsInASecond), BlockSize, BlockDelay);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputGrinch::ISR_GetNextIntensityToSend (uint32_t &DataToSend)
{
    if(ISR_MoreDataToSend())
    {
        // DEBUG_V(String("DataToSend: ") + String(DataToSend));
        DataToSend = ((uint8_t*)&dataBuffer[0])[SpiOutputDataByteIndex-1];
        SpiOutputDataByteIndex--;
    }

    return ISR_MoreDataToSend();
} // ISR_GetNextIntensityToSend

//----------------------------------------------------------------------------
void c_OutputGrinch::StartNewFrame()
{
    // DEBUG_START;

    // put the shift registers into clock through mode.
    digitalWrite(DataStrobe, HIGH);
    delay(1);
    digitalWrite(DataStrobe, LOW);

    // build the data frame
    uint32_t    NumChannelsToProcess = GetNumOutputBufferBytesNeeded();
    uint8_t     * pInputData = GetBufferAddress();
    uint64_t    OutputData = 0;
    uint8_t     OutputDataIndex = 0;
    uint8_t     bitCounter = 0;

    while(NumChannelsToProcess)
    {
        if(bitCounter >= (sizeof(OutputData) * 8))
        {
            // write the output data
            OutputDataIndex ++;

            // move to the next controller
            bitCounter = 0;
            OutputData = 0;
        }

        // set the output bit to one if the output is greater than half on
        OutputData = (OutputData << 1) | (pInputData[0] < 128);
        dataBuffer[OutputDataIndex] = OutputData;
        pInputData ++;
        bitCounter ++;
        NumChannelsToProcess--;
    }

    SpiOutputDataByteIndex = NumberOfGrinchDataBytes;

    // DEBUG_END;
} // StartNewFrame

#endif // def SUPPORT_OutputType_GRINCH
