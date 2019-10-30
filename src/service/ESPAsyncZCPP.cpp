/*
* ESPAsyncZCPP.cpp
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

#include "ESPAsyncZCPP.h"
#include <string.h>

// Constructor
ESPAsyncZCPP::ESPAsyncZCPP(uint8_t buffers) {
    pbuff = RingBuf_new(sizeof(ZCPP_packet_t), buffers);
	suspend = false;
	
    stats.num_packets = 0;
    stats.packet_errors = 0;
}

/////////////////////////////////////////////////////////
//
// Public begin() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncZCPP::begin(IPAddress ourIP) {

	suspend = false;
	return initUDP(ourIP);
}

/////////////////////////////////////////////////////////
//
// Private init() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncZCPP::initUDP(IPAddress ourIP) {
    bool success = false;
    delay(100);

    IPAddress address = IPAddress(224, 0, 30, 5);
    if (udp.listenMulticast(address, ZCPP_PORT)) {
        udp.onPacket(std::bind(&ESPAsyncZCPP::parsePacket, this,
                std::placeholders::_1));

        success = true;
    }
	
  	ip_addr_t ifaddr;
    ip_addr_t multicast_addr;
  	ifaddr.addr = static_cast<uint32_t>(ourIP);
	  multicast_addr.addr = static_cast<uint32_t>(IPAddress(224, 0, 31, (ifaddr.addr & 0xFF000000) >> 24));
	  igmp_joingroup(&ifaddr, &multicast_addr);
    if (Serial) {
        Serial.print("ZCPP subscribed to multicast 224.0.31.");
        Serial.println((ifaddr.addr & 0xFF000000) >> 24);
    }
    return success;
}

/////////////////////////////////////////////////////////
//
// Packet parsing - Private
//
/////////////////////////////////////////////////////////

void ESPAsyncZCPP::parsePacket(AsyncUDPPacket _packet) {
    ZCPP_error_t error = ERROR_ZCPP_NONE;

    sbuff = reinterpret_cast<ZCPP_packet_t *>(_packet.data());
    if (memcmp(sbuff->Discovery.Header.token, ZCPP_token, sizeof(sbuff->Discovery.Header.token))) {
        error = ERROR_ZCPP_ID;
    }
    
	if (!error && sbuff->Discovery.Header.type != ZCPP_TYPE_DISCOVERY &&
    sbuff->Discovery.Header.type != ZCPP_TYPE_CONFIG &&
    sbuff->Discovery.Header.type != ZCPP_TYPE_QUERY_CONFIG &&
		sbuff->Discovery.Header.type != ZCPP_TYPE_SYNC &&
		sbuff->Discovery.Header.type != ZCPP_TYPE_DATA) {
		error = ERROR_ZCPP_IGNORE;
	}

	if (!error && sbuff->Discovery.Header.protocolVersion > ZCPP_CURRENT_PROTOCOL_VERSION) {
		if (sbuff->Discovery.Header.type == 0x00) {
			error = ERROR_ZCPP_NONE;
		}
		else if (!error) {
			error = ERROR_ZCPP_PROTOCOL_VERSION;
		}
	}

    if (!error && (!suspend || sbuff->Discovery.Header.type == ZCPP_TYPE_DISCOVERY)) {
		
		if (sbuff->Discovery.Header.type == ZCPP_TYPE_DISCOVERY ||
		    sbuff->Discovery.Header.type == ZCPP_TYPE_QUERY_CONFIG ||
		    (sbuff->Discovery.Header.type == ZCPP_TYPE_CONFIG && (sbuff->Configuration.flags & ZCPP_CONFIG_FLAG_QUERY_CONFIGURATION_RESPONSE_REQUIRED) != 0)) {
			suspend = true;
		}
		
        pbuff->add(pbuff, sbuff);
        stats.num_packets++;
        stats.last_clientIP = _packet.remoteIP();
        stats.last_clientPort = _packet.remotePort();
        stats.last_seen = millis();
    } else if (error == ERROR_ZCPP_IGNORE || suspend) {
        // Do nothing
    } else {
        if (Serial)
            dumpError(error);
        stats.packet_errors++;
    }
}

/////////////////////////////////////////////////////////
//
// Debugging functions - Public
//
/////////////////////////////////////////////////////////

void ESPAsyncZCPP::dumpError(ZCPP_error_t error) {
    switch (error) {
        case ERROR_ZCPP_ID:
            Serial.print(F("INVALID PACKET ID: "));
            for (uint i = 0; i < sizeof(ZCPP_token); i++)
                Serial.print(sbuff->Discovery.Header.token[i], HEX);
            Serial.println("");
            break;
        case ERROR_ZCPP_PROTOCOL_VERSION:
            Serial.print(F("INVALID PROTOCOL VERSION: 0x"));
            Serial.println(sbuff->Discovery.Header.protocolVersion, HEX);
            break;
        case ERROR_ZCPP_NONE:
            break;
        case ERROR_ZCPP_IGNORE:
            break;
    }
}

void ESPAsyncZCPP::sendConfigResponse(ZCPP_packet_t* packet)
{
  if (udp.writeTo(packet->raw, sizeof(ZCPP_packet_t), stats.last_clientIP, stats.last_clientPort) != sizeof(ZCPP_packet_t)) {
    if (Serial)
        Serial.println("Write of configuration response failed");
  }
  else {
    if (Serial) {
        Serial.print("Configuration response wrote ");
        Serial.print(sizeof(ZCPP_packet_t));
        Serial.println(" bytes.");
    }
  }
  suspend = false;
}

void ESPAsyncZCPP::sendDiscoveryResponse(ZCPP_packet_t* packet, const char* firmwareVersion, const uint8_t* mac, const char* controllerName, int pixelPorts, int serialPorts, uint32_t maxPixelPortChannels, uint32_t maxSerialPortChannels, uint32_t maximumChannels, uint32_t ipAddress, uint32_t ipMask)
{
	memset(packet, 0x00, sizeof(ZCPP_packet_t));
	memcpy(packet->DiscoveryResponse.Header.token, ZCPP_token, sizeof(ZCPP_token));
	packet->DiscoveryResponse.Header.type = ZCPP_TYPE_DISCOVERY_RESPONSE;
	packet->DiscoveryResponse.Header.protocolVersion = ZCPP_CURRENT_PROTOCOL_VERSION;
	packet->DiscoveryResponse.minProtocolVersion = ZCPP_CURRENT_PROTOCOL_VERSION;
	packet->DiscoveryResponse.maxProtocolVersion = ZCPP_CURRENT_PROTOCOL_VERSION;
	packet->DiscoveryResponse.vendor = ntohs(ZCPP_VENDOR_ESPIXELSTICK);
	packet->DiscoveryResponse.model = ntohs(0);
	strncpy(packet->DiscoveryResponse.firmwareVersion, firmwareVersion, std::min((int)strlen(firmwareVersion), (int)sizeof(packet->DiscoveryResponse.firmwareVersion)));
	memcpy(packet->DiscoveryResponse.macAddress, mac, sizeof(packet->DiscoveryResponse.macAddress));
	packet->DiscoveryResponse.ipv4Address = ipAddress;
	packet->DiscoveryResponse.ipv4Mask = ipMask;	
	strncpy(packet->DiscoveryResponse.userControllerName, controllerName, std::min((int)strlen(controllerName), (int)sizeof(packet->DiscoveryResponse.userControllerName)));
	packet->DiscoveryResponse.pixelPorts = pixelPorts;
	packet->DiscoveryResponse.rsPorts = serialPorts;
	packet->DiscoveryResponse.channelsPerPixelPort = ntohs(maxPixelPortChannels); 
	packet->DiscoveryResponse.channelsPerRSPort = ntohs(maxSerialPortChannels);
	packet->DiscoveryResponse.maxTotalChannels = ntohl(maximumChannels);
	uint32_t protocolsSupported = 0;
	if (pixelPorts > 0) {
		protocolsSupported |= ZCPP_DISCOVERY_PROTOCOL_WS2811;
		protocolsSupported |= ZCPP_DISCOVERY_PROTOCOL_GECE;
	}
	if (serialPorts > 0) {
		protocolsSupported |= ZCPP_DISCOVERY_PROTOCOL_DMX;
		protocolsSupported |= ZCPP_DISCOVERY_PROTOCOL_RENARD;
	}
	packet->DiscoveryResponse.protocolsSupported = ntohl(protocolsSupported);
	packet->DiscoveryResponse.flags = ZCPP_DISCOVERY_FLAG_SEND_DATA_AS_MULTICAST;

	if (udp.writeTo(packet->raw, sizeof(packet->DiscoveryResponse), stats.last_clientIP, stats.last_clientPort) != sizeof(packet->DiscoveryResponse)) {
    if (Serial)
    		Serial.println("Write of discovery response failed");
	}
	else {
    if (Serial) {
  		Serial.print("Discovery response wrote ");
  		Serial.print(sizeof(packet->DiscoveryResponse));
  		Serial.println(" bytes.");
    }
	}
	suspend = false;
}
