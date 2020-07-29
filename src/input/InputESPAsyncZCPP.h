#pragma once
/*
* ESPAsyncZCPP.h
*
* Project: c_InputESPAsyncZCPP - Asynchronous ZCPP library for Arduino ESP8266 and ESP32
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
#include "InputCommon.hpp"

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
#include "ZCPP.h"

#if LWIP_VERSION_MAJOR == 1
typedef struct ip_addr ip4_addr_t;
#endif

class c_InputESPAsyncZCPP : public c_InputCommon
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

    uint16_t        EndOfFrameZCPPSequenceNumber = 0;
    AsyncUDP        udp;                      // UDP
    bool            suspend = true;           // suspends all ZCPP processing until discovery is responded to

    // Internal Initializers
    void initUDP ();

    // Packet parser callback
    void parsePacket (AsyncUDPPacket _packet);

    ZCPP_stats_t  ZcppStats;    // Statistics tracker
    
    enum ZcppPacketBufferStatus_t
    {
        BufferIsFree,
        BufferIsFilled,
        BufferIsInUse,
    };

    typedef struct ZcppPacketBuffer_t
    {
        ZcppPacketBufferStatus_t ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsFree;
        ZCPP_packet_t zcppPacket;
    };

    ZcppPacketBuffer_t ZcppPacketBuffer;

    void sendDiscoveryResponse    (
        String firmwareVersion, 
        String mac, 
        String controllerName, 
        uint8_t pixelPorts, 
        uint8_t serialPorts, 
        uint16_t maxPixelChannelsPerPort, 
        uint16_t maxSerialChannelsPerPort, 
        uint32_t maximumChannels, 
        IPAddress ipAddress, 
        IPAddress ipMask);
    void sendResponseToLastServer (size_t NumBytesToSend);
    void ProcessReceivedDiscovery ();
    void ProcessReceivedConfig    ();
    void ProcessReceivedData      ();
    void sendZCPPConfig           ();

    // Diag functions
    void dumpError (ZCPP_error_t error);

public:

    c_InputESPAsyncZCPP (c_InputMgr::e_InputChannelIds NewInputChannelId,
                         c_InputMgr::e_InputType       NewChannelType,
                         uint8_t                     * BufferStart,
                         uint16_t                      BufferSize);
    ~c_InputESPAsyncZCPP ();

    // Generic UDP listener, no physical or IP configuration
    void Begin ();
    bool SetConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject& jsonStatus);
    void Process ();                                        ///< Call from loop(),  renders Input data
    void GetDriverName (String& sDriverName) { sDriverName = "ZCPP"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize);

}; // c_InputESPAsyncZCPP
