/*
* c_SensorDS18B20.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2018, 2024 Shelby Merrick
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

#include "ESPixelStick.h"
#ifdef SUPPORT_SENSOR_DS18B20

#include "service/SensorDS18B20.h"
#include <DS18B20.h>
#include <TimeLib.h>

DS18B20 Device(ONEWIRE_PIN);

//-----------------------------------------------------------------------------
void c_SensorDS18B20::Begin ()
{
    // DEBUG_START;

    // DEBUG_V(String("Devices: ") + String(Device.getNumberOfDevices()));

    SensorPresent = Device.selectNext();
    // DEBUG_V(String("selected: ") + String(SensorPresent));

    // DEBUG_END;
} // Begin

//-----------------------------------------------------------------------------
void c_SensorDS18B20::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    JsonObject SensorConfig = json[(char*)CN_sensor].to<JsonObject> ();

    JsonWrite(SensorConfig, CN_units, TempUnit);

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
bool c_SensorDS18B20::SetConfig (JsonObject& json)
{
    // DEBUG_START;

    bool ConfigChanged = false;

    do // once
    {
        JsonObject JsonDeviceConfig = json[(char*)CN_sensor];
        if (!JsonDeviceConfig)
        {
            logcon (F ("No Sensor settings found."));
            break;
        }
        uint32_t t = TempUnit_t::TempUnitCentegrade;
        ConfigChanged |= setFromJSON (t, JsonDeviceConfig, CN_units);
        TempUnit = TempUnit_t(t);
        LastReadingTime = 0; // force a reading
    } while(false);

    // DEBUG_V (String ("TempUnit: ") + String (TempUnit));
    // DEBUG_V (String ("ConfigChanged: ") + String (ConfigChanged));

    // DEBUG_END;

    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_SensorDS18B20::GetStatus (JsonObject& json)
{
    // DEBUG_START;

    do // once
    {
        if(false == SensorPresent)
        {
            break;
        }

        JsonObject SensorStatus = json[(char*)CN_sensor].to<JsonObject> ();

        JsonWrite(SensorStatus, CN_reading, String(LastReading) + ((TempUnit == TempUnit_t::TempUnitCentegrade) ? " C" : " F"));

    } while(false);

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_SensorDS18B20::Poll()
{
    // pDEBUG_START;

    do // once
    {
        if(false == SensorPresent)
        {
            break;
        }

        // only read once per sevond
        if(LastReadingTime == now())
        {
            break;
        }

        // remember the last reading
        LastReadingTime = now();

        if (TempUnit == TempUnit_t::TempUnitCentegrade)
        {
            LastReading = Device.getTempC();
        }
        else
        {
            LastReading = Device.getTempF();
        }

    } while(false);

    // pDEBUG_END;
} // Poll

c_SensorDS18B20 SensorDS18B20;

#endif // def SUPPORT_SENSOR_DS18B20
