/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*       Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*       See Readme for other info and version history
*
*
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
******************************************************************/

#include "../ESPixelStick.h"
#ifdef SUPPORT_OutputType_Relay

#include <utility>
#include <algorithm>
#include <math.h>

#include "OutputRelay.hpp"
#include "OutputCommon.hpp"

#define Relay_OUTPUT_ENABLED         true
#define Relay_OUTPUT_DISABLED        false
#define Relay_OUTPUT_INVERTED        true
#define Relay_OUTPUT_NOT_INVERTED    false
#define Relay_OUTPUT_PWM             true
#define Relay_OUTPUT_NOT_PWM         false
#define Relay_DEFAULT_TRIGGER_LEVEL  128
#define Relay_DEFAULT_GPIO_ID        ((gpio_num_t)0)
#define RelayPwmHigh                 255
#define RelayPwmLow                  0

#if defined(ARDUINO_ARCH_ESP32)
#   define RelayPwmFrequency         , 12000
#else
#   define RelayPwmFrequency
#endif // defined(ARDUINO_ARCH_ESP32)

static const c_OutputRelay::RelayChannel_t RelayChannelDefaultSettings[] =
{
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency},
};

//----------------------------------------------------------------------------
c_OutputRelay::c_OutputRelay (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio,
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    memcpy((char*)OutputList, (char*)RelayChannelDefaultSettings, SaferArrayByteSize(OutputList));

    // DEBUG_END;
} // c_OutputRelay

//----------------------------------------------------------------------------
c_OutputRelay::~c_OutputRelay ()
{
    // DEBUG_START;

    if(HasBeenInitialized)
    {
        for (RelayChannel_t &currentRelay : OutputList)
        {
            if (currentRelay.Enabled)
            {
                pinMode(currentRelay.GpioId, INPUT);
            }
            currentRelay.Enabled = Relay_OUTPUT_DISABLED;
            currentRelay.GpioId = Relay_DEFAULT_GPIO_ID;
        }
    }

    // DEBUG_END;
} // ~c_OutputRelay

//----------------------------------------------------------------------------
void c_OutputRelay::Begin ()
{
    // DEBUG_START;
    if(!HasBeenInitialized)
    {
        SetOutputBufferSize(Num_Channels);

        validate();

        HasBeenInitialized = true;
    }

    // DEBUG_END;
}

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
bool c_OutputRelay::validate ()
{
    // DEBUG_START;
    bool response = true;

    if ((Num_Channels > OM_RELAY_CHANNEL_LIMIT) || (Num_Channels < 1))
    {
        logcon (CN_stars + String (F (" Requested channel count was not valid. Setting to ")) + OM_RELAY_CHANNEL_LIMIT + " " + CN_stars);
        Num_Channels = OM_RELAY_CHANNEL_LIMIT;
        response = false;
    }

    if (Num_Channels < OM_RELAY_CHANNEL_LIMIT)
    {
        logcon (CN_stars + String (F (" Requested channel count was vot valid. Insuficient number of input channels avaialable ")) + CN_stars);

        for (int ChannelIndex = OM_RELAY_CHANNEL_LIMIT - 1; ChannelIndex > Num_Channels; ChannelIndex--)
        {
            logcon (String (CN_stars + String(F (" Disabling channel '")) + String(ChannelIndex + 1) + "' " + CN_stars));
            OutputList[ChannelIndex].Enabled = false;
        }

        response = false;
    }

    SetOutputBufferSize (Num_Channels);
    uint8_t Channel = 0;
    for (RelayChannel_t & currentRelay : OutputList)
    {
        if (currentRelay.Enabled)
        {
            #if defined(ARDUINO_ARCH_ESP32)
               // assign GPIO to a channel and set the pwm 12 Khz frequency, 8 bit
               ledcAttachPin(currentRelay.GpioId, Channel);
               ledcSetup(Channel, currentRelay.PwmFrequency, 8);
            #else
               pinMode (currentRelay.GpioId, OUTPUT);
            #endif
        }

        if (currentRelay.Pwm)
        {
            if (currentRelay.InvertOutput)
            {
                currentRelay.OffValue = RelayPwmHigh;
                currentRelay.OnValue = RelayPwmLow;
            }
            else
            {
                currentRelay.OffValue = RelayPwmLow;
                currentRelay.OnValue = RelayPwmHigh;
            }
        }
        else
        {
            if (currentRelay.InvertOutput)
            {
                currentRelay.OffValue = HIGH;
                currentRelay.OnValue = LOW;
            }
            else
            {
                currentRelay.OffValue = LOW;
                currentRelay.OnValue = HIGH;
            }
        }

        // DEBUGV (String ("CurrentRelayChanIndex: ") + String (CurrentRelayChanIndex++));
        // DEBUGV (String ("currentRelay.OnValue: ")  + String (currentRelay.OnValue));
        // DEBUGV (String ("currentRelay.OffValue: ") + String (currentRelay.OffValue));
        // DEBUGV (String ("currentRelay.Enabled: ")  + String (currentRelay.Enabled));
        // DEBUGV (String ("currentRelay.GpioId: ")   + String (currentRelay.GpioId));
        ++Channel;
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
bool c_OutputRelay::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;
    int temp; // Holds enums prior to conversion

    do // once
    {
        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);

        // PrettyPrint (jsonConfig, String("c_OutputRelay::SetConfig"));
        setFromJSON (UpdateInterval, jsonConfig, OM_RELAY_UPDATE_INTERVAL_NAME);

        // do we have a channel configuration array?
        if (false == jsonConfig.containsKey (CN_channels))
        {
            // if not, flag an error and stop processing
            logcon (F ("No output channel settings found. Using defaults."));
            break;
        }
        JsonArray JsonChannelList = jsonConfig[CN_channels];

        for (JsonVariant JsonChannelData : JsonChannelList)
        {
            uint8_t ChannelId = OM_RELAY_CHANNEL_LIMIT;
            setFromJSON (ChannelId, JsonChannelData, CN_id);

            // do we have a valid channel configuration ID?
            if (ChannelId >= OM_RELAY_CHANNEL_LIMIT)
            {
                // if not, flag an error and stop processing this channel
                logcon (String(F ("No settings found for channel '")) + String(ChannelId) + "'");
                continue;
            }

            RelayChannel_t * CurrentOutputChannel = & OutputList[ChannelId];

            setFromJSON (CurrentOutputChannel->Enabled,           JsonChannelData, OM_RELAY_CHANNEL_ENABLED_NAME);
            setFromJSON (CurrentOutputChannel->InvertOutput,      JsonChannelData, OM_RELAY_CHANNEL_INVERT_NAME);
            setFromJSON (CurrentOutputChannel->Pwm,               JsonChannelData, OM_RELAY_CHANNEL_PWM_NAME);
            setFromJSON (CurrentOutputChannel->OnOffTriggerLevel, JsonChannelData, CN_trig);
#if defined(ARDUINO_ARCH_ESP32)
            setFromJSON (CurrentOutputChannel->PwmFrequency,      JsonChannelData, CN_Frequency);
#endif // defined(ARDUINO_ARCH_ESP32)

            // DEBUGV (String ("currentRelay.GpioId: ") + String (CurrentOutputChannel->GpioId));
            temp = CurrentOutputChannel->GpioId;
            setFromJSON (temp, JsonChannelData, CN_gid);
            // DEBUGV (String ("temp: ") + String (temp));

            if (temp != CurrentOutputChannel->GpioId)
            {
                // DEBUGV ("Revert Pin to input");
                // The pin has changed. Let go of the old pin
                pinMode (CurrentOutputChannel->GpioId, INPUT);
            }
            CurrentOutputChannel->GpioId = (gpio_num_t)temp;

            // DEBUGV (String ("         CurrentRelayChanIndex: ") + String (ChannelId));
            // DEBUGV (String ("          currentRelay.OnValue: ") + String (CurrentOutputChannel->OnValue));
            // DEBUGV (String ("         currentRelay.OffValue: ") + String (CurrentOutputChannel->OffValue));
            // DEBUGV (String ("          currentRelay.Enabled: ") + String (CurrentOutputChannel->Enabled));
            // DEBUGV (String ("     currentRelay.InvertOutput: ") + String (CurrentOutputChannel->InvertOutput));
            // DEBUGV (String ("currentRelay.OnOffTriggerLevel: ") + String (CurrentOutputChannel->OnOffTriggerLevel));
            // DEBUGV (String ("           currentRelay.GpioId: ") + String (CurrentOutputChannel->GpioId));
            // DEBUGV (String ("              currentRelay.Pwm: ") + String (CurrentOutputChannel->Pwm));
#if defined(ARDUINO_ARCH_ESP32)
            // DEBUGV (String ("     currentRelay.PwmFrequency: ") + String (CurrentOutputChannel->Pwm));
#endif // defined(ARDUINO_ARCH_ESP32)

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
void c_OutputRelay::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[OM_RELAY_UPDATE_INTERVAL_NAME] = UpdateInterval;

    JsonArray JsonChannelList = jsonConfig.createNestedArray (CN_channels);

    uint8_t ChannelId = 0;
    for (RelayChannel_t & currentRelay : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.createNestedObject ();

        JsonChannelData[CN_id]                         = ChannelId;
        JsonChannelData[OM_RELAY_CHANNEL_ENABLED_NAME] = currentRelay.Enabled;
        JsonChannelData[OM_RELAY_CHANNEL_INVERT_NAME]  = currentRelay.InvertOutput;
        JsonChannelData[OM_RELAY_CHANNEL_PWM_NAME]     = currentRelay.Pwm;
        JsonChannelData[CN_trig]                       = currentRelay.OnOffTriggerLevel;
        JsonChannelData[CN_gid]                        = currentRelay.GpioId;

#if defined(ARDUINO_ARCH_ESP32)
        JsonChannelData[CN_Frequency]                  = currentRelay.PwmFrequency;
#endif // defined(ARDUINO_ARCH_ESP32)

        // DEBUGV (String ("CurrentRelayChanIndex: ") + String (ChannelId));
        // DEBUGV (String ("currentRelay.OnValue: ")  + String (currentRelay.OnValue));
        // DEBUGV (String ("currentRelay.OffValue: ") + String (currentRelay.OffValue));
        // DEBUGV (String ("currentRelay.Enabled: ")  + String (currentRelay.Enabled));
        // DEBUGV (String ("currentRelay.GpioId: ")   + String (currentRelay.GpioId));
        // DEBUGV (String ("currentRelay.Pwm: ")      + String (currentRelay.Pwm));

        ++ChannelId;
    }

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void  c_OutputRelay::GetDriverName (String & sDriverName)
{
    sDriverName = F ("Relay");

} // GetDriverName

//----------------------------------------------------------------------------
void c_OutputRelay::Render ()
{
    // DEBUG_START;

    uint8_t OutputDataIndex = 0;

    for (RelayChannel_t & currentRelay : OutputList)
    {
        // DEBUG_V (String("OutputDataIndex: ") + String(OutputDataIndex));
        if (currentRelay.Enabled)
        {
            uint8_t newOutputValue = map (pOutputBuffer[OutputDataIndex], 0, 255, currentRelay.OffValue, currentRelay.OnValue);
            if (currentRelay.Pwm)
            {
                #if defined(ARDUINO_ARCH_ESP32)
                   ledcWrite(OutputDataIndex, newOutputValue);
                #else
                   analogWrite(currentRelay.GpioId, newOutputValue);
                #endif
            }
            else
            {
                newOutputValue = (newOutputValue > currentRelay.OnOffTriggerLevel) ? currentRelay.OnValue : currentRelay.OffValue;
                if (newOutputValue != currentRelay.previousValue)
                {
                    digitalWrite (currentRelay.GpioId, newOutputValue);
                }
            }

            // DEBUGV (String ("OutputDataIndex: ")       + String (OutputDataIndex++));
            // DEBUGV (String ("currentRelay.OnValue: ")  + String (currentRelay.OnValue));
            // DEBUGV (String ("currentRelay.OffValue: ") + String (currentRelay.OffValue));
            // DEBUGV (String ("currentRelay.Enabled: ")  + String (currentRelay.Enabled));
            // DEBUGV (String ("currentRelay.GpioId: ")   + String (currentRelay.GpioId));
            // DEBUGV (String ("newOutputValue: ")        + String (newOutputValue));
            // DEBUGV (String ("Pwm: ")                   + String (currentRelay.Pwm));
            currentRelay.previousValue = newOutputValue;
        }
        ++OutputDataIndex;
    }
    ReportNewFrame ();

    // DEBUG_END;
} // render

#endif // def SUPPORT_OutputType_Relay
