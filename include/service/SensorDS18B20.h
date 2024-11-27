#pragma once
/*
* c_SensorDS18B20.h
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

#ifdef ESP32
#	include <WiFi.h>
#	include <AsyncUDP.h>
#else
#	error Platform not supported
#endif

class c_SensorDS18B20
{
private:
    enum TempUnit_t
    {
        TempUnitCentegrade = 0,
        TempUnitFahrenheit,
    };
    TempUnit_t TempUnit = TempUnit_t::TempUnitCentegrade;
    uint8_t SensorPresent = 0;
    time_t  LastReadingTime = 0;
    float   LastReading = 0.0;

public:
    c_SensorDS18B20 () {};
    virtual ~c_SensorDS18B20() {}

    void    Begin     ();
    void    Poll      ();
    void    GetConfig (JsonObject& json);
    bool    SetConfig (JsonObject& json);
    void    GetStatus (JsonObject& json);

    void    GetDriverName (String & name) {name = "TempSensor";}
};

extern c_SensorDS18B20 SensorDS18B20;
#endif // def SUPPORT_SENSOR_DS18B20
