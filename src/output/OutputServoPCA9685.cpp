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

#include "../ESPixelStick.h"
#include <utility>
#include <algorithm>
#include <math.h>

#include "OutputServoPCA9685.hpp"

#define SERVO_PCA9685_OUTPUT_ENABLED         true
#define SERVO_PCA9685_OUTPUT_DISABLED        false
#define SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH 650
#define SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH 2350

#define SERVO_PCA9685_DEFAULT_PULSE_WIDTH    0

static const c_OutputServoPCA9685::ServoPCA9685Channel_t ServoPCA9685ChannelDefaultSettings[] =
{
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
    {SERVO_PCA9685_OUTPUT_DISABLED, SERVO_PCA9685_OUTPUT_MIN_PULSE_WIDTH, SERVO_PCA9685_OUTPUT_MAX_PULSE_WIDTH, SERVO_PCA9685_DEFAULT_PULSE_WIDTH },
};

//----------------------------------------------------------------------------
c_OutputServoPCA9685::c_OutputServoPCA9685 (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio, 
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    memcpy((char*)OutputList, 
        (char*)ServoPCA9685ChannelDefaultSettings, 
        sizeof(OutputList));

    // DEBUG_END;
} // c_OutputServoPCA9685

//----------------------------------------------------------------------------
c_OutputServoPCA9685::~c_OutputServoPCA9685 ()
{
    // DEBUG_START;

    // Terminate the I2C bus

    // DEBUG_END;
} // ~c_OutputServoPCA9685

//----------------------------------------------------------------------------
void c_OutputServoPCA9685::Begin ()
{
    // DEBUG_START;
    String DriverName = ""; GetDriverName (DriverName);
    LOG_PORT.println (String (F ("** ")) + DriverName + F(" Initialization for Chan: ") + String (OutputChannelId) + F(" **"));

    SetOutputBufferSize (Num_Channels);

    pwm.begin ();
    pwm.setPWMFreq (UpdateFrequency);

    validate ();

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
/*
*   Validate that the current values meet our needs
*
*   needs
*       data set in the class elements
*   returns
*       true - no issues found
*       false - had an issue and had to fix things
*/
bool c_OutputServoPCA9685::validate ()
{
    // DEBUG_START;
    bool response = true;

    if ((Num_Channels > OM_SERVO_PCA9685_CHANNEL_LIMIT) || (Num_Channels < 1))
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Setting to ")) + OM_SERVO_PCA9685_CHANNEL_LIMIT + F(" ***"));
        Num_Channels = OM_SERVO_PCA9685_CHANNEL_LIMIT;
        response = false;
    }

    if (Num_Channels < OM_SERVO_PCA9685_CHANNEL_LIMIT)
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Insuficient number of input channels avaialable ***")));

        for (int ChannelIndex = OM_SERVO_PCA9685_CHANNEL_LIMIT - 1; ChannelIndex > Num_Channels; ChannelIndex--)
        {
            LOG_PORT.println (String (String(F ("*** Disabling channel '")) + String(ChannelIndex + 1) + String("' ***")));
            OutputList[ChannelIndex].Enabled = false;
        }

        response = false;
    }

    SetOutputBufferSize (Num_Channels);
    uint8_t CurrentServoPCA9685ChanIndex = 0;
    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {

    } // for each output channel

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
    int temp; // Holds enums prior to conversion

    do // once
    {
        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);

        // PrettyPrint (jsonConfig, String("c_OutputServoPCA9685::SetConfig"));
        setFromJSON (UpdateFrequency, jsonConfig, OM_SERVO_PCA9685_UPDATE_INTERVAL_NAME);
        pwm.setPWMFreq (UpdateFrequency);

        // do we have a channel configuration array?
        if (false == jsonConfig.containsKey (OM_SERVO_PCA9685_CHANNELS_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Servo PCA9685 Output Channel Settings Found. Using Defaults"));
            break;
        }
        JsonArray JsonChannelList = jsonConfig[OM_SERVO_PCA9685_CHANNELS_NAME];

        for (JsonVariant JsonChannelData : JsonChannelList)
        {
            uint8_t ChannelId = OM_SERVO_PCA9685_CHANNEL_LIMIT;
            setFromJSON (ChannelId, JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ID_NAME);

            // do we have a valid channel configuration ID?
            if (ChannelId >= OM_SERVO_PCA9685_CHANNEL_LIMIT)
            {
                // if not, flag an error and stop processing this channel
                LOG_PORT.println (String(F ("No Servo PCA9685 Output Channel Settings Found for Channel '")) + String(ChannelId) + "'");
                continue;
            }

            ServoPCA9685Channel_t * CurrentOutputChannel = & OutputList[ChannelId];

            setFromJSON (CurrentOutputChannel->Enabled,  JsonChannelData, OM_SERVO_PCA9685_CHANNEL_ENABLED_NAME);
            setFromJSON (CurrentOutputChannel->MinLevel, JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MINLEVEL_NAME);
            setFromJSON (CurrentOutputChannel->MaxLevel, JsonChannelData, OM_SERVO_PCA9685_CHANNEL_MAXLEVEL_NAME);

            // DEBUGV (String ("ChannelId: ") + String (ChannelId));
            // DEBUGV (String ("  Enabled: ") + String (CurrentOutputChannel->Enabled));
            // DEBUGV (String (" MinLevel: ") + String (CurrentOutputChannel->MinLevel));
            // DEBUGV (String (" MaxLevel: ") + String (CurrentOutputChannel->MaxLevel));
        
            ++ChannelId;
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

    jsonConfig[OM_SERVO_PCA9685_UPDATE_INTERVAL_NAME] = UpdateFrequency;

    JsonArray JsonChannelList = jsonConfig.createNestedArray (OM_SERVO_PCA9685_CHANNELS_NAME);

    uint8_t ChannelId = 0;
    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.createNestedObject ();

        JsonChannelData[OM_SERVO_PCA9685_CHANNEL_ID_NAME]       = ChannelId;
        JsonChannelData[OM_SERVO_PCA9685_CHANNEL_ENABLED_NAME]  = currentServoPCA9685.Enabled;
        JsonChannelData[OM_SERVO_PCA9685_CHANNEL_MINLEVEL_NAME] = currentServoPCA9685.MinLevel;
        JsonChannelData[OM_SERVO_PCA9685_CHANNEL_MAXLEVEL_NAME] = currentServoPCA9685.MaxLevel;

        // DEBUGV (String ("ChannelId: ") + String (ChannelId));
        // DEBUGV (String ("  Enabled: ") + String (currentServoPCA9685.Enabled));
        // DEBUGV (String (" MinLevel: ") + String (currentServoPCA9685.MinLevel));
        // DEBUGV (String (" MaxLevel: ") + String (currentServoPCA9685.MaxLevel));

        ++ChannelId;
    }

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void  c_OutputServoPCA9685::GetDriverName (String & sDriverName)
{
    sDriverName = F ("Servo PCA9685");

} // GetDriverName

//----------------------------------------------------------------------------
void c_OutputServoPCA9685::Render ()
{
    // DEBUG_START;

    uint8_t OutputDataIndex = 0;

    for (ServoPCA9685Channel_t & currentServoPCA9685 : OutputList)
    {
        // DEBUG_V (String("OutputDataIndex: ") + String(OutputDataIndex));
        if (currentServoPCA9685.Enabled)
        {
            uint8_t newOutputValue = pOutputBuffer[OutputDataIndex];
            if (newOutputValue != currentServoPCA9685.PreviousValue)
            {
                // DEBUGV (String ("ChannelId: ") + String (ChannelId));
                // DEBUGV (String ("  Enabled: ") + String (currentServoPCA9685.Enabled));
                // DEBUGV (String (" MinLevel: ") + String (currentServoPCA9685.MinLevel));
                // DEBUGV (String (" MaxLevel: ") + String (currentServoPCA9685.MaxLevel));
                currentServoPCA9685.PreviousValue = newOutputValue;

                // send data to pwm

                uint16_t pulse_width = 0;
                uint16_t Final_value = 0;
                pulse_width = map (newOutputValue, 0, 255, currentServoPCA9685.MinLevel, currentServoPCA9685.MaxLevel);
                Final_value = int (float (pulse_width) / 1000000.0 * float(UpdateFrequency) * 4096.0);
                pwm.setPWM (OutputDataIndex, 0, Final_value);
            }
        }
        ++OutputDataIndex;
    }

    // DEBUG_END;
} // render
