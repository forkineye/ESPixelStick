#pragma once
/*
* WiFiMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP8266
#   include <ESP8266WiFi.h>
#else
#   include <WiFi.h>
#endif // def ARDUINO_ARCH_ESP8266


class c_WiFiMgr
{
public:
    c_WiFiMgr ();
    virtual ~c_WiFiMgr ();

    void      Begin           (config_t* NewConfig); ///< set up the operating environment based on the current config (or defaults)
    int       ValidateConfig  (config_t * NewConfig);
    IPAddress getIpAddress    () { return CurrentIpAddress; }
    IPAddress getIpSubNetMask () { return CurrentSubnetMask; }
    void      GetStatus       (JsonObject & jsonStatus);
    void      connectWifi     ();

private:
#define CLIENT_TIMEOUT  15      ///< In station/client mode try to connection for 15 seconds
#define AP_TIMEOUT      60      ///< In AP mode, wait 60 seconds for a connection or reboot

#ifdef ARDUINO_ARCH_ESP8266
    WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
    WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
#endif
    config_t           *config = nullptr;                           // Current configuration
    IPAddress           CurrentIpAddress  = IPAddress(0,0,0,0);
    IPAddress           CurrentSubnetMask = IPAddress (0, 0, 0, 0);
    Ticker              wifiTicker;                                 // Ticker to handle WiFi

    void initWifi ();

#ifdef ARDUINO_ARCH_ESP8266
    void onWiFiConnect (const WiFiEventStationModeGotIP& event);
    void onWiFiDisconnect (const WiFiEventStationModeDisconnected& event);
#else
    void onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info);
    void onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info);
#endif

protected:

}; // c_WiFiMgr

extern c_WiFiMgr WiFiMgr;
