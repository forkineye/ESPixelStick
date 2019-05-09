/*
* ESPAsyncDDP.h
*
* Project: ESPAsyncDDP - Asynchronous DDP library for Arduino ESP8266 and ESP32
* Copyright (c) 2019 Daniel Kulp
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

#ifndef ESPASYNCDDP_H_
#define ESPASYNCDDP_H_

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

#define DDP_PORT 4048

#define DDP_PUSH_FLAG 0x01
#define DDP_TIMECODE_FLAG 0x10

typedef struct __attribute__((packed)) {
  uint8_t flags;
  uint8_t sequenceNum;
  uint8_t dataType;
  uint8_t destination;
  uint32_t channelOffset;
  uint16_t dataLen;
  uint8_t data[1];
} DDP_Header;

typedef struct __attribute__((packed)) {
  uint8_t flags;
  uint8_t sequenceNum;
  uint8_t dataType;
  uint8_t destination;
  uint32_t channelOffset;
  uint16_t dataLen;
  uint32_t timeCode;
  uint8_t data[1];
} DDP_TimeCode_Header;

typedef union __attribute__((packed)) {
  DDP_Header header;  // header may or may not be time code
  DDP_TimeCode_Header timeCodeHeader;
  uint8_t raw[1458];
} DDP_packet_t;

typedef struct __attribute__((packed)) {
  uint32_t packetsReceived;
  uint32_t bytesReceived;
  uint32_t errors;
  uint32_t ddpMinChannel;
  uint32_t ddpMaxChannel;
} DDP_stats_t;

class ESPAsyncDDP {
 private:

    DDP_packet_t   *sbuff;       // Pointer to scratch packet buffer
    AsyncUDP        udp;         // UDP
    RingBuf         *pbuff;      // Ring Buffer of universe packet buffers
    uint8_t         lastSequenceSeen;
  
    // Internal Initializers
    bool initUDP(IPAddress ourIP);

    // Packet parser callback
    void parsePacket(AsyncUDPPacket _packet);
    
 public:
    DDP_stats_t  stats;    // Statistics tracker

    ESPAsyncDDP(uint8_t buffers = 1);

    // Generic UDP listener, no physical or IP configuration
    bool begin(IPAddress ourIP);

    // Ring buffer access
    inline bool isEmpty() { return pbuff->isEmpty(pbuff); }
    inline void *pull(DDP_packet_t *packet) { return pbuff->pull(pbuff, packet); }
};


#endif
