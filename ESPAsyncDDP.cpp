/*
* ESPAsyncDDP.cpp
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

#include "ESPAsyncDDP.h"
#include <string.h>

// Constructor
ESPAsyncDDP::ESPAsyncDDP(uint8_t buffers) {
  pbuff = RingBuf_new(sizeof(DDP_packet_t), buffers);  
  lastSequenceSeen = 0;

  stats.packetsReceived = 0;
  stats.bytesReceived = 0;
  stats.errors = 0;
  stats.ddpMinChannel = 9999999;
  stats.ddpMaxChannel = 0;
}

/////////////////////////////////////////////////////////
//
// Public begin() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncDDP::begin(IPAddress ourIP) {
  return initUDP(ourIP);
}

/////////////////////////////////////////////////////////
//
// Private init() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncDDP::initUDP(IPAddress ourIP) {
    bool success = false;
    delay(100);
    if (udp.listen(DDP_PORT)) {
        udp.onPacket(std::bind(&ESPAsyncDDP::parsePacket, this,
                std::placeholders::_1));

        success = true;
    }
    return success;
}

/////////////////////////////////////////////////////////
//
// Packet parsing - Private
//
/////////////////////////////////////////////////////////

void ESPAsyncDDP::parsePacket(AsyncUDPPacket _packet) {

  sbuff = reinterpret_cast<DDP_packet_t *>(_packet.data());
    
  pbuff->add(pbuff, sbuff);

  stats.packetsReceived++;
  stats.bytesReceived += _packet.length();

  int sn = sbuff->header.sequenceNum & 0xF;
  if (sn) {
    bool isErr = false;
    if (lastSequenceSeen) {
      if (sn == 1) {
        if (lastSequenceSeen != 15) {
          isErr = true;
        }
      } else if ((sn - 1) != lastSequenceSeen) {
        isErr = true;
      }
    }
    if (isErr) {
      stats.errors++;
    }
    lastSequenceSeen = sn;
  }
  if (stats.ddpMinChannel > htonl(sbuff->header.channelOffset)) {
    stats.ddpMinChannel = htonl(sbuff->header.channelOffset);
  }
  int mc = htonl(sbuff->header.channelOffset) + htons(sbuff->header.dataLen);
  if (mc > stats.ddpMaxChannel) {
    stats.ddpMaxChannel = mc;
  }
}
