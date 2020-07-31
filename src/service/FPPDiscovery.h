#pragma once
/*
* c_FPPDiscovery.h
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2018 Shelby Merrick
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

#ifdef ESP32
#include <WiFi.h>
#include <AsyncUDP.h>
#elif defined (ESP8266)
#include <ESPAsyncUDP.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#else
#error Platform not supported
#endif

#define FPP_DISCOVERY_PORT 32320

class c_FPPDiscovery 
{
  private:
    const char *version;
    AsyncUDP udp;
    void ProcessReceivedUdpPacket(AsyncUDPPacket _packet);
  public:
    c_FPPDiscovery();
    bool begin();
    void sendPingPacket();
};

extern c_FPPDiscovery FPPDiscovery;