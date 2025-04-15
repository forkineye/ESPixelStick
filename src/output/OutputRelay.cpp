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

#include "ESPixelStick.h"
#ifdef SUPPORT_OutputType_Relay

#include "output/OutputRelay.hpp"
#include "output/OutputCommon.hpp"
#include <utility>
#include <algorithm>
#include <math.h>
#include <limits>

#define Relay_OUTPUT_ENABLED         true
#define Relay_OUTPUT_DISABLED        false
#define Relay_OUTPUT_INVERTED        true
#define Relay_OUTPUT_NOT_INVERTED    false
#define Relay_OUTPUT_PWM             true
#define Relay_OUTPUT_NOT_PWM         false
#define Relay_DEFAULT_TRIGGER_LEVEL  128
#define Relay_DEFAULT_GPIO_ID        gpio_num_t(-1)
#define RelayPwmHigh                 255
#define RelayPwmLow                  0

#if defined(ARDUINO_ARCH_ESP32)
#   define RelayPwmFrequency         , 12000
#else
#   define RelayPwmFrequency
#endif // defined(ARDUINO_ARCH_ESP32)

static const c_OutputRelay::RelayChannel_t RelayChannelDefaultSettings[] =
{
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 0},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 1},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 2},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 3},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 4},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 5},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 6},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_OUTPUT_NOT_PWM, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH, HIGH RelayPwmFrequency, 7},
};

//----------------------------------------------------------------------------
c_OutputRelay::c_OutputRelay (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio,
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    memcpy((char*)OutputList, (char*)RelayChannelDefaultSettings, sizeof(OutputList));

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
            if (gpio_num_t(-1) != currentRelay.GpioId)
            {
                if (currentRelay.Enabled)
                {
                    ResetGpio(currentRelay.GpioId);
                    pinMode(currentRelay.GpioId, INPUT);
                }
                currentRelay.Enabled = Relay_OUTPUT_DISABLED;
                currentRelay.GpioId = Relay_DEFAULT_GPIO_ID;
                currentRelay.httpEnabled = Relay_OUTPUT_DISABLED;
            }
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
            logcon (String (CN_stars + String(MN_03) + String(ChannelIndex + 1) + "' " + CN_stars));
            OutputList[ChannelIndex].Enabled = Relay_OUTPUT_DISABLED;
            OutputList[ChannelIndex].httpEnabled = Relay_OUTPUT_DISABLED;
        }

        response = false;
    }

    SetOutputBufferSize (Num_Channels);
    for (RelayChannel_t & currentRelay : OutputList)
    {
        if (currentRelay.Enabled && (gpio_num_t(-1) != currentRelay.GpioId))
        {
            // DEBUG_V("Init GPIO as a generic output");
            ResetGpio(currentRelay.GpioId);
            pinMode (currentRelay.GpioId, OUTPUT);
            #if defined(ARDUINO_ARCH_ESP32)
            if(currentRelay.Pwm)
            {
                // DEBUG_V("Init GPIO as a PWM output");
                // assign GPIO to a channel and set the pwm 12 Khz frequency, 8 bit
                ledcAttachPin(currentRelay.GpioId, currentRelay.ChannelIndex);
                ledcSetup(currentRelay.ChannelIndex, currentRelay.PwmFrequency, 8);
            }
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
        // PrettyPrint (jsonConfig, String("c_OutputRelay::SetConfig"));
        setFromJSON (UpdateInterval, jsonConfig, OM_RELAY_UPDATE_INTERVAL_NAME);

        // do we have a channel configuration array?
        JsonArray JsonChannelList = jsonConfig[(char*)CN_channels];
        if (!JsonChannelList)
        {
            // if not, flag an error and stop processing
            logcon (F ("No output channel settings found. Using defaults."));
            break;
        }

        for (JsonVariant ChannelData : JsonChannelList)
        {
            JsonObject JsonChannelData = ChannelData.as<JsonObject>();

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
            setFromJSON (CurrentOutputChannel->httpEnabled,       JsonChannelData, CN_enhttp);
#if defined(ARDUINO_ARCH_ESP32)
            setFromJSON (CurrentOutputChannel->PwmFrequency,      JsonChannelData, CN_Frequency);
#endif // defined(ARDUINO_ARCH_ESP32)

            // DEBUGV (String ("currentRelay.GpioId: ") + String (CurrentOutputChannel->GpioId));
            temp = CurrentOutputChannel->GpioId;
            setFromJSON (temp, JsonChannelData, CN_gid);
            // DEBUGV (String ("temp: ") + String (temp));

            if ((gpio_num_t(-1) != CurrentOutputChannel->GpioId) && (temp != CurrentOutputChannel->GpioId))
            {
                // DEBUGV ("Revert Pin to input");
                // The pin has changed. Let go of the old pin
                ResetGpio(CurrentOutputChannel->GpioId);
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

    JsonWrite(jsonConfig, OM_RELAY_UPDATE_INTERVAL_NAME, UpdateInterval);

    JsonArray JsonChannelList = jsonConfig[(char*)CN_channels].to<JsonArray> ();

    uint8_t ChannelId = 0;
    for (RelayChannel_t & currentRelay : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.add<JsonObject> ();

        JsonWrite(JsonChannelData, CN_id,                         ChannelId);
        JsonWrite(JsonChannelData, OM_RELAY_CHANNEL_ENABLED_NAME, currentRelay.Enabled);
        JsonWrite(JsonChannelData, OM_RELAY_CHANNEL_INVERT_NAME,  currentRelay.InvertOutput);
        JsonWrite(JsonChannelData, OM_RELAY_CHANNEL_PWM_NAME,     currentRelay.Pwm);
        JsonWrite(JsonChannelData, CN_trig,                       currentRelay.OnOffTriggerLevel);
        JsonWrite(JsonChannelData, CN_gid,                        int(currentRelay.GpioId));
        JsonWrite(JsonChannelData, CN_enhttp,                     currentRelay.httpEnabled);

#if defined(ARDUINO_ARCH_ESP32)
        JsonWrite(JsonChannelData, CN_Frequency,                  currentRelay.PwmFrequency);
#endif // defined(ARDUINO_ARCH_ESP32)

        // DEBUGV (String ("CurrentRelayChanIndex: ") + String (ChannelId));
        // DEBUGV (String ("currentRelay.OnValue: ")  + String (currentRelay.OnValue));
        // DEBUGV (String ("currentRelay.OffValue: ") + String (currentRelay.OffValue));
        // DEBUGV (String ("currentRelay.Enabled: ")  + String (currentRelay.Enabled));
        // DEBUGV (String ("currentRelay.GpioId: ")   + String (currentRelay.GpioId));
        // DEBUGV (String ("currentRelay.Pwm: ")      + String (currentRelay.Pwm));

        ++ChannelId;
    }

    // PrettyPrint(jsonConfig, "Get Relay Config");

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputRelay::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputCommon::BaseGetStatus(jsonStatus);
    JsonArray JsonChannelList = jsonStatus[(char*)CN_Relay].to<JsonArray> ();

    uint8_t ChannelId = 0;
    for (RelayChannel_t & currentRelay : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.add<JsonObject> ();

        JsonWrite(JsonChannelData, CN_id,          ChannelId);
        JsonWrite(JsonChannelData, CN_activevalue, currentRelay.previousValue);

        ++ChannelId;
    }

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
void  c_OutputRelay::GetDriverName (String & sDriverName)
{
    // DEBUG_START;

    sDriverName = CN_Relay;

    // DEBUG_END;
} // GetDriverName

//----------------------------------------------------------------------------
uint32_t c_OutputRelay::Poll ()
{
    // DEBUG_START;

    uint8_t OutputDataIndex = 0;
    uint8_t newOutputValue = 0;

    for (RelayChannel_t & currentRelay : OutputList)
    {
        // DEBUG_V (String("OutputDataIndex: ") + String(OutputDataIndex));
        // DEBUG_V (String("        Enabled: ") + String(currentRelay.Enabled));
        if ((currentRelay.Enabled) &&
            (gpio_num_t(-1) != currentRelay.GpioId) &&
            (!currentRelay.httpEnabled))
        {
            OutputValue(currentRelay, pOutputBuffer[OutputDataIndex]);
        }
        ++OutputDataIndex;
    }
    ReportNewFrame ();

    // DEBUG_END;
    return 0;
} // render

//----------------------------------------------------------------------------
void c_OutputRelay::OutputValue(RelayChannel_t & currentRelay, uint8_t NewValue)
{
    // DEBUG_START;

    // DEBUG_V (String(" rawOutputValue: ") + String(NewValue));
    if (currentRelay.Pwm)
    {
        uint8_t newOutputValue = map (NewValue, 0, 255, currentRelay.OffValue, currentRelay.OnValue);
        // DEBUG_V (String(" newOutputValue: ") + String(newOutputValue));
        if (newOutputValue != currentRelay.previousValue)
        {
            // DEBUG_V (String(" newOutputValue: ") + String(newOutputValue));
            #if defined(ARDUINO_ARCH_ESP32)
            ledcWrite(currentRelay.ChannelIndex, newOutputValue);
            #else
            analogWrite(currentRelay.GpioId, newOutputValue);
            #endif
            currentRelay.previousValue = newOutputValue;
        }
    }
    else
    {
        uint8_t newOutputValue = (NewValue > currentRelay.OnOffTriggerLevel) ? currentRelay.OnValue : currentRelay.OffValue;
        // DEBUG_V (String(" OnOffTriggerLevel: ") + String(currentRelay.OnOffTriggerLevel));
        // DEBUG_V (String("    newOutputValue: ") + String(newOutputValue));
        if (newOutputValue != currentRelay.previousValue)
        {
            // DEBUG_V (String("OutputDataIndex: ") + String(currentRelay.ChannelIndex));
            // DEBUG_V("Write New Value");
            // DEBUG_V (String("OnOffTriggerLevel: ") + String(currentRelay.OnOffTriggerLevel));
            // DEBUG_V (String("         NewValue: ") + String(NewValue));
            // DEBUG_V (String("   newOutputValue: ") + String(newOutputValue));
            // DEBUG_V (String("           GpioId: ") + String(currentRelay.GpioId));
            digitalWrite (uint8_t(currentRelay.GpioId), newOutputValue);
            currentRelay.previousValue = newOutputValue;
        }
    }

    // DEBUGV (String ("OutputDataIndex: ")       + String (OutputDataIndex++));
    // DEBUGV (String ("currentRelay.OnValue: ")  + String (currentRelay.OnValue));
    // DEBUGV (String ("currentRelay.OffValue: ") + String (currentRelay.OffValue));
    // DEBUGV (String ("currentRelay.Enabled: ")  + String (currentRelay.Enabled));
    // DEBUGV (String ("currentRelay.GpioId: ")   + String (currentRelay.GpioId));
    // DEBUGV (String ("newOutputValue: ")        + String (newOutputValue));
    // DEBUGV (String ("Pwm: ")                   + String (currentRelay.Pwm));
    // DEBUG_END;
} // OutputValue

//----------------------------------------------------------------------------
bool c_OutputRelay::ValidateGpio (gpio_num_t ConsoleTxGpio, gpio_num_t ConsoleRxGpio)
{
    // DEBUG_START;

    bool response = false;

    for (RelayChannel_t & currentRelay : OutputList)
    {
        if (currentRelay.Enabled)
        {
            response |= ((currentRelay.GpioId == ConsoleTxGpio) || (currentRelay.GpioId == ConsoleRxGpio));
        }
    }

    // DEBUG_END;
    return response;

} // ValidateGpio

//----------------------------------------------------------------------------
void c_OutputRelay::RelayUpdate (uint8_t RelayId, String & NewValue, String & Response)
{
    // DEBUG_START;

    // DEBUG_V(String(" RelayId: ") + String(RelayId));
    // DEBUG_V(String("NewValue: ") + NewValue);

    uint8_t OutputIntensityValue = 0;
    do // once
    {
        // zero justify the id
        RelayId --;
        if(RelayId >= OM_RELAY_CHANNEL_LIMIT)
        {
            Response = F("Invalid relay port ID");
            break;
        }

        if(NewValue.equalsIgnoreCase("on"))
        {
            // DEBUG_V("ON");
            OutputIntensityValue = UINT8_MAX;
        }
        else if(NewValue.equalsIgnoreCase("off"))
        {
            // DEBUG_V("OFF");
            OutputIntensityValue = uint8_t(0);
        }
        else
        {
            int32_t temp = NewValue.toInt();
            // DEBUG_V(String("    temp: ") + String(temp));
            // DEBUG_V(String("UINT8_MAX: ") + String(UINT8_MAX));

            if(UINT8_MAX < temp)
            {
                Response = F("Invalid output value");
                break;
            }
            OutputIntensityValue = uint8_t(temp);
        }

        // DEBUG_V(String("OutputIntensityValue: ") + String(OutputIntensityValue));

        if(!OutputList[RelayId].Enabled)
        {
            Response = F("Relay Port is not enabled");
            break;
        }

        if(!OutputList[RelayId].httpEnabled)
        {
            Response = F("HTTP Relay Port support is not enabled");
            break;
        }

        // update the output
        OutputValue(OutputList[RelayId], OutputIntensityValue);

        Response = F("OK");

    } while(false);

    // DEBUG_V(String("Response: ") + Response);

    // DEBUG_END;
} // RelayUpdate

#endif // def SUPPORT_OutputType_Relay
