/*
* c_InputDDP.cpp
*
* Project: c_InputDDP - Asynchronous DDP library for Arduino ESP8266 and ESP32
* Copyright (c) 2019, 2025 Daniel Kulp, Shelby Merrick
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

#include "input/InputDDP.h"
#include "network/NetworkMgr.hpp"
#include "service/FPPDiscovery.h"
#include <string.h>

#ifdef ARDUINO_ARCH_ESP32
#   define FPP_TYPE_ID          0xC3
#   define FPP_VARIANT_NAME     (String(CN_ESPixelStick) + "-ESP32")

#else
#   define FPP_TYPE_ID          0xC2
#   define FPP_VARIANT_NAME     (String(CN_ESPixelStick) + "-ESP8266")
#endif

//-----------------------------------------------------------------------------
c_InputDDP::c_InputDDP (c_InputMgr::e_InputChannelIds NewInputChannelId,
                        c_InputMgr::e_InputType       NewChannelType,
                        uint32_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)

{
    // DEBUG_START;

    PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;

    // DEBUG_END;
} // c_InputDDP

//-----------------------------------------------------------------------------
c_InputDDP::~c_InputDDP ()
{
    // DEBUG_START;

    // OutputMgr.PauseOutput (false);
    // udp->stop ();

    // DEBUG_END;
} // ~c_InputDDP

//-----------------------------------------------------------------------------
void c_InputDDP::Begin ()
{
    // DEBUG_START;

    suspend = false;

    memset (&stats, 0x00, sizeof (stats));

    // DEBUG_V("");
    udp = new AsyncUDP ();

    NetworkStateChanged (NetworkMgr.IsConnected ());

    // HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//-----------------------------------------------------------------------------
void c_InputDDP::GetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;


    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputDDP::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject ddpStatus = jsonStatus[F ("ddp")].to<JsonObject> ();
    // DEBUG_V ("");

    JsonWrite(ddpStatus, F("packetsreceived"), stats.packetsReceived);
    JsonWrite(ddpStatus, F("bytesreceived"),  float(stats.bytesReceived) / 1024.0);
    JsonWrite(ddpStatus, CN_errors,           stats.errors);
    JsonWrite(ddpStatus, CN_id,               InputChannelId);
    JsonWrite(ddpStatus, F("lastError"),      lastError);

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
bool c_InputDDP::SetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    // DEBUG_END;

    return false;

} // SetConfig

//-----------------------------------------------------------------------------
void c_InputDDP::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;

    // DEBUG_V (String ("        InputBuffer: 0x") + String (uint32_t (InputDataBuffer), HEX));
    // DEBUG_V (String ("InputDataBufferSize: ") + String (uint32_t (InputDataBufferSize)));

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputDDP::NetworkStateChanged (bool IsConnected)
{
    if (IsConnected && !HasBeenInitialized)
    {
        // DEBUG_V ();

        if (udp->listen (DDP_PORT))
        {
            udp->onPacket (std::bind (&c_InputDDP::ProcessReceivedUdpPacket, this, std::placeholders::_1));
        }

        HasBeenInitialized = true;

        logcon (String (F ("Listening on port ")) + DDP_PORT);
    }
} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedUdpPacket(AsyncUDPPacket ReceivedPacket)
{
    // DEBUG_START;

    do // once
    {
        DDP_packet_t & packet = *((DDP_packet_t * )(ReceivedPacket.data ()));

        stats.packetsReceived++;
        stats.bytesReceived += ReceivedPacket.length ();

        if ((packet.header.flags1 & DDP_FLAGS1_VERMASK) != DDP_FLAGS1_VER1)
        {
            stats.errors++;
            lastError = String(F("Incorrect version. Flags1: 0x")) + String(packet.header.flags1, HEX);
            // DEBUG_V ("Invalid version");
            break;
        }

        // need to fast track data
        if (true == IsData(packet.header.flags1))
        {
            ProcessReceivedData (packet);
            break;
        }

        // do we have a place to put the received data?
        if (PacketBuffer.PacketBufferStatus == PacketBufferStatus_t::BufferIsBeingProcessed)
        {
            // DEBUG_V ("Throw away the received packet. We dont have a place to put it.");
            break;
        }
        // DEBUG_V ("");

        PacketBuffer.ResponseAddress = ReceivedPacket.remoteIP ();
        PacketBuffer.ResponsePort = ReceivedPacket.remotePort ();
        memcpy ((void*)&PacketBuffer.Packet, ReceivedPacket.data (), sizeof (PacketBuffer.Packet));
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsFilled;

    } while (false);

    // DEBUG_END;

} // ProcessReceivedUdpPacket

//-----------------------------------------------------------------------------
void c_InputDDP::Process ()
{
    // DEBUG_START;

    do // once
    {
        if(!IsInputChannelActive)
        {
            break;
        }

        if (PacketBuffer.PacketBufferStatus != PacketBufferStatus_t::BufferIsFilled)
        {
            // DEBUG_V ("There is nothing in the buffer for us to porcess");
            break;
        }

        // DEBUG_V ("There is something in the buffer for us to process");
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsBeingProcessed;

        if (true == IsData(PacketBuffer.Packet.header.flags1))
        {
            ProcessReceivedData (PacketBuffer.Packet);
            PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
            break;
        }

        if (true == IsQuery (PacketBuffer.Packet.header.flags1))
        {
            ProcessReceivedQuery ();
            PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
            break;
        }

        // DEBUG_V ("not sure what this thing is but we are going to ignore it");
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
        // DEBUG_V("UnSupported PDU type");

    } while (false);

    // DEBUG_END;

} // Process

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedData (DDP_packet_t & Packet)
{
    // DEBUG_START;

    do // once
    {
        DDP_Header_t & header = Packet.header;
        // DEBUG_V (String ("              header: 0x") + String (uint32_t (&Packet.header), HEX));

        // is the offset and length valid?

        uint32_t InputBufferOffset = ntohl (header.channelOffset);
        uint32_t packetDataLength  = ntohs (header.dataLen);

        // DEBUG_V (String ("    packetDataLength: ") + String (packetDataLength));
        // DEBUG_V (String (" InputDataBufferSize: ") + String (InputDataBufferSize));

        if (InputBufferOffset >= InputDataBufferSize)
        {
            // DEBUG_V ("Cant write any of this data to the input buffer");
            lastError = String("Too much data received. Entire PDU discarded");
            stats.errors++;
            break;
        }

        uint32_t RemainingBufferSpace = InputDataBufferSize - InputBufferOffset;
        // DEBUG_V (String ("RemainingBufferSpace: ") + String (RemainingBufferSpace));

        uint32_t AdjPacketDataLength = packetDataLength;
        if (RemainingBufferSpace < packetDataLength)
        {
            AdjPacketDataLength = RemainingBufferSpace;
            lastError = String("Too much data received. Discarding ") + String(packetDataLength - RemainingBufferSpace) + (" bytes of data");
            stats.errors++;
        }
        // DEBUG_V (String (" AdjPacketDataLength: ") + String (AdjPacketDataLength));

        byte* Data = (IsTime(header.flags1)) ? &((DDP_TimeCode_packet_t&)Packet).data[0] : &Packet.data[0];
        // DEBUG_V (String ("                Data: 0x") + String (uint32_t (Data), HEX));
        // DEBUG_V (String ("   InputBufferOffset: ") + String (InputBufferOffset));
        OutputMgr.WriteChannelData(InputBufferOffset, AdjPacketDataLength, &Data[0]);

        InputMgr.RestartBlankTimer (GetInputChannelId ());

    } while (false);

    // DEBUG_END;

} // ProcessReceivedData

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedQuery ()
{
    // DEBUG_START;

    bool haveResponse = false;
    JsonDocument JsonResponseDoc;

    DDP_packet_t & Packet = PacketBuffer.Packet;

    DDP_packet_t DDPresponse;
    memset ((void*)&DDPresponse, 0x00, sizeof (DDPresponse));
    DDPresponse.header.flags1 = DDP_FLAGS1_VER1 | DDP_FLAGS1_REPLY | DDP_FLAGS1_PUSH;

    AsyncUDPMessage UDPresponse;

    // DEBUG_V (String ("Packet.header.flags1: ") + String (Packet.header.flags1));
    // DEBUG_V (String ("  Packet.header.type: ") + String (Packet.header.type));
    // DEBUG_V (String ("    Packet.header.id: ") + String (Packet.header.id));

    switch (Packet.header.id)
    {
        case DDP_ID_STATUS:
        {
            // DEBUG_V ("DDP_ID_STATUS query");
            haveResponse = true;

            DDPresponse.header.id = DDP_ID_STATUS;

            JsonObject JsonStatus = JsonResponseDoc[(char*)CN_status].to<JsonObject> ();
            JsonWrite(JsonStatus, F("man"), "ESPixelStick");
            JsonWrite(JsonStatus, F("mod"), "V4");
            JsonWrite(JsonStatus, F("ver"), VERSION);

            FPPDiscovery.GetSysInfoJSON(JsonStatus);
            break;
        }

        case DDP_ID_CONFIG:
        {
            // DEBUG_V ("DDP_ID_CONFIG query");
            haveResponse = true;

            DDPresponse.header.id = DDP_ID_CONFIG;

            JsonObject JsonConfig = JsonResponseDoc[(char*)CN_config].to<JsonObject> ();
            FPPDiscovery.GetSysInfoJSON(JsonConfig);
            JsonWrite(JsonConfig, CN_id,              config.id);
            JsonWrite(JsonConfig, CN_ip,              NetworkMgr.GetlocalIP ().toString ());
            JsonWrite(JsonConfig, F("hardwareType"),  FPP_VARIANT_NAME);
            JsonWrite(JsonConfig, CN_type,            FPP_TYPE_ID);
            JsonWrite(JsonConfig, CN_num_chan,        InputDataBufferSize);
            uint16_t PixelPortCount;
            uint16_t SerialPortCount;
            OutputMgr.GetPortCounts (PixelPortCount, SerialPortCount);
            JsonWrite(JsonConfig, F("NumPixelPort"),  PixelPortCount);
            JsonWrite(JsonConfig, F("NumSerialPort"), SerialPortCount);
            break;
        }

        default:
        {
            stats.errors++;
            lastError = String (F("Unsupported query: ")) + String (DDPresponse.header.id);
            break;
        }
    }

    if(haveResponse)
    {
        String JsonResponse;
        serializeJson (JsonResponseDoc, JsonResponse);
        // DEBUG_V(JsonResponse);

        memcpy (&DDPresponse.data, JsonResponse.c_str (), JsonResponse.length ());
        DDPresponse.header.dataLen = htons (JsonResponse.length ());
        UDPresponse.write ((const uint8_t*)&DDPresponse, uint32_t (sizeof (DDPresponse.header) + JsonResponse.length ()));
        udp->sendTo (UDPresponse, PacketBuffer.ResponseAddress, PacketBuffer.ResponsePort);
    }
    // DEBUG_END;

} // ProcessReceivedDiscovery
