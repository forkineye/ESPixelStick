/*
* FPPDiscovery.h
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
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

#ifndef FPPDISCOVERY
#define FPPDISCOVERY

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

typedef union {
    struct {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
        uint8_t  ping_version;
        uint8_t  ping_subtype;
        uint8_t  ping_hardware;
        uint16_t versionMajor;
        uint16_t versionMinor;
        uint8_t  operatingMode;
        uint8_t  ipAddress[4];
        char  hostName[65];
        char  version[41];
        char  hardwareType[41];
        char  ranges[41];
    } __attribute__((packed));

    uint8_t raw[256];
} FPPPingPacket;


class FPPDiscovery {
  private:
    const char *version;
    AsyncUDP udp;
    void parsePacket(AsyncUDPPacket _packet);
  public:
    FPPDiscovery(const char *ver);
    bool begin();
    void sendPingPacket();  
};



#endif
