/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
******************************************************************/

#include "ESPixelStick.h"

#ifdef SUPPORT_OutputType_Servo_PCA9685

#include "output/OutputServoPCA9685.hpp"
#include <utility>
#include <algorithm>
#include <math.h>

//----------------------------------------------------------------------------
c_OutputServoPCA9685::c_OutputServoPCA9685 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio,
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;

    uint32_t id = 0;
    for (ServoPCA9685Channel_t &currentServoPCA9685Channel : OutputList)
    {
        currentServoPCA9685Channel.Id               = id++;
        currentServoPCA9685Channel.Enabled          = false;
        currentServoPCA9685Channel.MinLevel         = SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH;
        currentServoPCA9685Channel.MaxLevel         = SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH;
        currentServoPCA9685Channel.PreviousValue    = 0;
        currentServoPCA9685Channel.IsReversed       = false;
        currentServoPCA9685Channel.Is16Bit          = false;
        currentServoPCA9685Channel.IsScaled         = true;
        currentServoPCA9685Channel.HomeValue        = 0;
    }

    // DEBUG_END;
} // c_OutputServoPCA9685

//----------------------------------------------------------------------------
c_OutputServoPCA9685::~c_OutputServoPCA9685 ()
{
    // DEBUG_START;

    if(nullptr != pwm)
    {
        // DEBUG_V();
        delete pwm;
    }

    // DEBUG_END;
} // ~c_OutputServoPCA9685

//----------------------------------------------------------------------------
void c_OutputServoPCA9685::Begin ()
{
    // DEBUG_START;

    if(!HasBeenInitialized)
    {
        // DEBUG_V("Allocate PWM");
        pwm = new Adafruit_PWMServoDriver();

        SetOutputBufferSize(Num_Channels);

        pwm->begin();
        pwm->setPWMFreq(UpdateFrequency);

        validate();

        HasBeenInitialized = true;
    }

    // DEBUG_END;
} // Begin

#ifdef UseCustomClearBuffer
//-----------------------------------------------------------------------------
void c_OutputServoPCA9685::ClearBuffer ()
{
    // DEBUG_START;

    // memset(GetBufferAddress(), 0x00, GetBufferUsedSize());
    for (ServoPCA9685Channel_t & currentServoPCA9685Channel : OutputList)
    {
        GetBufferAddress()[currentServoPCA9685Channel.Id] =
            currentServoPCA9685Channel.HomeValue;
    }

    // DEBUG_END;
} // ClearBuffer
#endif // def UseCustomClearBuffer

//----------------------------------------------------------------------------
bool c_OutputServoPCA9685::validate ()
{
    // DEBUG_START;
    bool response = true;

    if ((Num_Channels > OM_SERVO_PCA9685_CHANNEL_LIMIT) || (Num_Channels < 1))
    {
        logcon (CN_stars + String (MN_01) + OM_SERVO_PCA9685_CHANNEL_LIMIT + " " + CN_stars);
        Num_Channels = OM_SERVO_PCA9685_CHANNEL_LIMIT;
        response = false;
    }

    if (Num_Channels < OM_SERVO_PCA9685_CHANNEL_LIMIT)
    {
        logcon (CN_stars + String (MN_02) + CN_stars);

        for (int ChannelIndex = OM_SERVO_PCA9685_CHANNEL_LIMIT - 1; ChannelIndex > Num_Channels; ChannelIndex--)
        {
            logcon (CN_stars + String (MN_03) + String (ChannelIndex + 1) + "' " + CN_stars);
            OutputList[ChannelIndex].Enabled = false;
        }

        response = false;
    }

    SetOutputBufferSize (Num_Channels);

    /*
    uint8_t CurrentServoPCA9685ChanIndex = 0;
    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {

    } // for each output channel
    */
    // DEBUG_END;
    return response;

} // validate

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputServoPCA9685::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    do // once
    {
        // PrettyPrint (jsonConfig, String("c_OutputServoPCA9685::SetConfig"));
        setFromJSON (UpdateFrequency, jsonConfig, OM_SERVO_PCA9685_UPDATE_INTERVAL_NAME);
        pwm->setPWMFreq (UpdateFrequency);

        // do we have a channel configuration array?
        JsonArray JsonChannelList = jsonConfig[(char*)OM_SERVO_PCA9685_CHANNELS_NAME];
        if (!JsonChannelList)
        {
            // if not, flag an error and stop processing
            logcon (MN_04);
            break;
        }

        for (JsonVariant ChannelData : JsonChannelList)
        {
            JsonObject JsonChannelData = ChannelData.as<JsonObject>();
            uint8_t ChannelId = OM_SERVO_PCA9685_CHANNEL_LIMIT;
            setFromJSON (ChannelId, JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ID_NAME);

            // do we have a valid channel configuration ID?
            if (ChannelId >= OM_SERVO_PCA9685_CHANNEL_LIMIT)
            {
                // if not, flag an error and stop processing this channel
                logcon (String(MN_05) + String(ChannelId) + "'");
                continue;
            }

            ServoPCA9685Channel_t * CurrentOutputChannel = & OutputList[ChannelId];

            setFromJSON (CurrentOutputChannel->Enabled,    JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ENABLED_NAME);
            setFromJSON (CurrentOutputChannel->MinLevel,   JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MINLEVEL_NAME);
            setFromJSON (CurrentOutputChannel->MaxLevel,   JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MAXLEVEL_NAME);
            setFromJSON (CurrentOutputChannel->IsReversed, JsonChannelData, OM_SERVO_PCA9685_CHANNEL_REVERSED);
            setFromJSON (CurrentOutputChannel->Is16Bit,    JsonChannelData, OM_SERVO_PCA9685_CHANNEL_16BITS);
            setFromJSON (CurrentOutputChannel->IsScaled,   JsonChannelData, OM_SERVO_PCA9685_CHANNEL_SCALED);
            setFromJSON (CurrentOutputChannel->HomeValue,  JsonChannelData, OM_SERVO_PCA9685_CHANNEL_HOME);

            // DEBUG_V (String ("ChannelId: ") + String (ChannelId));
            // DEBUG_V (String ("  Enabled: ") + String (CurrentOutputChannel->Enabled));
            // DEBUG_V (String (" MinLevel: ") + String (CurrentOutputChannel->MinLevel));
            // DEBUG_V (String (" MaxLevel: ") + String (CurrentOutputChannel->MaxLevel));
        }

    } while (false);

    bool response = validate ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputServoPCA9685::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    JsonWrite(jsonConfig, OM_SERVO_PCA9685_UPDATE_INTERVAL_NAME, UpdateFrequency);

    JsonArray JsonChannelList = jsonConfig[(char*)OM_SERVO_PCA9685_CHANNELS_NAME].to<JsonArray> ();

    uint8_t ChannelId = 0;
    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.add<JsonObject> ();

        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ID_NAME,       ChannelId);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ENABLED_NAME,  currentServoPCA9685.Enabled);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MINLEVEL_NAME, currentServoPCA9685.MinLevel);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MAXLEVEL_NAME, currentServoPCA9685.MaxLevel);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_REVERSED,      currentServoPCA9685.IsReversed);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_16BITS,        currentServoPCA9685.Is16Bit);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_SCALED,        currentServoPCA9685.IsScaled);
        JsonWrite(JsonChannelData, OM_SERVO_PCA9685_CHANNEL_HOME,          currentServoPCA9685.HomeValue);

        // DEBUG_V (String ("ChannelId: ") + String (ChannelId));
        // DEBUG_V (String ("  Enabled: ") + String (currentServoPCA9685.Enabled));
        // DEBUG_V (String (" MinLevel: ") + String (currentServoPCA9685.MinLevel));
        // DEBUG_V (String (" MaxLevel: ") + String (currentServoPCA9685.MaxLevel));

        ++ChannelId;
    }

    // PrettyPrint(jsonConfig, "Servo");

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void  c_OutputServoPCA9685::GetDriverName (String & sDriverName)
{
    sDriverName = CN_Servo_PCA9685;

} // GetDriverName

//----------------------------------------------------------------------------
uint32_t c_OutputServoPCA9685::Poll ()
{
    // DEBUG_START;

    uint8_t OutputDataIndex = 0;
    ReportNewFrame ();

    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {
        // DEBUG_V (String("OutputDataIndex: ") + String(OutputDataIndex));
        // DEBUG_V (String ("       Enabled: ") + String (currentServoPCA9685.Enabled));
        if (currentServoPCA9685.Enabled)
        {
            uint16_t MaxScaledValue = 255;
            uint16_t MinScaledValue = 0;
            uint16_t newOutputValue = pOutputBuffer[OutputDataIndex];

            if(0 == newOutputValue)
            {
                newOutputValue = currentServoPCA9685.HomeValue;
            }

            if (currentServoPCA9685.Is16Bit)
            {
                // DEBUG_V ("16 Bit Mode");
                newOutputValue  = (pOutputBuffer[(OutputDataIndex * 2) + 0] << 0);
                newOutputValue += (pOutputBuffer[(OutputDataIndex * 2) + 1] << 8);
                MaxScaledValue = uint16_t (-1);
            }

            // DEBUG_V (String ("newOutputValue: ") + String (newOutputValue));
            // DEBUG_V (String (" PreviousValue: ") + String (currentServoPCA9685.PreviousValue));

            if (newOutputValue != currentServoPCA9685.PreviousValue)
            {
                // DEBUG_V (String ("ChannelId: ") + String (OutputDataIndex));
                // DEBUG_V (String (" MinLevel: ") + String (currentServoPCA9685.MinLevel));
                // DEBUG_V (String (" MaxLevel: ") + String (currentServoPCA9685.MaxLevel));
                currentServoPCA9685.PreviousValue = newOutputValue;

                if (currentServoPCA9685.IsReversed)
                {
                    // DEBUG_V (String("Reverse Lookup"));
                    MinScaledValue = MaxScaledValue;
                    MaxScaledValue = 0;
                }

                uint16_t Final_value = newOutputValue;
                if (currentServoPCA9685.IsScaled)
                {
                    // DEBUG_V (String ("Is Scalled"));
                    // DEBUG_V (String ("newOutputValue: ") + String (newOutputValue));
                    // DEBUG_V (String ("      MinLevel: ") + String (currentServoPCA9685.MinLevel));
                    // DEBUG_V (String ("      MaxLevel: ") + String (currentServoPCA9685.MaxLevel));
                    // DEBUG_V (String ("MinScaledValue: ") + String (MinScaledValue));
                    // DEBUG_V (String ("MaxScaledValue: ") + String (MaxScaledValue));

                    uint16_t pulse_width = map (newOutputValue,
                                                MinScaledValue,
                                                MaxScaledValue,
                                                currentServoPCA9685.MinLevel,
                                                currentServoPCA9685.MaxLevel);
                    Final_value = int((float(pulse_width) / float(MicroSecondsInASecond)) * float(UpdateFrequency) * 4096.0);
                    // DEBUG_V (String ("pulse_width: ") + String (pulse_width));
                    // DEBUG_V (String ("Final_value: ") + String (Final_value));
                }
                pwm->setPWM (OutputDataIndex, 0, Final_value);
            }
        }
        ++OutputDataIndex;
    }

    // DEBUG_END;
    return 0;

} // render

#endif // def SUPPORT_OutputType_Servo_PCA9685
