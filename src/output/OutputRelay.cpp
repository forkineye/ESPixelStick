/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by copyright 2015 - 2026 Shelby Merrick
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
#ifdef SUPPORT_OutputProtocol_Relay

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
#   define RelayPwmFrequency         12000
#else
#   define RelayPwmFrequency         19000
#endif // defined(ARDUINO_ARCH_ESP32)

//----------------------------------------------------------------------------
c_OutputRelay::c_OutputRelay (OM_OutputPortDefinition_t & OutputPortDefinition,
                              c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputCommon(OutputPortDefinition, outputType)
{
    // DEBUG_START;

    httpEnabled = Relay_OUTPUT_DISABLED;
    InvertOutput = Relay_OUTPUT_INVERTED;
    Pwm = Relay_OUTPUT_NOT_PWM;
    OnOffTriggerLevel = Relay_DEFAULT_TRIGGER_LEVEL;
    GpioId = Relay_DEFAULT_GPIO_ID;
    OnValue = LOW;
    OffValue = HIGH;
    previousValue = HIGH;
#if defined(ARDUINO_ARCH_ESP32)
    PwmFrequency = RelayPwmFrequency;
#endif // defined(ARDUINO_ARCH_ESP32)
    UpdateInterval = 0;

    // DEBUG_END;
} // c_OutputRelay

//----------------------------------------------------------------------------
c_OutputRelay::~c_OutputRelay ()
{
    // DEBUG_START;

    if(HasBeenInitialized)
    {
        if (gpio_num_t(-1) != GpioId)
        {
            // if (currentRelay.Enabled)
            {
                ResetGpio(GpioId);
                pinMode(GpioId, INPUT);
            }
            GpioId = Relay_DEFAULT_GPIO_ID;
            httpEnabled = Relay_OUTPUT_DISABLED;
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
        SetOutputBufferSize(1);

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
    SetOutputBufferSize (1);
    if (gpio_num_t(-1) != GpioId)
    {
        // DEBUG_V("Init GPIO as a generic output");
        ResetGpio(GpioId);
        pinMode (GpioId, OUTPUT);
        #if defined(ARDUINO_ARCH_ESP32)
        if(Pwm)
        {
            // DEBUG_V("Init GPIO as a PWM output");
            // assign GPIO to a channel and set the pwm 12 Khz frequency, 8 bit
            #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            ledcAttach(GpioId, GetOutputPortId(), 8);
            #else
            ledcAttachPin(GpioId, GetOutputPortId());
            ledcSetup(GetOutputPortId(), PwmFrequency, 8);
            #endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        }
        #endif
    }

    if (Pwm)
    {
        if (InvertOutput)
        {
            OffValue = RelayPwmHigh;
            OnValue = RelayPwmLow;
        }
        else
        {
            OffValue = RelayPwmLow;
            OnValue = RelayPwmHigh;
        }
    }
    else
    {
        if (InvertOutput)
        {
            OffValue = HIGH;
            OnValue = LOW;
        }
        else
        {
            OffValue = LOW;
            OnValue = HIGH;
        }
    }

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

            uint8_t ChannelId = 0;
            setFromJSON (ChannelId, JsonChannelData, CN_id);

            // do we have a valid channel configuration ID?
            if (ChannelId != 0)
            {
                // if not, flag an error and stop processing this channel
                logcon (String(F ("No settings found for channel '")) + String(ChannelId) + "'");
                continue;
            }

            setFromJSON (InvertOutput,      JsonChannelData, OM_RELAY_CHANNEL_INVERT_NAME);
            setFromJSON (Pwm,               JsonChannelData, OM_RELAY_CHANNEL_PWM_NAME);
            setFromJSON (OnOffTriggerLevel, JsonChannelData, CN_trig);
            setFromJSON (httpEnabled,       JsonChannelData, CN_enhttp);
            #if defined(ARDUINO_ARCH_ESP32)
            setFromJSON (PwmFrequency,      JsonChannelData, CN_Frequency);
            #endif // defined(ARDUINO_ARCH_ESP32)

            // DEBUGV (String ("GpioId: ") + String (GpioId));
            temp = GpioId;
            setFromJSON (temp, JsonChannelData, CN_gid);
            // DEBUGV (String ("temp: ") + String (temp));
            GpioId = (gpio_num_t)temp;

            // DEBUGV (String ("          OnValue: ") + String (OnValue));
            // DEBUGV (String ("         OffValue: ") + String (OffValue));
            // DEBUGV (String ("     InvertOutput: ") + String (InvertOutput));
            // DEBUGV (String ("OnOffTriggerLevel: ") + String (OnOffTriggerLevel));
            // DEBUGV (String ("           GpioId: ") + String (GpioId));
            // DEBUGV (String ("              Pwm: ") + String (Pwm));
            #if defined(ARDUINO_ARCH_ESP32)
            // DEBUGV (String ("     PwmFrequency: ") + String (Pwm));
            #endif // defined(ARDUINO_ARCH_ESP32)
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
    JsonObject JsonChannelData = JsonChannelList.add<JsonObject> ();

    JsonWrite(JsonChannelData, CN_id,                         0);
    JsonWrite(JsonChannelData, OM_RELAY_CHANNEL_INVERT_NAME,  InvertOutput);
    JsonWrite(JsonChannelData, OM_RELAY_CHANNEL_PWM_NAME,     Pwm);
    JsonWrite(JsonChannelData, CN_trig,                       OnOffTriggerLevel);
    JsonWrite(JsonChannelData, CN_gid,                        int(GpioId));
    JsonWrite(JsonChannelData, CN_enhttp,                     httpEnabled);

    #if defined(ARDUINO_ARCH_ESP32)
    JsonWrite(JsonChannelData, CN_Frequency,                  PwmFrequency);
    #endif // defined(ARDUINO_ARCH_ESP32)

    // DEBUGV (String ("OnValue: ")  + String (OnValue));
    // DEBUGV (String ("OffValue: ") + String (OffValue));
    // DEBUGV (String ("Enabled: ")  + String (Enabled));
    // DEBUGV (String ("GpioId: ")   + String (GpioId));
    // DEBUGV (String ("Pwm: ")      + String (Pwm));

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
    JsonObject JsonChannelData = JsonChannelList.add<JsonObject> ();

    JsonWrite(JsonChannelData, CN_id,          ChannelId);
    JsonWrite(JsonChannelData, CN_activevalue, previousValue);

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

    if ((Relay_DEFAULT_GPIO_ID != GpioId) && (!httpEnabled))
    {
        OutputValue(pOutputBuffer[0]);
    }

    ISR_ReportNewFrame ();

    // DEBUG_END;
    return 0;
} // render

//----------------------------------------------------------------------------
void c_OutputRelay::OutputValue(uint8_t NewValue)
{
    // DEBUG_START;

    // DEBUG_V (String(" rawOutputValue: ") + String(NewValue));
    if (Pwm)
    {
        uint8_t newOutputValue = map (NewValue, 0, 255, OffValue, OnValue);
        // DEBUG_V (String(" newOutputValue: ") + String(newOutputValue));
        if (newOutputValue != previousValue)
        {
            // DEBUG_V (String(" newOutputValue: ") + String(newOutputValue));
            #if defined(ARDUINO_ARCH_ESP32)
            ledcWrite(GetOutputPortId(), newOutputValue);
            #else
            analogWrite(GpioId, newOutputValue);
            #endif
            previousValue = newOutputValue;
        }
    }
    else
    {
        uint8_t newOutputValue = (NewValue > OnOffTriggerLevel) ? OnValue : OffValue;
        // DEBUG_V (String(" OnOffTriggerLevel: ") + String(currentRelay.OnOffTriggerLevel));
        // DEBUG_V (String("    newOutputValue: ") + String(newOutputValue));
        if (newOutputValue != previousValue)
        {
            // DEBUG_V (String("OutputDataIndex: ") + String(currentRelay.ChannelIndex));
            // DEBUG_V("Write New Value");
            // DEBUG_V (String("OnOffTriggerLevel: ") + String(OnOffTriggerLevel));
            // DEBUG_V (String("         NewValue: ") + String(NewValue));
            // DEBUG_V (String("   newOutputValue: ") + String(newOutputValue));
            // DEBUG_V (String("           GpioId: ") + String(GpioId));
            digitalWrite (uint8_t(GpioId), newOutputValue);
            previousValue = newOutputValue;
        }
    }

    // DEBUGV (String ("OnValue: ")         + String (OnValue));
    // DEBUGV (String ("OffValue: ")        + String (OffValue));
    // DEBUGV (String ("Enabled: ")         + String (Enabled));
    // DEBUGV (String ("GpioId: ")          + String (GpioId));
    // DEBUGV (String ("newOutputValue: ")  + String (newOutputValue));
    // DEBUGV (String ("Pwm: ")             + String (Pwm));
    // DEBUG_END;
} // OutputValue

//----------------------------------------------------------------------------
bool c_OutputRelay::ValidateGpio (gpio_num_t ConsoleTxGpio, gpio_num_t ConsoleRxGpio)
{
    // DEBUG_START;

    bool response = false;

    response |= ((GpioId == ConsoleTxGpio) || (GpioId == ConsoleRxGpio));

    // DEBUG_END;
    return response;

} // ValidateGpio

//----------------------------------------------------------------------------
void c_OutputRelay::RelayUpdate (String & NewValue, String & Response)
{
    // DEBUG_START;
    // DEBUG_V(String("NewValue: ") + NewValue);

    uint8_t OutputIntensityValue = 0;
    do // once
    {
        if(!httpEnabled)
        {
            Response = F("HTTP Relay Port support is not enabled");
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
            uint32_t temp = NewValue.toInt();
            // DEBUG_V(String("     temp: ") + String(temp));
            // DEBUG_V(String("UINT8_MAX: ") + String(UINT8_MAX));

            if(UINT8_MAX < temp)
            {
                Response = F("Invalid output value");
                break;
            }
            OutputIntensityValue = uint8_t(temp);
        }

        // update the output
        OutputValue(OutputIntensityValue);

        Response = F("OK");

    } while(false);

    // DEBUG_V(String("Response: ") + Response);

    // DEBUG_END;
} // RelayUpdate

#endif // def SUPPORT_OutputProtocol_Relay
