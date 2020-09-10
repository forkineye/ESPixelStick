/*
* c_FPPDiscovery.cpp

* Copyright (c) 2020 Shelby Merrick
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

#include <SD.h>
#include <Int64String.h>

extern const String VERSION;

#ifdef ARDUINO_ARCH_ESP32
#define SD_CARD_PIN 5
#else
#define SD_CARD_PIN D8
#endif


//-----------------------------------------------------------------------------
typedef union 
{
    struct 
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPPacket;

typedef union
{
    struct
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
        uint8_t  ping_version;
        uint8_t  ping_subtype;
        uint8_t  ping_hardware;
        uint16_t versionMajor;
        uint16_t versionMinor;
        uint8_t  operatingMode;
        uint8_t  ipAddress[4];
        char  hostName[65];
        char  version[41];
        char  hardwareType[41];
        char  ranges[121];
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPPingPacket;
typedef union
{
    struct
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
        uint8_t sync_action;
        uint8_t sync_type;
        uint32_t frame_number;
        float  seconds_elapsed;
        char filename[250];
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPMultiSyncPacket;


struct FSEQHeader {
    uint8_t header[4];
    uint16_t dataOffset;
    uint8_t minorVersion;
    uint8_t majorVersion;
    uint16_t headerLen;
    uint32_t channelCount;
    uint32_t numFrames;
    uint8_t stepTime;
    uint8_t flags;
    uint8_t compressionType;
    uint8_t numCompressedBlocks;
    uint8_t numSparseRanges;
    uint8_t flags2;
    uint64_t id;
} __attribute__ ((packed));

//-----------------------------------------------------------------------------
c_FPPDiscovery::c_FPPDiscovery() {
    buffer = nullptr;
    bufCurPos = 0;
}

//-----------------------------------------------------------------------------
bool c_FPPDiscovery::begin(uint8_t * BufferStart, uint16_t BufferSize)
{
    outputBuffer = BufferStart;
    outputBufferSize = BufferSize;
    isRemoteRunning = false;
    fseqName = "";
    
    inFileUpload = false;
    hasSDStorage = false;
    
    // DEBUG_START;

    bool success = false;
    delay(100);

    IPAddress address = IPAddress(239, 70, 80, 80);
    if (udp.listen(FPP_DISCOVERY_PORT)) {
        LOG_PORT.println (String (F ("FPPDiscovery subscribed to broadcast")));
        if (udp.listenMulticast(address, FPP_DISCOVERY_PORT)) {
            LOG_PORT.println (String (F ("FPPDiscovery subscribed to multicast: ")) + address.toString ());
            udp.onPacket(std::bind (&c_FPPDiscovery::ProcessReceivedUdpPacket, this, std::placeholders::_1));
            success = true;
        }
    }
    if (success) {
        //try to detect if SD card is present.  If so, we'll report as remote mode instead of bridge
        if (SD.begin(SD_CARD_PIN)) {
            LOG_PORT.println();
            LOG_PORT.print("Found SD Card - Type: ");
            switch (SD.type()) {
              case sdfat::SD_CARD_TYPE_SD1:
                LOG_PORT.println("SD1");
                break;
              case sdfat::SD_CARD_TYPE_SD2:
                LOG_PORT.println("SD2");
                break;
              case sdfat::SD_CARD_TYPE_SDHC:
                LOG_PORT.println("SDHC");
                break;
              default:
                LOG_PORT.println("Unknown");
            }
            hasSDStorage = true;
        } else {
            LOG_PORT.println (String (F ("No SD card")));
            hasSDStorage = false;
        }
        
        sendPingPacket();
    }
    // DEBUG_END;

    return success;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::Process()
{
    if (isRemoteRunning) {
        uint32_t frame = (millis() - fseqStartMillis) / frameStepTime;
        if (frame != fseqCurrentFrame) {
            uint32_t pos = dataOffset + channelsPerFrame * frame;
            fseqFile.seek(pos);
            int toRead = channelsPerFrame > outputBufferSize ? outputBufferSize : channelsPerFrame;
            
            fseqFile.read(outputBuffer, toRead);
            //LOG_PORT.printf("New Frame!   Old: %d     New:  %d      Offset: %d\n", fseqCurrentFrame, frame, pos);
            fseqCurrentFrame = frame;
        }
    }
}

uint64_t read64(uint8_t *buf, int idx) {
    uint32_t r1 = (int)(buf[idx + 3]) << 24;
    r1 |= (int)(buf[idx + 2]) << 16;
    r1 |= (int)(buf[idx + 1]) << 8;
    r1 |= (int)(buf[idx]);
    
    uint32_t r2 = (int)(buf[idx + 7]) << 24;
    r2 |= (int)(buf[idx + 6]) << 16;
    r2 |= (int)(buf[idx + 5]) << 8;
    r2 |= (int)(buf[idx + 4]);
    uint64_t r = r2;
    r <<= 32;
    r |= r1;
    return r;
}
uint32_t read32(uint8_t *buf, int idx) {
    uint32_t r = (int)(buf[idx + 3]) << 24;
    r |= (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}
uint32_t read24(uint8_t *buf, int idx) {
    uint32_t r = (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}
uint16_t read16(uint8_t *buf, int idx) {
    uint16_t r = (int)(buf[idx]);
    r |= (int)(buf[idx + 1]) << 8;
    return r;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessReceivedUdpPacket(AsyncUDPPacket _packet)
{
    FPPPacket * packet = reinterpret_cast<FPPPacket*>(_packet.data ());
    //LOG_PORT.println("Received FPP packet");

    switch (packet->packet_type) {
        case 0x04: //Ping Packet
        {
            FPPPingPacket * pingPacket = reinterpret_cast<FPPPingPacket*>(_packet.data ());
            if ((pingPacket->ping_subtype == 0x00) || (pingPacket->ping_subtype == 0x01)) {
                LOG_PORT.println (String (F ("FPPPing discovery packet")));
                //discover ping packet, need to send a ping out
                sendPingPacket();
            }
        }
        break;
        case 0x01: //Multisync packet
        {
            FPPMultiSyncPacket * msPacket = reinterpret_cast<FPPMultiSyncPacket*>(_packet.data ());
            if (msPacket->sync_type == 0x00) { //FSEQ type, not media
                //LOG_PORT.println (String (F ("FPP FSEQ sync packet")));
                ProcessSyncPacket(msPacket->sync_action, msPacket->filename, msPacket->frame_number);
            }
            break;
        }
        case 0x03: //Blank packet
            //LOG_PORT.println (String (F ("FPP Blank packet")));
            ProcessBlankPacket();
            break;
        default:
            // DEBUG_V (String ("packet_type:  ") + String (packet->packet_type));
            // DEBUG_V (String ("ping_subtype: ") + String (packet->ping_subtype));
            break;
    }
}
void c_FPPDiscovery::ProcessSyncPacket(uint8_t action, String filename, uint32_t frame) {
    if (!hasSDStorage) {
        return;
    }
    switch (action) {
        case 0x00: // Start
            if (filename != fseqName) {
                ProcessSyncPacket(0x03, filename, frame); // need to open the file
            }
            if (fseqName != "") {
                isRemoteRunning = true;
                fseqStartMillis = millis() - frameStepTime * frame;
                fseqCurrentFrame = 99999999;
            }
            break;
        case 0x01: // Stop
            if (fseqName != "") {
                fseqFile.close();
            }
            isRemoteRunning = false;
            fseqName = "";
            break;
        case 0x02: // Sync
            if (!isRemoteRunning || filename != fseqName) {
                ProcessSyncPacket(0x00, filename, frame); // need to start first
            }
            if (isRemoteRunning) {
                int diff = (frame-fseqCurrentFrame);
                if (diff > 2 || diff < -2) {
                    //reset the start time which will then trigger a new frame time
                    fseqStartMillis = millis() - frameStepTime * frame;
                    //LOG_PORT.printf("Large diff %d\n", diff);
                }
            }
            break;
        case 0x03: // Open
            if (isRemoteRunning || filename != "") {
                ProcessSyncPacket(0x01, filename, frame); //need to stop first
            }
            if (!inFileUpload && failedFseqName != filename) {
                fseqFile = SD.open(filename);
                if (fseqFile.size() > 0) {
                    uint8_t buf[48];
                    fseqFile.read(buf, 48);
                    FSEQHeader * fsqHeader = reinterpret_cast<FSEQHeader*>(buf);
                    if (fsqHeader->majorVersion != 2 || fsqHeader->compressionType != 0) {
                        //not a v2 uncompressed sequence
                        failedFseqName = filename;
                        fseqFile.close();
                    } else {
                        fseqName = filename;
                        fseqCurrentFrame = 0;
                        dataOffset = fsqHeader->dataOffset;
                        channelsPerFrame = fsqHeader->channelCount;
                        frameStepTime = fsqHeader->stepTime;
                    }
                } else {
                    failedFseqName = filename;
                    fseqFile.close();
                }
            }
            break;
    }
}

void c_FPPDiscovery::ProcessBlankPacket() {
    if (!hasSDStorage) {
        return;
    }
    memset(outputBuffer, 0x0, outputBufferSize);
}


void c_FPPDiscovery::sendPingPacket() 
{
    // DEBUG_START;
    FPPPingPacket packet;
    memset(packet.raw, 0, sizeof(packet));
    packet.header[0] = 'F';
    packet.header[1] = 'P';
    packet.header[2] = 'P';
    packet.header[3] = 'D';
    packet.packet_type = 0x04;
    packet.data_len = 294;
    packet.ping_version = 0x3;
    packet.ping_subtype = 0x0; // 1 is to "discover" others, we don't need that
    
#ifdef ARDUINO_ARCH_ESP32
    packet.ping_hardware = 0xC3;  // 0xC3 is assigned for ESPixelStick on ESP32
#else
    packet.ping_hardware = 0xC2;  // 0xC2 is assigned for ESPixelStick on ESP8266
#endif
    
    const char *version = VERSION.c_str();
    uint16_t v = (uint16_t)atoi(version);
    packet.versionMajor = (v >> 8) + ((v & 0xFF) << 8);
    v = (uint16_t)atoi(&version[2]);
    packet.versionMinor = (v >> 8) + ((v & 0xFF) << 8);
    if (hasSDStorage) {
        packet.operatingMode = 0x08; // Support remote mode
    } else {
        packet.operatingMode = 0x01; // Only support bridge mode
    }
    uint32_t ip = static_cast<uint32_t>(WiFi.localIP());
    memcpy (packet.ipAddress, &ip, 4);
    strcpy (packet.hostName, config.id.c_str());
    strcpy (packet.version, version);
    strcpy (packet.hardwareType, "ESPixelStick");
    packet.ranges[0] = 0;
    
    udp.broadcastTo(packet.raw, sizeof(packet), FPP_DISCOVERY_PORT);
    // DEBUG_END;
}


static void printReq(AsyncWebServerRequest* request, bool post) {
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){ //p->isPost() is also true
        LOG_PORT.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        LOG_PORT.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        LOG_PORT.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
}


String printFSEQJSON(String fname, File fseq) {
    uint8_t buf[48];
    fseq.read(buf, 48);
    
    FSEQHeader * fsqHeader = reinterpret_cast<FSEQHeader*>(buf);

    String resp = "{\"Name\": \"" + fname + "\", \"Version\": \"";
    resp += String(fsqHeader->majorVersion);
    resp += ".";
    resp += String(fsqHeader->minorVersion);
    resp += "\", \"ID\": \"" + int64String(fsqHeader->id) + "\"";
    resp += ", \"StepTime\": ";
    resp += String(fsqHeader->stepTime);
    resp += ", \"NumFrames\": ";
    resp += String(fsqHeader->numFrames);
    
    uint32_t maxChannel = fsqHeader->channelCount;
    String ranges = "";
    if (fsqHeader->numSparseRanges) {
        maxChannel = 0;
        uint8_t *buf2 = (uint8_t*)malloc(6 * fsqHeader->numSparseRanges);
        fseq.seek(fsqHeader->numCompressedBlocks*8 + 32);
        fseq.read(buf2, 6 * fsqHeader->numSparseRanges);
        for (int x = 0; x < fsqHeader->numSparseRanges; x++) {
            uint32_t st = read24(buf2, x*6);
            uint16_t len = read24(buf2, x*6 + 3);
            if (ranges != "") {
                ranges += ", ";
            }
            ranges += "{\"Start\": " + String(st) + ", \"Length\": " + String(len) + "}";
            if ((st + len - 1) > maxChannel) {
                maxChannel = st + len - 1;
            }
        }
        free(buf2);
    }

    resp += ", \"MaxChannel\": ";
    resp += String(maxChannel);
    resp += ", \"ChannelCount\": ";
    resp += String(fsqHeader->channelCount);

    int compressionType = fsqHeader->compressionType;
    uint32_t pos = read16(buf, 8);
    uint32_t dataPos = read16(buf, 4);
    String headers = "";
    while (pos < dataPos) {
        fseq.seek(pos);
        fseq.read(buf, 4);
        buf[4] = 0;
        int l = read16(buf, 0);
        if ((buf[2] == 'm' && buf[3] == 'f')
            || (buf[2] == 's' && buf[3] == 'p')) {
            if (headers != "") {
                headers += ", ";
            }
            String h = (const char *)(&buf[2]);
            headers += "\"" + h + "\": \"";
            char *buf2 = (char*)malloc(l);
            fseq.read((uint8_t*)buf2, l - 4);
            headers += buf2;
            free(buf2);
            headers += "\"";
        }
        pos += l;
    }
    if (headers != "") {
        resp += ", \"variableHeaders\": {";
        resp += headers;
        resp += "}";
    }
    
    if (ranges != "") {
        resp += ", \"Ranges\": [" + ranges + "]";
    }
    resp += ", \"CompressionType\": ";
    resp += compressionType;
    resp += "}";
    return resp;
}

void c_FPPDiscovery::ProcessGET(AsyncWebServerRequest* request) {
    //LOG_PORT.println("In process get");
    //printReq(request, false);
    
    if (request->hasParam("path")) {
        String path = request->getParam("path")->value();
        if (path.startsWith("/api/sequence/") && hasSDStorage) {
            String seq = path.substring(14);
            if (seq.endsWith("/meta")) {
                seq = seq.substring(0, seq.length() - 5);
                ProcessSyncPacket(0x1, "", 0); //must stop
                if (SD.exists(seq)) {
                    File file = SD.open(seq);
                    if (file.size() > 0) {
                        // found the file.... return metadata as json
                        String resp = printFSEQJSON(seq, file);
                        file.close();
                        request->send(200, "application/json", resp);
                    }
                }
            }
        }
    }
    request->send(404);
}
void c_FPPDiscovery::ProcessPOST(AsyncWebServerRequest* request) {
    //printReq(request, true);
    String path = request->getParam("path")->value();
    if (path == "uploadFile") {
        String filename = request->getParam("filename")->value();
        if (SD.exists(filename)) {
            File file = SD.open(filename);
            String resp = printFSEQJSON(filename, file);
            file.close();
            request->send(200, "application/json", resp);
        } else {
            LOG_PORT.printf("File doesn't exist\n");
        }
    }
    
    request->send(404);
}
void c_FPPDiscovery::ProcessFile(AsyncWebServerRequest* request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    //LOG_PORT.printf("In process file: %s    idx: %d    len: %d    final: %d\n",filename.c_str(), index, len, final? 1 : 0);
    //printReq(request, false);
    request->send(404);
}

// the blocks come in very small (~500 bytes) we'll accumulate in a buffer
// so the writes out to SD can be more in line with what the SD file system can handle
#define BUFFER_LEN 8192

void c_FPPDiscovery::ProcessBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)  {
    if (!index){
        //LOG_PORT.printf("In process Body:    idx: %d    len: %d    total: %d\n", index, len, total);
        printReq(request, false);
        
        String path = request->getParam("path")->value();
        if (path == "uploadFile") {
            String filename = request->getParam("filename")->value();
            ProcessSyncPacket(0x1, "", 0); //must stop
            inFileUpload = true;
            fseqFile = SD.open(filename, sdfat::O_READ | sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_TRUNC);
            bufCurPos = 0;
            if (buffer == nullptr) {
                buffer = (uint8_t*)malloc(BUFFER_LEN);
            }
        }
    }
    if (inFileUpload) {
        if (buffer && ((bufCurPos + len) > BUFFER_LEN)) {
            int i = fseqFile.write(buffer, bufCurPos);
            //LOG_PORT.printf("Write1: %u/%u    Pos: %d   Resp: %d\n", index, total, bufCurPos,  i);
            bufCurPos = 0;
        }
        if ((buffer == nullptr) || (len >= BUFFER_LEN)) {
            int i = fseqFile.write(data, len);
            //LOG_PORT.printf("Write2: %u/%u    Resp: %d\n", index, total, i);
        } else {
            memcpy(&buffer[bufCurPos], data, len);
            bufCurPos += len;
        }
    }
    if (index + len == total){
        if (bufCurPos) {
            int i = fseqFile.write(buffer, bufCurPos);
            //LOG_PORT.printf("Write3: %u/%u   Pos: %d   Resp: %d\n", index, total, bufCurPos, i);
        }
        fseqFile.close();
        inFileUpload = false;
        free(buffer);
        buffer = nullptr;
    }
}


c_FPPDiscovery FPPDiscovery;
