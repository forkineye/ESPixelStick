#pragma once
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
******************************************************************
*
*   This is a derived class that converts data in the output buffer into 
*   Servo States and then outputs on I2C
*
*/

#include "OutputCommon.hpp"
#include <Adafruit_PWMServoDriver.h>

class c_OutputServoPCA9685 : public c_OutputCommon  
{
public:
    typedef struct ServoPCA9685Channel_s
    {
        bool        Enabled;
        uint8_t     MinLevel;
        uint8_t     MaxLevel;
        uint8_t     PreviousValue;

    } ServoPCA9685Channel_t;

    // These functions are inherited from c_OutputCommon
    c_OutputServoPCA9685 (c_OutputMgr::e_OutputChannelIds OutputChannelId, 
                   gpio_num_t outputGpio, 
                   uart_port_t uart,
                   c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputServoPCA9685 ();

    // functions to be provided by the derived class
    void Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void Render ();                                        ///< Call from loop(),  renders output data
    void GetDriverName (String& sDriverName);
    void GetStatus (ArduinoJson::JsonObject & jsonStatus) { c_OutputCommon::GetStatus (jsonStatus); }
    uint16_t GetNumChannelsNeeded () { return Num_Channels; }

private:
#   define OM_SERVO_PCA9685_CHANNEL_LIMIT           16
#   define OM_SERVO_PCA9685_UPDATE_INTERVAL_NAME    F("updateinterval")
#   define OM_SERVO_PCA9685_CHANNELS_NAME           F("channels")
#   define OM_SERVO_PCA9685_CHANNEL_ENABLED_NAME    F("enabled")
#   define OM_SERVO_PCA9685_CHANNEL_MINLEVEL_NAME   F("MinLevel")
#   define OM_SERVO_PCA9685_CHANNEL_MAXLEVEL_NAME   F("MaxLevel")
#   define OM_SERVO_PCA9685_CHANNEL_GPIO_NAME       F("gpioid")
#   define OM_SERVO_PCA9685_CHANNEL_ID_NAME         F("id")
#   define SERVO_PCA9685_UPDATE_FREQUENCY           50

    bool    validate ();

    // config data
    ServoPCA9685Channel_t   OutputList[OM_SERVO_PCA9685_CHANNEL_LIMIT];
    uint16_t                UpdateFrequenct = 0;
    Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver ();
    uint8_t                 UpdateFrequenct = SERVO_PCA9685_UPDATE_FREQUENCY;

    // non config data
    String      OutputName;
    uint16_t    Num_Channels = OM_SERVO_PCA9685_CHANNEL_LIMIT;

}; // c_OutputServoPCA9685

