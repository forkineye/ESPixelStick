/*
* c_InputDDP.cpp
*
* Project: c_InputDDP - Asynchronous DDP library for Arduino ESP8266 and ESP32
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

#include "InputDDP.h"
#include <string.h>
#include "../WiFiMgr.hpp"

//-----------------------------------------------------------------------------
c_InputDDP::c_InputDDP (c_InputMgr::e_InputChannelIds NewInputChannelId,
                        c_InputMgr::e_InputType       NewChannelType,
                        uint8_t                     * BufferStart,
                        uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)

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
    // udp.stop ();

    // DEBUG_END;
} // ~c_InputDDP

//-----------------------------------------------------------------------------
void c_InputDDP::Begin ()
{
    // DEBUG_START;

    suspend = false;

    memset (&stats, 0x00, sizeof (stats));

    // DEBUG_V("");

    initUDP ();

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

    JsonObject ddpStatus = jsonStatus.createNestedObject (F ("ddp"));
    // DEBUG_V ("");

    ddpStatus["packetsreceived"] = stats.packetsReceived;
    ddpStatus["bytesreceived"]   = stats.bytesReceived;
    ddpStatus["errors"]          = stats.errors;

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
void c_InputDDP::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    // DEBUG_START;

    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputDDP::initUDP ()
{
    // DEBUG_START;

    delay (100);

    if (udp.listen (DDP_PORT))
    {
        udp.onPacket (std::bind (&c_InputDDP::ProcessReceivedUdpPacket, this, std::placeholders::_1));
    }

    LOG_PORT.println (String (F ("DDP subscribed to UDP: ")));

    // DEBUG_END;
} // initUDP

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedUdpPacket(AsyncUDPPacket ReceivedPacket)
{
    // DEBUG_START;

    do // once
    {
        // do we have a place to put the received data?
        if (PacketBuffer.PacketBufferStatus == PacketBufferStatus_t::BufferIsBeingProcessed)
        {
            // DEBUG_V ("Throw away the received packet. We dont have a place to put it.");
            break;
        }
        // DEBUG_V ("");

        // overwrrite the existing data for the case of PacketBufferStatus_t::BufferIsFilled
        DDP_packet_t & packet = PacketBuffer.Packet;
        memcpy (packet.raw, ReceivedPacket.data (), sizeof (packet.raw));

        stats.packetsReceived++;
        stats.bytesReceived += ReceivedPacket.length ();

        int CurrentReceivedSequenceNumber = packet.header.sequenceNum & 0xF;
        // DEBUG_V (String("CurrentReceivedSequenceNumber: ") + String(CurrentReceivedSequenceNumber));

        // is the sender using sequence number?
        if (0 != CurrentReceivedSequenceNumber)
        {
            // DEBUG_V (String ("lastReceivedSequenceNumber: ") + String (lastReceivedSequenceNumber));

            // ignore duplicate packets. They are allowed
            if (CurrentReceivedSequenceNumber == lastReceivedSequenceNumber)
            {
                // DEBUG_V ("Duplicate PDU received");
                break;
            }

            // move to the next expected value
            uint8_t NextExpectedSequenceNumber = 0xf & ++lastReceivedSequenceNumber;

            // fix the wrap condition
            if (0 == NextExpectedSequenceNumber) { NextExpectedSequenceNumber = 1; }
            // DEBUG_V (String ("NextExpectedSequenceNumber") + String (NextExpectedSequenceNumber));

            // remember what came in
            lastReceivedSequenceNumber = CurrentReceivedSequenceNumber;

            if (CurrentReceivedSequenceNumber != NextExpectedSequenceNumber)
            {
                stats.errors++;
                // DEBUG_V (String ("Sequence error: stats.errors: ") + String (stats.errors));
            }

        } // using sequence numbers
        // DEBUG_V ("");

        // we have a valid PDU. Time to parse it
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsFilled;
        // DEBUG_V (String ("packet.header.flags: 0x") + String (packet.header.flags, HEX));

        // need to fast track data
        if (true == IsData(packet.header.flags))
        {
            ProcessReceivedData ();
            PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
            break;
        }

#ifdef SUPPORT_QUERY
#else
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
#endif // def SUPPORT_QUERY


    } while (false);

    // DEBUG_END;

} // ProcessReceivedUdpPacket

//-----------------------------------------------------------------------------
void c_InputDDP::Process ()
{
#ifdef SUPPORT_QUERY
    // DEBUG_START;

    do // once
    {
        if (PacketBuffer.PacketBufferStatus != PacketBufferStatus_t::BufferIsFilled)
        {
            // DEBUG_V ("There is nothing in the buffer for us to porcess");
            break;
        }

        // DEBUG_V ("There is something in the buffer for us to process");
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsBeingProcessed;

        if (true == IsData(PacketBuffer.Packet.header.flags))
        {
            ProcessReceivedData ();
            PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
            break;
        }

        if (true == IsQuery (PacketBuffer.Packet.header.flags))
        {
            ProcessReceivedQuery ();
            PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
            break;
        }

        // not sure what this thing is but we are going to ignore it
        PacketBuffer.PacketBufferStatus = PacketBufferStatus_t::BufferIsAvailable;
        // DEBUG_V("UnSupported PDU type");

    } while (false);

    // DEBUG_END;
#endif // def SUPPORT_QUERY

} // Process

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedData ()
{
    // DEBUG_START;

    do // once
    {
        DDP_Header_t & header = PacketBuffer.Packet.header;

        // is the offset and length valid?

        uint32_t InputBufferOffset = ntohl (header.channelOffset);
        uint16_t packetDataLength  = ntohs (header.dataLen);
        uint32_t LastCharOffset    = packetDataLength + InputBufferOffset;

        // DEBUG_V (String ("InputBufferOffset:    ") + String (InputBufferOffset));
        // DEBUG_V (String ("packetDataLength:     ") + String (packetDataLength));
        // DEBUG_V (String ("LastCharOffset:       ") + String (LastCharOffset));

        // will this data fit?
        if (LastCharOffset > InputDataBufferSize)
        {
            // trim the data and record an error
            packetDataLength -= LastCharOffset - InputDataBufferSize;
            stats.errors++;
            // DEBUG_V (String ("New packetDataLength: ") + String (packetDataLength));
        }

        memcpy (&InputDataBuffer[InputBufferOffset], header.data, packetDataLength);

        InputMgr.ResetBlankTimer ();

    } while (false);

    // DEBUG_END;

} // ProcessReceivedData

//-----------------------------------------------------------------------------
void c_InputDDP::ProcessReceivedQuery ()
{
#ifdef SUPPORT_QUERY

    // DEBUG_START;

    uint16_t pixelPorts = 0;
    uint16_t serialPorts = 0;
    OutputMgr.GetPortCounts (pixelPorts, serialPorts);

    sendDiscoveryResponse (
        VERSION,
        WiFi.macAddress (),
        config.id,
        pixelPorts,
        serialPorts,
        InputDataBufferSize,
        InputDataBufferSize,
        InputDataBufferSize,
        WiFiMgr.getIpAddress (),
        WiFiMgr.getIpSubNetMask ());

    // DEBUG_END;
#endif // def SUPPORT_QUERY

} // ProcessReceivedDiscovery


