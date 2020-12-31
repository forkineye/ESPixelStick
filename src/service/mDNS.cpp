/*
* c_mDNS.cpp

* Copyright (c) 2020 Shelby Merrick
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

#include "../ESPixelStick.h"

#include "mDNS.hpp"
#include "../input/InputMgr.hpp"

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

c_mDNS::c_mDNS()
{
    // DEBUG_START;
    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_mDNS::Begin(config_t *NewConfig)
{
    // DEBUG_START;
    config = NewConfig;

    do
    { // Start the mDNS responder for esp8266.local
        LOG_PORT.println(F("Error setting up MDNS responder!"));
        delay(2000);
    } while (!MDNS.begin(config->hostname.c_str()));

    LOG_PORT.println(F("mDNS responder started."));
    MDNS.addService("fpp", "udp", 32320);
    MDNS.addService("http", "tcp", 80);
    // DEBUG_END;
} // Begin

void c_mDNS::Update()
{
    MDNS.update();
}

c_mDNS mDNS;
