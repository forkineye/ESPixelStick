#pragma once
/*
* MainRelayMgr.hpp - Main Relay Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2025 Shelby Merrick
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
*   This class manages a main relay output that can be toggled on when
*   data is being received and off when no data is present.
*
*/

#include "ESPixelStick.h"

#ifdef SUPPORT_MAIN_RELAY

class c_MainRelayMgr
{
public:
    c_MainRelayMgr();
    virtual ~c_MainRelayMgr();

    void Begin();
    void GetConfig(JsonObject& JsonData);
    void SetConfig(JsonObject& JsonData);
    void SetRelayState(bool Active);
    void GetDriverName(String& Name) { Name = "MainRelayMgr"; }

private:
    gpio_num_t  RelayGpio = gpio_num_t::GPIO_NUM_NC;
    bool        RelayInvert = false;
    bool        HasBeenInitialized = false;

}; // c_MainRelayMgr

extern c_MainRelayMgr MainRelayMgr;

#endif // SUPPORT_MAIN_RELAY
