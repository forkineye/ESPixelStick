#pragma once
/*
* c_ESPAsyncZCPP.h
*
* Project: c_ESPAsyncZCPP - Asynchronous ZCPP library for Arduino ESP8266 and ESP32
* Copyright (c) 2019 Keith Westley
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

#include <lwip/ip_addr.h>
#include <lwip/igmp.h>
#include "RingBuf.h"
#include "ZCPP.h"

#if LWIP_VERSION_MAJOR == 1
typedef struct ip_addr ip4_addr_t;
#endif


class c_ESPAsyncZCPP
{
private:

    // Error Types
    typedef enum 
    {
        ERROR_ZCPP_NONE,
        ERROR_ZCPP_IGNORE,
        ERROR_ZCPP_ID,
        ERROR_ZCPP_PROTOCOL_VERSION,
    } ZCPP_error_t;

    // Status structure
    typedef struct 
    {
        uint32_t      num_packets;
        uint32_t      packet_errors;
        IPAddress     last_clientIP;
        uint16_t      last_clientPort;
        unsigned long last_seen;
    } ZCPP_stats_t;

    // ZCPP_packet_t * sbuff = nullptr; // Pointer to scratch packet buffer
    AsyncUDP        udp;                   // UDP
    RingBuf       * pRingBuffer = nullptr; // Ring Buffer of universe packet buffers
    bool            suspend = true;        // suspends all ZCPP processing until discovery is responded to

    // Internal Initializers
    bool initUDP (IPAddress ourIP);

    // Packet parser callback
    void parsePacket (AsyncUDPPacket _packet);

public:
    ZCPP_stats_t  stats;    // Statistics tracker

    c_ESPAsyncZCPP ();
    ~c_ESPAsyncZCPP ();

    // Generic UDP listener, no physical or IP configuration
    bool begin (uint8_t buffers = 1);

    // Ring buffer access
    bool isEmpty () { return pRingBuffer->isEmpty (pRingBuffer); }
    void * pull (ZCPP_packet_t * packet) { return pRingBuffer->pull (pRingBuffer, packet); }
    void sendDiscoveryResponse (ZCPP_packet_t* packet, const char* firmwareVersion, const uint8_t* mac, const char* controllerName, int pixelPorts, int serialPorts, uint32_t maxPixelPortChannels, uint32_t maxSerialPortChannels, uint32_t maximumChannels, uint32_t ipAddress, uint32_t ipMask);
    void sendConfigResponse (ZCPP_packet_t* packet);

    // Diag functions
    void dumpError (ZCPP_error_t error);
};

extern c_ESPAsyncZCPP ESPAsyncZCPP;

