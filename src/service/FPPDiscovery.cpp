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
#   define SD_CARD_DATA_PIN     5
#   define SD_OPEN_WRITEFLAGS   "w"
#   define FPP_TYPE_ID          0xC3
#   define FPP_VARIANT_NAME     "ESPixelStick-ESP32"
#   define GET_HOST_NAME        WiFi.getHostname()
#   define SD_CARD_MISO_PIN    19
#   define SD_CARD_MOSI_PIN    23 
#   define SD_CARD_CLK_PIN     18
#   define SD_CARD_CS_PIN      4

#else
#   define SD_CARD_DATA_PIN     D8
#   define SD_CARD_CS_PIN       SD_CARD_DATA_PIN
#   define SD_OPEN_WRITEFLAGS   sdfat::O_READ | sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_TRUNC
#   define FPP_TYPE_ID          0xC2
#   define FPP_VARIANT_NAME     "ESPixelStick-ESP8266"
#   define GET_HOST_NAME        WiFi.hostname().c_str()
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
    uint32_t TotalNumberOfFramesInSequence;
    uint8_t stepTime;
    uint8_t flags;
    uint8_t compressionType;
    uint8_t numCompressedBlocks;
    uint8_t numSparseRanges;
    uint8_t flags2;
    uint64_t id;
} __attribute__ ((packed));

//-----------------------------------------------------------------------------
c_FPPDiscovery::c_FPPDiscovery () 
{
    // DEBUG_START;
    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::begin ()
{
    // DEBUG_START;

    do // once
    {
        StopPlaying ();

        inFileUpload = false;
        hasSDStorage = false;

        // delay (100);

        IPAddress address = IPAddress (239, 70, 80, 80);

        // Try to listen to the broadcast port
        if (!udp.listen (FPP_DISCOVERY_PORT))
        {
            LOG_PORT.println (String (F ("FPPDiscovery FAILED to subscribed to broadcast messages")));
            break;
        }
        LOG_PORT.println (String (F ("FPPDiscovery subscribed to broadcast")));

        if (!udp.listenMulticast (address, FPP_DISCOVERY_PORT))
        {
            LOG_PORT.println (String (F ("FPPDiscovery FAILED to subscribed to multicast messages")));
            break;
        }
        LOG_PORT.println (String (F ("FPPDiscovery subscribed to multicast: ")) + address.toString ());
        udp.onPacket (std::bind (&c_FPPDiscovery::ProcessReceivedUdpPacket, this, std::placeholders::_1));

        SPI.begin (SD_CARD_CLK_PIN, SD_CARD_MISO_PIN, SD_CARD_MOSI_PIN, SD_CARD_CS_PIN);

        if (!SD.begin (SD_CARD_CS_PIN))
        {
            LOG_PORT.println (String (F ("FPPDiscovery: No SD card")));
            break;
        }

        hasSDStorage = true;

        DescribeSdCardToUser ();

    } while (false);

    sendPingPacket ();

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
void c_FPPDiscovery::DescribeSdCardToUser ()
{
    // DEBUG_START;

    String BaseMessage = F ("*** Found SD Card - Type: ");

#ifdef ARDUINO_ARCH_ESP32
    switch (SD.cardType ())
    {
        case CARD_NONE:
        {
            hasSDStorage = false;
            BaseMessage += F ("NONE");
            break;
        }

        case CARD_MMC:
        {
            BaseMessage += F ("MMC");
            break;
        }

        case CARD_SD:
        {
            BaseMessage += F ("SD");
            break;
        }

        case CARD_SDHC:
        {
            BaseMessage += F ("SDHC");
            break;
        }

        default:
        {
            BaseMessage += F ("Unknown");
            break;
        }
    } // switch (SD.cardType ())
#else
    switch (SD.type ())
    {
        case sdfat::SD_CARD_TYPE_SD1:
        {
            BaseMessage += F ("SD1");
            break;
        }
        case sdfat::SD_CARD_TYPE_SD2:
        {
            BaseMessage += F ("SD2");
            break;
        }
        case sdfat::SD_CARD_TYPE_SDHC:
        {
            BaseMessage += F ("SDHC");
            break;
        }
        default:
        {
            BaseMessage += F ("Unknown");
            break;
        }
    } // switch (SD.type ())
#endif

    LOG_PORT.println (BaseMessage);

    uint64_t cardSize = SD.cardSize () / (1024 * 1024);
    LOG_PORT.println (String(F("SD Card Size: ")) + int64String(cardSize) + "MB");

    File root = SD.open ("/");

    printDirectory (root, 0);

    // DEBUG_END;

} // DescribeSdCardToUser

//-----------------------------------------------------------------------------
void c_FPPDiscovery::printDirectory (File dir, int numTabs)
{
    while (true)
    {
        File entry = dir.openNextFile ();

        if (!entry)
        {
            // no more files
            break;
        }
        for (uint8_t i = 0; i < numTabs; i++)
        {
            Serial.print ('\t');
        }
        Serial.print (entry.name ());
        if (entry.isDirectory ())
        {
            Serial.println ("/");
            printDirectory (entry, numTabs + 1);
        }
        else
        {
            // files have sizes, directories do not
            Serial.print ("\t\t");
            Serial.println (entry.size (), DEC);
        }
        entry.close ();
    }
} // printDirectory

//-----------------------------------------------------------------------------
void c_FPPDiscovery::Process ()
{
    // DEBUG_START;
#ifdef foo
    if (isRemoteRunning)
    {
        uint32_t frame = (millis () - fseqStartMillis) / frameStepTime;
        if (frame != fseqCurrentFrame)
        {
            uint32_t pos = dataOffset + channelsPerFrame * frame;
            fseqFile.seek (pos);
            int toRead = channelsPerFrame > outputBufferSize ? outputBufferSize : channelsPerFrame;

            fseqFile.read (outputBuffer, toRead);
            //LOG_PORT.printf("New Frame!   Old: %d     New:  %d      Offset: %d\n", fseqCurrentFrameId, frame, pos);
            fseqCurrentFrame = frame;
        }
    }
#endif // def foo
    // DEBUG_END;
} // Process

uint64_t read64 (uint8_t* buf, int idx) {
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
uint32_t read32 (uint8_t* buf, int idx) {
    uint32_t r = (int)(buf[idx + 3]) << 24;
    r |= (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}
uint32_t read24 (uint8_t* buf, int idx) {
    uint32_t r = (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}
uint16_t read16 (uint8_t* buf, int idx) {
    uint16_t r = (int)(buf[idx]);
    r |= (int)(buf[idx + 1]) << 8;
    return r;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessReceivedUdpPacket (AsyncUDPPacket _packet)
{
    DEBUG_START;

    FPPPacket* packet = reinterpret_cast<FPPPacket*>(_packet.data ());
    DEBUG_V ("Received FPP packet");
    DEBUG_V (String("packet->packet_type: ") + String(packet->packet_type));

    switch (packet->packet_type) 
    {
        case 0x04: //Ping Packet
        {
            FPPPingPacket* pingPacket = reinterpret_cast<FPPPingPacket*>(_packet.data ());
            if ((pingPacket->ping_subtype == 0x00) || (pingPacket->ping_subtype == 0x01)) 
            {
                DEBUG_V (String (F ("FPPPing discovery packet")));
                // received a discover ping packet, need to send a ping out
                sendPingPacket ();
            }
            break;
        }

        case 0x01: //Multisync packet
        {
            FPPMultiSyncPacket* msPacket = reinterpret_cast<FPPMultiSyncPacket*>(_packet.data ());
            DEBUG_V (String (F ("msPacket->sync_type: ")) + String(msPacket->sync_type));

            if (msPacket->sync_type == 0x00)
            {
                //FSEQ type, not media
                DEBUG_V (String (F ("Received FPP FSEQ sync packet")));
                ProcessSyncPacket (msPacket->sync_action, msPacket->filename, msPacket->frame_number);
            }
            break;
        }

        case 0x03: //Blank packet
        {
            DEBUG_V (String (F ("FPP Blank packet")));
            ProcessBlankPacket ();
            break;
        }

        default:
        {
            DEBUG_V (String ("UnHandled PDU: packet_type:  ") + String (packet->packet_type));
            break;
        }
    }

    DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessSyncPacket (uint8_t action, String filename, uint32_t frame)
{
    DEBUG_START;

    if (!hasSDStorage) 
    {
        return;
    }

    DEBUG_V (String("action: ") + String(action));

    switch (action) 
    {
        case 0x00: // Start
        {
            DEBUG_V("Start")
            if (filename != fseqName)
            {
                ProcessSyncPacket (0x03, filename, frame); // need to open the file
            }

            if (fseqName != "")
            {
                isRemoteRunning = true;
                fseqStartMillis = millis () - frameStepTime * frame;
                fseqCurrentFrameId = 99999999;
            }
            break;
        }

        case 0x01: // Stop
        {
            DEBUG_V ("Stop");
            if (fseqName != "")
            {
                fseqFile.close ();
            }
            StopPlaying ();
            break;
        }

        case 0x02: // Sync
        {
            DEBUG_V ("Sync");

            if (!isRemoteRunning || filename != fseqName) 
            {
                ProcessSyncPacket (0x00, filename, frame); // need to start first
            }

            if (isRemoteRunning) 
            {
                int diff = (frame - fseqCurrentFrameId);
                if (diff > 2 || diff < -2) 
                {
                    // reset the start time which will then trigger a new frame time
                    fseqStartMillis = millis () - frameStepTime * frame;
                    // DEBUF_V("Large diff %d\n", diff);
                }
            }
            break;
        }

        case 0x03: // Open
        {
            DEBUG_V (String("Open:: filename: ") + filename);

            if (isRemoteRunning || filename != "")
            {
                ProcessSyncPacket (0x01, filename, frame); //need to stop first
            }

            if (inFileUpload || failedFseqName == filename)
            {
                DEBUG_V ("Uploading or this file failed to previously open");
                break;
            }

            fseqFile = SD.open (String("/") + filename);

            if (fseqFile.size () < 1)
            {
                DEBUG_V (String ("Open:: Could not open: filename: ") + filename);
                failedFseqName = filename;
                fseqFile.close ();
                StopPlaying ();
                break;
            }

            uint8_t buf[48];
            fseqFile.seek (0);
            fseqFile.read (buf, sizeof(buf));
            FSEQHeader* fsqHeader = reinterpret_cast<FSEQHeader*>(buf);
            if (fsqHeader->majorVersion != 2 || fsqHeader->compressionType != 0)
            {
                DEBUG_V ("not a v2 uncompressed sequence");

                String resp;
                BuildFseqResponse (filename, fseqFile, resp); // todo - remove
                DEBUG_V (resp);

                failedFseqName = filename;
                fseqFile.close ();
                StopPlaying ();

                break;
            }

            DEBUG_V ("Starting file output");

            fseqName                      = filename;
            fseqCurrentFrameId            = 0;
            dataOffset                    = fsqHeader->dataOffset;
            channelsPerFrame              = fsqHeader->channelCount;
            frameStepTime                 = fsqHeader->stepTime;
            TotalNumberOfFramesInSequence = fsqHeader->TotalNumberOfFramesInSequence;

            break;
        }

        default:
        {
            DEBUG_V (String (F ("ERROR: Unknown Action: ")) + String (action));
            break;
        }

    } // switch

    DEBUG_END;
} // ProcessSyncPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessBlankPacket () 
{
    DEBUG_START;
#ifdef foo
    if (!hasSDStorage)
    {
        return;
    }
    memset (outputBuffer, 0x0, outputBufferSize);
#endif // def foo
    DEBUG_END;
} // ProcessBlankPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::sendPingPacket ()
{
    // DEBUG_START;
    FPPPingPacket packet;
    memset (packet.raw, 0, sizeof (packet));
    packet.header[0] = 'F';
    packet.header[1] = 'P';
    packet.header[2] = 'P';
    packet.header[3] = 'D';
    packet.packet_type = 0x04;
    packet.data_len = 294;
    packet.ping_version = 0x3;
    packet.ping_subtype = 0x0; // 1 is to "discover" others, we don't need that
    packet.ping_hardware = FPP_TYPE_ID;

    const char* version = VERSION.c_str ();
    uint16_t v = (uint16_t)atoi (version);
    packet.versionMajor = (v >> 8) + ((v & 0xFF) << 8);
    v = (uint16_t)atoi (&version[2]);
    packet.versionMinor = (v >> 8) + ((v & 0xFF) << 8);

    packet.operatingMode = (hasSDStorage) ? 0x08 : 0x01; // Support remote mode : Bridge Mode

    uint32_t ip = static_cast<uint32_t>(WiFi.localIP ());
    memcpy (packet.ipAddress, &ip, 4);
    strcpy (packet.hostName, GET_HOST_NAME);
    strcpy (packet.version, version);
    strcpy (packet.hardwareType, FPP_VARIANT_NAME);
    packet.ranges[0] = 0;

    udp.broadcastTo (packet.raw, sizeof (packet), FPP_DISCOVERY_PORT);

    // DEBUG_END;
} // sendPingPacket

#ifdef PRINT_DEBUG
//-----------------------------------------------------------------------------
static void printReq (AsyncWebServerRequest* request, bool post)
{
    int params = request->params ();
    for (int i = 0; i < params; i++) 
    {
        AsyncWebParameter* p = request->getParam (i);
        if (p->isFile ()) 
        { //p->isPost() is also true
            LOG_PORT.printf ("FILE[%s]: %s, size: %u\n", p->name ().c_str (), p->value ().c_str (), p->size ());
        }
        else if (p->isPost ()) 
        {
            LOG_PORT.printf ("POST[%s]: %s\n", p->name ().c_str (), p->value ().c_str ());
        }
        else 
        {
            LOG_PORT.printf ("GET[%s]: %s\n", p->name ().c_str (), p->value ().c_str ());
        }
    }
} // printReq
#else
#define printReq
#endif // !def PRINT_DEBUG

//-----------------------------------------------------------------------------
void c_FPPDiscovery::BuildFseqResponse (String fname, File fseq, String & resp)
{
    DEBUG_START;

    DynamicJsonDocument JsonDoc (16*1024);
    JsonObject JsonData = JsonDoc.to<JsonObject> ();

    uint8_t buf[48];
    fseq.seek (0);
    fseq.read (buf, sizeof(buf));

    FSEQHeader * fsqHeader = reinterpret_cast<FSEQHeader *>(buf);

    JsonData["Name"]      = fname;
    JsonData["Version"]   = String (fsqHeader->majorVersion) + "." + String (fsqHeader->minorVersion);
    JsonData["ID"]        = int64String (fsqHeader->id);
    JsonData["StepTime"]  = String (fsqHeader->stepTime);
    JsonData["NumFrames"] = String (fsqHeader->TotalNumberOfFramesInSequence);

    resp = "{\"Name\": \"" + fname + "\", \"Version\": \"";
    resp += String (fsqHeader->majorVersion);
    resp += ".";
    resp += String (fsqHeader->minorVersion);
    resp += "\", \"ID\": \"" + int64String (fsqHeader->id) + "\"";
    resp += ", \"StepTime\": ";
    resp += String (fsqHeader->stepTime);
    resp += ", \"NumFrames\": ";
    resp += String (fsqHeader->TotalNumberOfFramesInSequence);

    uint32_t maxChannel = fsqHeader->channelCount;
    String ranges = "";
    JsonArray  JsonDataRanges = JsonData.createNestedArray ("Ranges");

    if (fsqHeader->numSparseRanges)
    {
        maxChannel = 0;
        uint8_t* buf2 = (uint8_t*)malloc (6 * fsqHeader->numSparseRanges);
        fseq.seek (fsqHeader->numCompressedBlocks * 8 + 32);
        fseq.read (buf2, 6 * fsqHeader->numSparseRanges);
        for (int x = 0; x < fsqHeader->numSparseRanges; x++)
        {
            uint32_t st = read24 (buf2, x * 6);
            uint16_t len = read24 (buf2, x * 6 + 3);
            if (ranges != "")
            {
                ranges += ", ";
            }

            JsonObject JsonRange = JsonDataRanges.createNestedObject ();
            if (true == JsonRange.isNull ())
            {
                DEBUG_V ("Out of Doc Memory");
            }
            JsonRange["Start"]  = String (st);
            JsonRange["Length"] = String (len);

            ranges += "{\"Start\": " + String (st) + ", \"Length\": " + String (len) + "}";
            if ((st + len - 1) > maxChannel)
            {
                maxChannel = st + len - 1;
            }
        }
        free (buf2);
    }

    JsonData["MaxChannel"]   = String (maxChannel);
    JsonData["ChannelCount"] = String (fsqHeader->channelCount);

    resp += ", \"MaxChannel\": ";
    resp += String (maxChannel);
    resp += ", \"ChannelCount\": ";
    resp += String (fsqHeader->channelCount);

    int compressionType = fsqHeader->compressionType;

    JsonArray  JsonDataHeaders = JsonData.createNestedArray ("variableHeaders");

    uint32_t pos = read16 (buf, 8);
    uint32_t dataPos = read16 (buf, 4);
    String headers = "";
    while (pos < dataPos) 
    {
        fseq.seek (pos);
        fseq.read (buf, 4);
        buf[4] = 0;
        int l = read16 (buf, 0);

        if ((buf[2] == 'm' && buf[3] == 'f') ||
            (buf[2] == 's' && buf[3] == 'p'))
        {
            if (headers != "")
            {
                headers += ", ";
            }

            String h = (const char*)(&buf[2]);
            headers += "\"" + h + "\": \"";
            char* buf2 = (char*)malloc (l);
            fseq.read ((uint8_t*)buf2, l - 4);
            headers += buf2;

            JsonObject JsonDataHeader = JsonDataHeaders.createNestedObject ();
            JsonDataHeader[h] = String (buf2);

            free (buf2);
            headers += "\"";
        }
        pos += l + 4;
    }

    if (0 == JsonDataHeaders.size ())
    {
        JsonData.remove ("variableHeaders");
    }
    if (headers != "")
    {
        resp += ", \"variableHeaders\": {";
        resp += headers;
        resp += "}";
    }

    if (0 == JsonDataRanges.size ())
    {
        JsonData.remove ("Ranges");
    }
    if (ranges != "")
    {
        resp += ", \"Ranges\": [" + ranges + "]";
    }

    JsonData["CompressionType"] = compressionType;

    resp += ", \"CompressionType\": ";
    resp += compressionType;
    resp += "}";

    String temp;
    serializeJson (JsonData, temp);
    DEBUG_V (temp);

    DEBUG_END;

} // BuildFseqResponse

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessGET (AsyncWebServerRequest* request)
{
    DEBUG_START;
    printReq(request, false);

    do // once
    {
        if (!request->hasParam ("path"))
        {
            request->send (404);
            break;
        }
        
        String path = request->getParam ("path")->value ();
        if (path.startsWith ("/api/sequence/") && hasSDStorage)
        {
            String seq = path.substring (14);
            if (seq.endsWith ("/meta"))
            {
                seq = seq.substring (0, seq.length () - 5);
                ProcessSyncPacket (0x1, "", 0); //must stop
                if (SD.exists (seq))
                {
                    File file = SD.open (seq);
                    if (file.size () > 0)
                    {
                        // found the file.... return metadata as json
                        String resp = "";
                        BuildFseqResponse (seq, file, resp);
                        file.close ();
                        request->send (200, "application/json", resp);
                        break;
                    }
					else 
					{
                        LOG_PORT.printf("File doesn't exist: %s\n", seq.c_str());
                    }
                }
            }
        }
        request->send (404);

    } while (false); // do once

    DEBUG_END;

} // ProcessGET
  
//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessPOST (AsyncWebServerRequest* request)
{
    DEBUG_START;
    printReq(request, true);

    do // once
    {
        String path = request->getParam ("path")->value ();
        // DEBUG_V (String(F("path: ")) + path);

        if (path != "uploadFile")
        {
            request->send (404);
            break;
        }
        
        String filename = request->getParam ("filename")->value ();
        DEBUG_V (String(F("filename: ")) + filename);

        if (!SD.exists (filename))
        {
            LOG_PORT.println (String (F ("c_FPPDiscovery::ProcessPOST: File Does Not Exist - filename: ")) + filename);
            request->send (404);
            break;
        }

        File file = SD.open (filename);
        String resp = "";
        BuildFseqResponse (filename, file, resp);
        file.close ();
        request->send (200, F ("application/json"), resp);

    } while (false);

    DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFile (AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
{
    // DEBUG_START;
    //LOG_PORT.printf("In ProcessFile: %s    idx: %d    len: %d    final: %d\n",filename.c_str(), index, len, final? 1 : 0);
    //printReq(request, false);
    request->send (404);
    // DEBUG_END;

} // ProcessFile

// the blocks come in very small (~500 bytes) we'll accumulate in a buffer
// so the writes out to SD can be more in line with what the SD file system can handle
#define BUFFER_LEN 8192

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessBody (AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
    DEBUG_START;

    if (!index)
    {
        //LOG_PORT.printf("In process Body:    idx: %d    len: %d    total: %d\n", index, len, total);
        printReq (request, false);

        String path = request->getParam ("path")->value ();
        if (path == "uploadFile")
        {
            ProcessSyncPacket (0x1, "", 0); //must stop

            inFileUpload = true;

            String filename = request->getParam ("filename")->value ();
            SD.remove (filename);
            fseqFile = SD.open (filename, SD_OPEN_WRITEFLAGS);
            bufCurPos = 0;
            if (buffer == nullptr)
            {
                buffer = (uint8_t*)malloc (BUFFER_LEN);
            }
        }
    }

    if (inFileUpload)
    {
        if (buffer && ((bufCurPos + len) > BUFFER_LEN))
        {
            int i = fseqFile.write (buffer, bufCurPos);
            // LOG_PORT.printf("Write1: %u/%u    Pos: %d   Resp: %d\n", index, total, bufCurPos,  i);
            bufCurPos = 0;
        }

        if ((buffer == nullptr) || (len >= BUFFER_LEN))
        {
            int i = fseqFile.write (data, len);
            // LOG_PORT.printf("Write2: %u/%u    Resp: %d\n", index, total, i);
        }
        else
        {
            memcpy (&buffer[bufCurPos], data, len);
            bufCurPos += len;
        }
    }

    if (index + len == total) 
    {
        if (bufCurPos) 
        {
            int i = fseqFile.write (buffer, bufCurPos);
            //LOG_PORT.printf("Write3: %u/%u   Pos: %d   Resp: %d\n", index, total, bufCurPos, i);
        }

        fseqFile.close ();
        inFileUpload = false;
        free (buffer);
        buffer = nullptr;
    }

    DEBUG_END;
}

void c_FPPDiscovery::GetSysInfoJSON (JsonObject & jsonResponse)
{
    // DEBUG_START;

    jsonResponse[F ("HostName")]        = GET_HOST_NAME;
    jsonResponse[F ("HostDescription")] = config.id;
    jsonResponse[F ("Platform")]        = "ESPixelStick";
    jsonResponse[F ("Variant")]         = FPP_VARIANT_NAME;
    jsonResponse[F ("Mode")]            = (true == hasSDStorage) ? "remote" : "bridge";
    jsonResponse[F ("Version")]         = VERSION;

    const char* version = VERSION.c_str ();
    uint16_t v = (uint16_t)atoi (version);

    jsonResponse[F ("majorVersion")] = (uint16_t)atoi (version);
    jsonResponse[F ("minorVersion")] = (uint16_t)atoi (&version[2]);
    jsonResponse[F ("typeId")]       = FPP_TYPE_ID;

    JsonObject jsonResponseUtilization = jsonResponse.createNestedObject (F("Utilization"));
    jsonResponseUtilization["MemoryFree"] = ESP.getFreeHeap ();
    jsonResponseUtilization["Uptime"]     = millis ();

    jsonResponse[F ("rssi")] = WiFi.RSSI ();
    JsonArray jsonResponseIpAddresses = jsonResponse.createNestedArray (F ("IPS"));
    jsonResponseIpAddresses.add(WiFi.localIP ().toString ());

    // DEBUG_END;

} // GetSysInfoJSON

void c_FPPDiscovery::ProcessFPPJson (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, false);

    do // once
    {
        if (!request->hasParam ("command"))
        {
            request->send (404);
            DEBUG_V (String ("Missing Param: 'command' "));

            break;
        }

        DynamicJsonDocument JsonDoc (2048);
        JsonObject JsonData = JsonDoc.to<JsonObject> ();

        String command = request->getParam ("command")->value ();
        // DEBUG_V (String ("command: ") + command);

        if (command == "getFPPstatus")
        {
            String adv = "false";
            if (request->hasParam ("advancedView"))
            {
                adv = request->getParam ("advancedView")->value ();
            }

            JsonObject JsonDataMqtt = JsonData.createNestedObject(F("MQTT"));

            JsonDataMqtt[F ("configured")] = false;
            JsonDataMqtt[F ("connected")]  = false;

            JsonObject JsonDataCurrentPlaylist = JsonData.createNestedObject (F ("current_playlist"));

            JsonDataCurrentPlaylist[F ("count")]       = "0";
            JsonDataCurrentPlaylist[F ("description")] = "";
            JsonDataCurrentPlaylist[F ("index")]       = "0";
            JsonDataCurrentPlaylist[F ("playlist")]    = "";
            JsonDataCurrentPlaylist[F ("type")]        = "";

            JsonData["volume"]         = 70;
            JsonData["media_filename"] = "";
            JsonData["fppd"]           = "running";
            JsonData["current_song"]   = "";

            int mseconds      = fseqCurrentFrameId * frameStepTime;
            int msecondsTotal = frameStepTime * TotalNumberOfFramesInSequence;

            int secs    = mseconds / 1000;
            int secsTot = msecondsTotal / 1000;

            JsonData[F ("current_sequence")] = fseqName;
            JsonData[F ("playlist")] = fseqName;
            JsonData[F ("seconds_elapsed")] = String (secs);
            JsonData[F ("seconds_played")] = String (secs);
            JsonData[F ("seconds_remaining")] = String (secsTot - secs);
            JsonData[F ("sequence_filename")] = fseqName;

            if (false == isRemoteRunning)
            {
                JsonData[F ("status")] = 0;
                JsonData[F ("status_name")] = F ("idle");
            }
            else
            {
                JsonData[F ("status")] = 1;
                JsonData[F ("status_name")] = F ("playing");
            }

            int mins = secs / 60;
            secs = secs % 60;

            secsTot    = secsTot - secs;
            int minRem = secsTot / 60;
            secsTot    = secsTot % 60;

            char buf[8];
            sprintf (buf, "%02d:%02d", mins, secs);
            JsonData[F ("time_elapsed")] = buf;

            sprintf (buf, "%02d:%02d", minRem, secsTot);
            JsonData[F ("time_remaining")] = buf;

            if (hasSDStorage)
            {
                JsonData[F("mode")] = 8;
                JsonData[F("mode_name")] = F ("remote");
            }
            else
            {
                JsonData["mode"] = 1;
                JsonData["mode_name"] = F("bridge");
            }

            if (adv == "true")
            {
                JsonObject JsonDataAdvancedView = JsonData.createNestedObject (F ("advancedView"));
                GetSysInfoJSON (JsonDataAdvancedView);
            }

            String Response;
            serializeJson (JsonDoc, Response);
            // DEBUG_V (String ("JsonDoc: ") + Response);
            request->send (200, F("application/json"), Response);

            break;
        }

        if (command == "getSysInfo")
        {
            GetSysInfoJSON (JsonData);

            String resp = "";
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("JsonDoc: ") + resp);
            request->send (200, F("application/json"), resp);

            break;
        }

        if (command == "getHostNameInfo")
        {
            JsonData[F("HostName")] = GET_HOST_NAME;
            JsonData[F("HostDescription")] = config.id;

            String resp;
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("resp: ") + resp);
            request->send (200, F("application/json"), resp);

            break;
        }

        DEBUG_V (String ("Unknown command: ") + command);
        request->send (404);

    } while (false);

    // DEBUG_END;

} // ProcessFPPJson

void c_FPPDiscovery::StopPlaying ()
{
    DEBUG_START;

    isRemoteRunning = false;

    fseqName = "";
    fseqCurrentFrameId = 0;
    frameStepTime = 0;
    TotalNumberOfFramesInSequence = 0;
    dataOffset = 0;
    channelsPerFrame = 0;

    DEBUG_END;

} // StopPlaying


c_FPPDiscovery FPPDiscovery;
