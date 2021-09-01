#pragma once
/*
* InputDDP.h
*
* Project: InputDDP - Asynchronous DDP library for Arduino ESP8266 and ESP32
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

class c_InputDDP : public c_InputCommon
{
private:

#define DDP_PORT 4048

#define DDP_Header_t_LEN (sizeof(struct ddp_hdr_struct))
#define DDP_MAX_DATALEN (480*3)   // fits nicely in an ethernet packet

#define DDP_FLAGS1_VER      0xc0   // version mask
#define DDP_FLAGS1_VER1     0x40   // version=1
#define DDP_FLAGS1_PUSH     0x01
#define DDP_FLAGS1_QUERY    0x02
#define DDP_FLAGS1_REPLY    0x04
#define DDP_FLAGS1_STORAGE  0x08
#define DDP_FLAGS1_TIME     0x10
#define DDP_FLAGS1_DATAMASK (DDP_FLAGS1_QUERY | DDP_FLAGS1_REPLY | DDP_FLAGS1_STORAGE | DDP_FLAGS1_TIME)
#define DDP_FLAGS1_DATA     0x00

#define DDP_ID_DISPLAY       1
#define DDP_ID_CONFIG      250
#define DDP_ID_STATUS      251

#define IsData(f)          (DDP_FLAGS1_DATA    == ((f) & DDP_FLAGS1_DATAMASK))
#define IsPush(f)          (DDP_FLAGS1_PUSH    == ((f) & DDP_FLAGS1_PUSH))
#define IsQuery(f)         (DDP_FLAGS1_QUERY   == ((f) & DDP_FLAGS1_QUERY))
#define IsReply(f)         (DDP_FLAGS1_REPLY   == ((f) & DDP_FLAGS1_REPLY))
#define IsStorage(f)       (DDP_FLAGS1_STORAGE == ((f) & DDP_FLAGS1_STORAGE))
#define IsTime(f)          (DDP_FLAGS1_TIME    == ((f) & DDP_FLAGS1_TIME))

    typedef struct __attribute__ ((packed))
    {
        uint8_t  flags;
        uint8_t  sequenceNum;
        uint8_t  type;
        uint8_t  destination;
        uint32_t channelOffset;
        uint16_t dataLen;
        uint8_t  data[1];
    } DDP_Header_t;

    typedef struct __attribute__ ((packed))
    {
        uint8_t  flags;
        uint8_t  sequenceNum;
        uint8_t  type;
        uint8_t  destination;
        uint32_t channelOffset;
        uint16_t dataLen;
        uint32_t timeCode;
        uint8_t  data[1];
    } DDP_TimeCode_Header_t;

    typedef union __attribute__ ((packed))
    {
        DDP_Header_t          header;  // header may or may not be time code
        DDP_TimeCode_Header_t timeCodeHeader;
        uint8_t               raw[1458];
    } DDP_packet_t;

    typedef struct __attribute__ ((packed))
    {
        uint32_t packetsReceived;
        uint32_t bytesReceived;
        uint32_t errors;
    } DDP_stats_t;

    AsyncUDP        udp;         // UDP
    uint8_t         lastReceivedSequenceNumber = 0;
    bool            suspend = false;
    DDP_stats_t     stats;    // Statistics tracker

    // Internal Initializers
    void initUDP ();

    // Packet parser callback
    void ProcessReceivedUdpPacket (AsyncUDPPacket _packet);
    void ProcessReceivedData ();
    void ProcessReceivedQuery ();

    enum PacketBufferStatus_t
    {
        BufferIsAvailable,
        BufferIsFilled,
        BufferIsBeingProcessed,
    };

    typedef struct 
    {
        PacketBufferStatus_t PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
        DDP_packet_t Packet;
    } PacketBuffer_t;

    PacketBuffer_t PacketBuffer;

public:

    c_InputDDP (c_InputMgr::e_InputChannelIds NewInputChannelId,
                c_InputMgr::e_InputType       NewChannelType,
                uint8_t                     * BufferStart,
                uint16_t                      BufferSize);

    ~c_InputDDP ();

    // Generic UDP listener, no physical or IP configuration
    void Begin ();
    bool SetConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject& jsonStatus);
    void Process ();                                        ///< Call from loop(),  renders Input data
    void GetDriverName (String& sDriverName) { sDriverName = "DDP"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize);

};

