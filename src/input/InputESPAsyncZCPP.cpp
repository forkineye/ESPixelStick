/*
* c_InputESPAsyncZCPP.cpp
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

#include "InputESPAsyncZCPP.h"
#include "../WiFiMgr.hpp"
#include "../output/OutputMgr.hpp"

//-----------------------------------------------------------------------------
c_InputESPAsyncZCPP::c_InputESPAsyncZCPP (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                          c_InputMgr::e_InputType       NewChannelType,
                                          uint8_t                     * BufferStart,
                                          uint16_t                      BufferSize) :
                                          c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)

{
    // DEBUG_START;

    ZcppPacketBuffer.ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsAvailable;

    // DEBUG_END;
} // c_InputESPAsyncZCPP

//-----------------------------------------------------------------------------
c_InputESPAsyncZCPP::~c_InputESPAsyncZCPP ()
{
    // DEBUG_START;

    OutputMgr.PauseOutput (false);

    // DEBUG_END;
} // ~c_InputESPAsyncZCPP

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::Begin ()
{
    // DEBUG_START;

    Serial.println (String (F ("** ZCPP Initialization for channel '")) + InputChannelId + String (F ("' **")));

    suspend = false;

    ZcppStats.num_packets = 0;
    ZcppStats.packet_errors = 0;

    suspend = false;

    // DEBUG_V("");

    initUDP ();

    // DEBUG_END;

} // Begin

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;


    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
bool c_InputESPAsyncZCPP::SetConfig (JsonObject& jsonConfig)
{

} // SetConfig

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;
} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::initUDP ()
{
    // DEBUG_START;

    delay (100);

    IPAddress MulticastAddress = IPAddress (224, 0, 30, 5);
    if (udp.listenMulticast (MulticastAddress, ZCPP_PORT))
    {
        udp.onPacket (std::bind (&c_InputESPAsyncZCPP::ProcessReceivedUdpPacket, this, std::placeholders::_1));
    }

    ip4_addr_t ifaddr; ifaddr.addr = static_cast<uint32_t>(WiFiMgr.getIpAddress ());
    IPAddress MulticastIpAddress = IPAddress (224, 0, 31, (ifaddr.addr & 0xFF000000) >> 24);
    ip4_addr_t multicast_addr; multicast_addr.addr = static_cast<uint32_t>(MulticastIpAddress);

    igmp_joingroup (&ifaddr, &multicast_addr);

    LOG_PORT.println (String (F ("ZCPP subscribed to multicast: ")) + MulticastIpAddress.toString ());

    // DEBUG_END;
} // initUDP

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::ProcessReceivedUdpPacket (AsyncUDPPacket ReceivedPacket)
{
    // DEBUG_START;

    ZCPP_error_t error = ERROR_ZCPP_NONE;

    do // once
    {
        // do we have a place to put the received data?
        if (ZcppPacketBuffer.ZcppPacketBufferStatus == ZcppPacketBufferStatus_t::BufferIsBeingProcessed)
        {
            DEBUG_V ("Throw away the received packet. We dont have a place to put it.");
            break;
        }
        // DEBUG_V ("");

        // overwrrite the existing data for the case of ZcppPacketBufferStatus_t::BufferIsFilled

        ZCPP_packet_t& packet = ZcppPacketBuffer.zcppPacket;
        memcpy (packet.raw, ReceivedPacket.data(), sizeof (packet.raw));

        if (memcmp (packet.Discovery.Header.token,
            ZCPP_token,
            sizeof (packet.Discovery.Header.token)))
        {
            // DEBUG_V ("");
            error = ERROR_ZCPP_ID;
            break;
        }

        // DEBUG_V ("");
        if (packet.Discovery.Header.type != ZCPP_TYPE_DISCOVERY &&
            packet.Discovery.Header.type != ZCPP_TYPE_CONFIG &&
            packet.Discovery.Header.type != ZCPP_TYPE_QUERY_CONFIG &&
            packet.Discovery.Header.type != ZCPP_TYPE_SYNC &&
            packet.Discovery.Header.type != ZCPP_TYPE_DATA)
        {
            error = ERROR_ZCPP_IGNORE;
            break;
        }

        // DEBUG_V ("");
        if (packet.Discovery.Header.protocolVersion > ZCPP_CURRENT_PROTOCOL_VERSION)
        {
            if (packet.Discovery.Header.type != 0x00)
            {
                error = ERROR_ZCPP_PROTOCOL_VERSION;
                break;
            }
        }

        // DEBUG_V ("");
        if ((false == suspend || packet.Discovery.Header.type == ZCPP_TYPE_DISCOVERY))
        {
            // DEBUG_V ("");
            if (packet.Discovery.Header.type  == ZCPP_TYPE_DISCOVERY ||
                packet.Discovery.Header.type  == ZCPP_TYPE_QUERY_CONFIG ||
                (packet.Discovery.Header.type == ZCPP_TYPE_CONFIG && (packet.Configuration.flags & ZCPP_CONFIG_FLAG_QUERY_CONFIGURATION_RESPONSE_REQUIRED) != 0))
            {
                suspend = true;
            }

            // DEBUG_V ("");
            ZcppStats.num_packets++;
            ZcppStats.last_clientIP = ReceivedPacket.remoteIP ();
            ZcppStats.last_clientPort = ReceivedPacket.remotePort ();
            ZcppStats.last_seen = millis ();

            ZcppPacketBuffer.ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsFilled;

            // need to fast track data
            if (packet.Discovery.Header.type == ZCPP_TYPE_DATA)
            {
                ProcessReceivedData ();
                ZcppPacketBuffer.ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsAvailable;
            }
        }

    } while (false);

    if ((error != ERROR_ZCPP_IGNORE) &&
        (error != ERROR_ZCPP_NONE))
    {
        // DEBUG_V ("");
        dumpError (error);
        ZcppStats.packet_errors++;
    }

    // DEBUG_END;

} // ProcessReceivedUdpPacket

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::dumpError (ZCPP_error_t error)
{
    // DEBUG_START;

    switch (error)
    {
        case ERROR_ZCPP_ID:
        {
            LOG_PORT.print (F ("INVALID PACKET ID: "));
            for (uint i = 0; i < sizeof (ZCPP_token); i++)
            {
                LOG_PORT.print (ZcppPacketBuffer.zcppPacket.Discovery.Header.token[i], HEX);
            }
            LOG_PORT.println ("");
            break;
        }

        case ERROR_ZCPP_PROTOCOL_VERSION:
        {
            LOG_PORT.print (F ("INVALID PROTOCOL VERSION: 0x"));
            LOG_PORT.println (ZcppPacketBuffer.zcppPacket.Discovery.Header.protocolVersion, HEX);
            break;
        }

        case ERROR_ZCPP_NONE:
        case ERROR_ZCPP_IGNORE:
        default:
        {
            break;
        }

    } // switch (error) 

    // DEBUG_END;

} // dumpError

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::sendResponseToMostRecentRequester ()
{
    // DEBUG_START;

    uint32_t NumBytesToSend = ZCPP_GetPacketActualSize (ZcppPacketBuffer.zcppPacket);

    size_t BytesWrittenToUdp = udp.writeTo (ZcppPacketBuffer.zcppPacket.raw, NumBytesToSend, ZcppStats.last_clientIP, ZcppStats.last_clientPort);

    if (BytesWrittenToUdp != NumBytesToSend)
    {
        LOG_PORT.println (F ("Sending response to server failed"));
    }

    DEBUG_V (String ("BytesWrittenToUdp: ") + String (BytesWrittenToUdp));

    // DEBUG_END;

} // sendResponseToMostRecentRequester

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::sendDiscoveryResponse (
    String    firmwareVersion,
    String    mac,
    String    controllerName,
    uint8_t   pixelPorts,
    uint8_t   serialPorts,
    uint16_t  maxPixelChannelsPerPixelPort,
    uint16_t  maxSerialChannelsPerSerialPort,
    uint32_t  TotalMaximumNumChannels,
    IPAddress ipAddress,
    IPAddress ipMask)
{
    DEBUG_START;

    DEBUG_V (String ("ipAddress: ") + String (ipAddress));
    DEBUG_V (String ("ipMask: ") + String (ipMask));
    DEBUG_V (String ("serialPorts: ") + String (serialPorts));
    DEBUG_V (String ("maxPixelChannelsPerPixelPort: ") + String (maxPixelChannelsPerPixelPort));
    DEBUG_V (String ("maxSerialChannelsPerSerialPort: ") + String (maxSerialChannelsPerSerialPort));
    DEBUG_V (String ("TotalMaximumNumChannels: ") + String (TotalMaximumNumChannels));

    ZCPP_DiscoveryResponse & packet = ZcppPacketBuffer.zcppPacket.DiscoveryResponse;

    memset (ZcppPacketBuffer.zcppPacket.raw, 0x00, sizeof (ZcppPacketBuffer.zcppPacket.raw));
    
    memcpy (packet.Header.token, ZCPP_token, sizeof (ZCPP_token));
    packet.Header.type            = ZCPP_TYPE_DISCOVERY_RESPONSE;
    packet.Header.protocolVersion = ZCPP_CURRENT_PROTOCOL_VERSION;
    packet.minProtocolVersion     = ZCPP_CURRENT_PROTOCOL_VERSION;
    packet.maxProtocolVersion     = ZCPP_CURRENT_PROTOCOL_VERSION;
    packet.vendor                 = ZCPP_ToWire16 (ZCPP_VENDOR_ESPIXELSTICK);
    packet.model                  = ZCPP_ToWire16 (4);
    memcpy(packet.firmwareVersion, firmwareVersion.c_str(), sizeof(packet.firmwareVersion)-1);
    memcpy(packet.macAddress, mac.c_str(), sizeof(packet.macAddress));
    // memcpy(&packet.ipv4Address, &ipAddress, sizeof(packet.ipv4Address));

    packet.ipv4Address            = ipAddress;
    packet.ipv4Mask               = ipMask;
    memcpy(packet.userControllerName, controllerName.c_str(), sizeof(packet.userControllerName)-1);
    packet.maxTotalChannels       = ZCPP_ToWire32 (TotalMaximumNumChannels);
    packet.pixelPorts             = pixelPorts;
    packet.rsPorts                = serialPorts;
    packet.channelsPerPixelPort   = ZCPP_ToWire16 (maxPixelChannelsPerPixelPort);
    packet.channelsPerRSPort      = ZCPP_ToWire16 (maxSerialChannelsPerSerialPort);
    packet.flags                  = ZCPP_DISCOVERY_FLAG_SEND_DATA_AS_MULTICAST;
    packet.protocolsSupported     = ZCPP_ToWire32 (ZCPP_DISCOVERY_PROTOCOL_WS2811 |
                                           ZCPP_DISCOVERY_PROTOCOL_GECE |
                                           ZCPP_DISCOVERY_PROTOCOL_DMX |
                                           ZCPP_DISCOVERY_PROTOCOL_RENARD);

    sendResponseToMostRecentRequester ();

    suspend = false;

    DEBUG_END;

} // sendDiscoveryResponse

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::Process ()
{
    // DEBUG_START;

    do // once
    {
        if (ZcppPacketBuffer.ZcppPacketBufferStatus != ZcppPacketBufferStatus_t::BufferIsFilled)
        {
            // DEBUG_V ("There is nothing in the buffer for us to porcess");
            break;
        }
        
        DEBUG_V ("There is something in the buffer for us to process");
        ZcppPacketBuffer.ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsBeingProcessed;

        // todo idleTicker.attach (config.effect_idletimeout, idleTimeout);
/* todo
        if (config.ds == DataSource::IDLEWEB || config.ds == DataSource::E131)
        {
            config.ds = DataSource::ZCPP;
        }
*/
        switch (ZcppPacketBuffer.zcppPacket.Discovery.Header.type)
        {
            case ZCPP_TYPE_DISCOVERY: // discovery
            {
                ProcessReceivedDiscovery ();
                break;
            }

            case ZCPP_TYPE_CONFIG: // config
            {
                ProcessReceivedConfig ();
                break;
            }

            case ZCPP_TYPE_QUERY_CONFIG: // query config
            {
                sendZCPPConfig ();
                break;
            }

            case ZCPP_TYPE_SYNC: // sync
            {
                OutputMgr.PauseOutput (false);
                break;
            }

            case ZCPP_TYPE_DATA: // data
            {
                ProcessReceivedData ();
                break;
            }

            default:
            {
                LOG_PORT.println (String (F("ZCPP: Discarding Packet with Unknown Header Type: ")) + String (ZcppPacketBuffer.zcppPacket.Discovery.Header.type));
                break;
            }
        } // switch (zcppPacket.Discovery.Header.type)

        ZcppPacketBuffer.ZcppPacketBufferStatus = ZcppPacketBufferStatus_t::BufferIsAvailable;

    } while (false);

    // DEBUG_END;

} // Process

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::ProcessReceivedConfig ()
{
    DEBUG_START;

    ZCPP_Configuration& packet = ZcppPacketBuffer.zcppPacket.Configuration;

    do // once
    {
        if (ZCPP_FromWire16 (packet.sequenceNumber) == LastReceivedSequenceNumber)
        {
            DEBUG_V ("Ignore duplicate Config Message");
            break;
        }

        LastReceivedSequenceNumber = ZCPP_FromWire16 (packet.sequenceNumber);

        // a new config to apply
        DEBUG_V (String ("The config is new: ") + ZCPP_FromWire16 (LastReceivedSequenceNumber));

        config.id = String (packet.userControllerName);
        DEBUG_V (String ("Set Controller Name: ") + config.id);

        DEBUG_V (String ("packet.ports: ") + String (packet.ports));

        DynamicJsonDocument JsonConfigRoot (2048);
        JsonObject JsonConfig = JsonConfigRoot.to<JsonObject> ();

        for (int CurrentPortIndex = 0; CurrentPortIndex < packet.ports; CurrentPortIndex++)
        {
            DEBUG_V (String ("CurrentPortIndex: ") + String (CurrentPortIndex));

            ZCPP_PortConfig& CurrentPortConfig = packet.PortConfig[CurrentPortIndex];

            if (CurrentPortConfig.port != 0)
            {
                LOG_PORT.println (String (F ("ZCPP Attempt to configure invalid port ")) + String (CurrentPortConfig.port));
                break;
            }

            JsonObject JsonOutputPortConfig = JsonConfig.createNestedObject (String (CurrentPortConfig.port));
            JsonObject JsonInputPortConfig  = JsonConfig.createNestedObject (String (CurrentPortConfig.port));

            switch (CurrentPortConfig.protocol)
            {
                //TODO: Add unified mode switching or simplify
            case ZCPP_PROTOCOL_WS2811:
            {
                DEBUG_V ("ZCPP_PROTOCOL_WS2811");

                JsonOutputPortConfig["type"] = c_OutputMgr::e_OutputType::OutputType_WS2811;
                break;
            }

            case ZCPP_PROTOCOL_GECE:
            {
                DEBUG_V ("ZCPP_PROTOCOL_GECE");
                JsonOutputPortConfig["type"] = c_OutputMgr::e_OutputType::OutputType_GECE;
                break;
            }

            case ZCPP_PROTOCOL_DMX:
            {
                DEBUG_V ("ZCPP_PROTOCOL_DMX");
                JsonOutputPortConfig["type"] = c_OutputMgr::e_OutputType::OutputType_DMX;
                break;
            }

            case ZCPP_PROTOCOL_RENARD:
            {
                DEBUG_V ("ZCPP_PROTOCOL_RENARD");
                JsonOutputPortConfig["type"] = c_OutputMgr::e_OutputType::OutputType_Renard;
                break;
            }

            default:
            {
                LOG_PORT.println (String (F ("ZCPP: Attempt to configure invalid output protocol ")) + CurrentPortConfig.protocol);
                break;
            }
            } // switch (CurrentPortConfig.protocol)

            //TODO: Add to E1.31 input config
            // config.channel_start = ZCPP_ToWire32 (CurrentPortConfig.startChannel);
            // config.channel_count = ZCPP_ToWire32 (CurrentPortConfig.channels);

            // TODO more output config
            // config.groupSize = CurrentPortConfig.grouping;

            switch (ZCPP_GetColourOrder (CurrentPortConfig.directionColourOrder))
            {
            case ZCPP_COLOUR_ORDER_RGB:
            {
                // config.pixel_color = "RGB";
                break;
            }
            case ZCPP_COLOUR_ORDER_RBG:
            {
                // config.pixel_color = "RBG";
                break;
            }
            case ZCPP_COLOUR_ORDER_GRB:
            {
                // config.pixel_color = "GRB";
                break;
            }
            case ZCPP_COLOUR_ORDER_GBR:
            {
                // config.pixel_color = "GBR";
                break;
            }
            case ZCPP_COLOUR_ORDER_BRG:
            {
                // config.pixel_color = "BRG";
                break;
            }
            case ZCPP_COLOUR_ORDER_BGR:
            {
                // config.pixel_color = "GBR";
                break;
            }
            default:
            {
                LOG_PORT.print ("Attempt to configure invalid colour order ");
                LOG_PORT.print (ZCPP_GetColourOrder (CurrentPortConfig.directionColourOrder));
                break;
            }
            } // color order

            // config.briteVal = (float)(CurrentPortConfig.brightness) / 100.0f;
            // config.gammaVal = ZCPP_GetGamma (CurrentPortConfig.gamma);
        } // for each port

        // save config values

    } while (false);

    // is this the final config packet?
    if (packet.flags & ZCPP_CONFIG_FLAG_LAST)
    {
        // has a response been requested
        if ((packet.flags & ZCPP_CONFIG_FLAG_QUERY_CONFIGURATION_RESPONSE_REQUIRED) != 0)
        {
            sendZCPPConfig ();
        }
    } // final config

    DEBUG_END;
} // ProcessReceivedConfig

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::ProcessReceivedData ()
{
    // DEBUG_START;

    do // once
    {
        ZCPP_Data& packet = ZcppPacketBuffer.zcppPacket.Data;

        uint8_t sequenceNumber     = packet.sequenceNumber;
        uint32_t InputBufferOffset = ZCPP_FromWire32 (packet.frameAddress);
        uint16_t packetDataLength  = ZCPP_FromWire16 (packet.packetDataLength);
        // bool IsFirstFrameInMessage = ZCPP_DATA_FLAG_FIRST == (packet.flags & ZCPP_DATA_FLAG_FIRST);
        // bool IsLastFrameInMessage  = ZCPP_DATA_FLAG_LAST  == (packet.flags & ZCPP_DATA_FLAG_LAST);

        // DEBUG_V (String ("packet.flags: 0x") + String (packet.flags, HEX));

        if (ZCPP_DATA_FLAG_SYNC_WILL_BE_SENT == (packet.flags & ZCPP_DATA_FLAG_SYNC_WILL_BE_SENT))
        {
            // suppress display until we see a sync
            // todo - restore - OutputMgr.PauseOutput (true);
            // DEBUG_V("Output Suppressed.")
        }

        if (sequenceNumber != LastReceivedSequenceNumber + 1)
        {
            LOG_PORT.print (F ("Sequence Error - expected: "));
            LOG_PORT.print (LastReceivedSequenceNumber + 1);
            LOG_PORT.print (F (" actual: "));
            LOG_PORT.println (sequenceNumber);
            ZcppStats.packet_errors++;
        }
        else
        {
            LOG_PORT.print (F ("Sequence - actual: "));
            LOG_PORT.println (sequenceNumber);
        }

        LastReceivedSequenceNumber = sequenceNumber;

        ZcppStats.num_packets++;

        DEBUG_V (String ("InputBufferOffset: ") + String (InputBufferOffset));
        DEBUG_V (String ("packetDataLength: ") + String (packetDataLength));

        memcpy (&InputDataBuffer[InputBufferOffset], packet.data, packetDataLength);

    } while (false);

    // DEBUG_END;

} // ProcessReceivedData

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::ProcessReceivedDiscovery ()
{
    DEBUG_START;

    uint16_t pixelPorts  = 0;
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

    DEBUG_END;

} // ProcessReceivedDiscovery

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::sendZCPPConfig ()
{
    DEBUG_START;

    ZCPP_QueryConfigurationResponse & packet = ZcppPacketBuffer.zcppPacket.QueryConfigurationResponse;

    uint16_t PixelPortCount = 0;
    uint16_t SerialPortCount = 0;
    OutputMgr.GetPortCounts (PixelPortCount, SerialPortCount);

    memset (&packet, 0x00, sizeof (packet));

    memcpy (packet.Header.token, ZCPP_token, sizeof (ZCPP_token));
    packet.Header.type = ZCPP_TYPE_QUERY_CONFIG_RESPONSE;
    packet.Header.protocolVersion = ZCPP_CURRENT_PROTOCOL_VERSION;
    packet.sequenceNumber = ZCPP_ToWire16 (LastReceivedSequenceNumber);
    memcpy (packet.userControllerName, config.id.c_str (), sizeof (packet.userControllerName));
    packet.ports = PixelPortCount;

    // ask output manager for the config record and translate to ZCPP speak.
    for (int outputPortId = int (c_OutputMgr::e_OutputChannelIds::OutputChannelId_Start); 
        outputPortId < int (c_OutputMgr::e_OutputChannelIds::OutputChannelId_End);
        ++outputPortId)
    {
        DEBUG_V ("");

        // get config for this channel from the output manager
        String PortConfig;
        OutputMgr.GetPortConfig (static_cast<c_OutputMgr::e_OutputChannelIds>(outputPortId), PortConfig);
        DEBUG_V (String("PortConfig: ") + PortConfig );

        DynamicJsonDocument JsonConfigDoc (2048);
        DeserializationError error = deserializeJson (JsonConfigDoc, PortConfig);

        if (error)
        {
            LOG_PORT.println (String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            LOG_PORT.println (String (F ("++++")) + PortConfig + String (F ("----")));
            break;
        }
        DEBUG_V ("");

        JsonObject JsonConfig = JsonConfigDoc.to<JsonObject> ();
        
        AddPortDataToResponsePacket (outputPortId, JsonConfig);

    } // for each port

    DEBUG_V ("");

    sendResponseToMostRecentRequester ();

    DEBUG_END;

} // sendZCPPConfig

//-----------------------------------------------------------------------------
void c_InputESPAsyncZCPP::AddPortDataToResponsePacket (int PortId, JsonObject & PortConfig)
{
    DEBUG_START;

    ZCPP_PortConfig & PortConfigResponse = ZcppPacketBuffer.zcppPacket.QueryConfigurationResponse.PortConfig[PortId];
    memset ((void*)(&PortConfigResponse), 0x0, sizeof (PortConfigResponse));

    PortConfigResponse.port         = PortId;
    PortConfigResponse.string       = 0;

    uint32_t startChannel = 0;
    FileIO::setFromJSON (startChannel, PortConfig[F ("start_channel")]);

    // setFromJSON makes no change to the target value if the key is not found.
    uint32_t channel_count = 0;
    FileIO::setFromJSON (channel_count, PortConfig[F ("pixel_count")]);
    channel_count *= 3;
    FileIO::setFromJSON (channel_count, PortConfig[F ("num_chan")]);

    uint8_t group_size = 0;
    FileIO::setFromJSON (group_size, PortConfig[F ("group_size")]);

    float brightness = 1.0;
    FileIO::setFromJSON (brightness, PortConfig[F ("brightness")]);

    float gamma = 0;
    FileIO::setFromJSON (gamma, PortConfig[F ("gamma")]);

    PortConfigResponse.startChannel         = ZCPP_ToWire32 (startChannel);
    PortConfigResponse.channels             = ZCPP_ToWire32 (channel_count);
    PortConfigResponse.grouping             = group_size;
    PortConfigResponse.protocol             = TranslateOutputType (PortConfig);
    PortConfigResponse.directionColourOrder = TranslateColorOrder (PortConfig);
    PortConfigResponse.brightness           = uint8_t(brightness * 100.0f);
    PortConfigResponse.gamma                = uint8_t(gamma * 10);

    if ((ZCPP_PROTOCOL_DMX == PortConfigResponse.protocol) || (ZCPP_PROTOCOL_RENARD == PortConfigResponse.protocol))
    {
        PortConfigResponse.port |= 0x80;
    }

    DEBUG_END;

} // AddPortDataToResponsePacket

//-----------------------------------------------------------------------------
uint8_t c_InputESPAsyncZCPP::TranslateColorOrder (JsonObject & PortConfig)
{
    DEBUG_START;

    uint8_t response = ZCPP_COLOUR_ORDER_RGB;

    String color_order;
    FileIO::setFromJSON (color_order, PortConfig[F ("color_order")]);

    do // once (color_order)
    {
        if(color_order == "RGB")
        {
            response = ZCPP_COLOUR_ORDER_RGB;
            break;
        }

        if (color_order == "RBG")
        {
            response = ZCPP_COLOUR_ORDER_RBG;
            break;
        }

        if (color_order == "GRB")
        {
            response = ZCPP_COLOUR_ORDER_GRB;
            break;
        }

        if (color_order == "GBR")
        {
            response = ZCPP_COLOUR_ORDER_GBR;
            break;
        }

        if (color_order == "BRG")
        {
            response = ZCPP_COLOUR_ORDER_BRG;
            break;
        }

        if (color_order == "BGR")
        {
            response = ZCPP_COLOUR_ORDER_BGR;
            break;
        }

    } while (false);

    DEBUG_END;

    return response;
} // TranslateColorOrder

//-----------------------------------------------------------------------------
uint8_t c_InputESPAsyncZCPP::TranslateOutputType (JsonObject& PortConfig)
{
    DEBUG_START;

    uint8_t response = ZCPP_PROTOCOL_WS2811;

    String outputType;
    FileIO::setFromJSON (outputType, PortConfig[F ("type")]);

    do // once (color_order)
    {
        if (outputType == "WS2811")
        {
            response = ZCPP_PROTOCOL_WS2811;
            break;
        }

        if (outputType == "GECE")
        {
            response = ZCPP_PROTOCOL_GECE;
            break;
        }

        if (outputType == "DMX")
        {
            response = ZCPP_PROTOCOL_DMX;
            break;
        }

        if (outputType == "RENARD")
        {
            response = ZCPP_PROTOCOL_RENARD;
            break;
        }

        if (outputType == "SERIAL")
        {
            response = ZCPP_PROTOCOL_RENARD;
            break;
        }

    } while (false);

    DEBUG_END;

    return response;
} // TranslateOutputType
