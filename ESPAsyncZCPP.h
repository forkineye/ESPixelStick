/*
* ESPAsyncZCPP.h
*
* Project: ESPAsyncZCPP - Asynchronous ZCPP library for Arduino ESP8266 and ESP32
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

#ifndef ESPASYNCZCPP_H_
#define ESPASYNCZCPP_H_

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
#include <Arduino.h>
#include "RingBuf.h"

#if LWIP_VERSION_MAJOR == 1
typedef struct ip_addr ip4_addr_t;
#endif

#include "ZCPP.h"

// Error Types
typedef enum {
    ERROR_ZCPP_NONE,
    ERROR_ZCPP_IGNORE,
    ERROR_ZCPP_ID,
    ERROR_ZCPP_PROTOCOL_VERSION,
} ZCPP_error_t;

// Status structure
typedef struct {
    uint32_t    num_packets;
    uint32_t    packet_errors;
    IPAddress   last_clientIP;
    uint16_t    last_clientPort;
    unsigned long    last_seen;
} ZCPP_stats_t;

class ESPAsyncZCPP {
 private:

	ZCPP_packet_t   *sbuff;       // Pointer to scratch packet buffer
    AsyncUDP        udp;          // UDP
    RingBuf         *pbuff;       // Ring Buffer of universe packet buffers
	bool            suspend;      // suspends all ZCPP processing until discovery is responded to
	
    // Internal Initializers
    bool initUDP(IPAddress ourIP);

    // Packet parser callback
    void parsePacket(AsyncUDPPacket _packet);

 public:
    ZCPP_stats_t  stats;    // Statistics tracker

    ESPAsyncZCPP(uint8_t buffers = 1);

    // Generic UDP listener, no physical or IP configuration
    bool begin(IPAddress ourIP);

    // Ring buffer access
    inline bool isEmpty() { return pbuff->isEmpty(pbuff); }
    inline void *pull(ZCPP_packet_t *packet) { return pbuff->pull(pbuff, packet); }
	  void sendDiscoveryResponse(ZCPP_packet_t* packet, const char* firmwareVersion, const uint8_t* mac, const char* controllerName, int pixelPorts, int serialPorts, uint32_t maxPixelPortChannels, uint32_t maxSerialPortChannels, uint32_t maximumChannels, uint32_t ipAddress, uint32_t ipMask);
    void sendConfigResponse(ZCPP_packet_t* packet);

    // Diag functions
    void dumpError(ZCPP_error_t error);
};
#endif  // ESPASYNCZCPP_H_
