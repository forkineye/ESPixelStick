/*
* FPPDiscovery.cpp

* Copyright (c) 2019 Shelby Merrick
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

#include "FPPDiscovery.h"
#include <string.h>


FPPDiscovery::FPPDiscovery(const char *ver) {
    version = ver;
}


bool FPPDiscovery::begin() {
    bool success = false;
    delay(100);

    IPAddress address = IPAddress(239, 70, 80, 80);  
    if (udp.listenMulticast(address, FPP_DISCOVERY_PORT)) {
        udp.onPacket(std::bind(&FPPDiscovery::parsePacket, this,
                  std::placeholders::_1));
       success = true;
    }
    sendPingPacket();
    return success;
}


void FPPDiscovery::parsePacket(AsyncUDPPacket _packet) {
    FPPPingPacket *packet = reinterpret_cast<FPPPingPacket *>(_packet.data());
    if (packet->packet_type == 0x04 && packet->ping_subtype == 0x01) {
        //discover ping packet, need to send a ping out
        sendPingPacket();
    }
}


void FPPDiscovery::sendPingPacket() {
    FPPPingPacket packet;
    packet.header[0] = 'F';
    packet.header[1] = 'P';
    packet.header[2] = 'P';
    packet.header[3] = 'D';
    packet.packet_type = 0x04;
    packet.data_len = 214;
    packet.ping_version = 0x2;
    packet.ping_subtype = 0x0; // 1 is to "discover" others, we don't need that
    packet.ping_hardware = 0xC2;  // 0xC2 is assigned for ESPixelStick
    uint16_t v = (uint16_t)atoi(version);
    packet.versionMajor = (v >> 8) + ((v & 0xFF) << 8);
    v = (uint16_t)atoi(&version[2]);
    packet.versionMinor = (v >> 8) + ((v & 0xFF) << 8);
    packet.operatingMode = 0x01; // we only support bridge mode
    uint32_t ip = static_cast<uint32_t>(WiFi.localIP());
    memcpy(packet.ipAddress, &ip, 4);
    if (WiFi.hostname()) {
      strcpy(packet.hostName, WiFi.hostname().c_str());
    }
    strcpy(packet.version, version);
    strcpy(packet.hardwareType, "ESPixelStick");
    packet.ranges[0] = 0;
    udp.broadcastTo((uint8_t*)&packet, 221, FPP_DISCOVERY_PORT);
}
