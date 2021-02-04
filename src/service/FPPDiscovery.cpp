/*
* c_FPPDiscovery.cpp

* Copyright (c) 2021 Shelby Merrick
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
#include "fseq.h"

#include <Int64String.h>
#include "../FileMgr.hpp"
#include "../WiFiMgr.hpp"
#include "../output/OutputMgr.hpp"

extern const String VERSION;

#ifdef ARDUINO_ARCH_ESP32
#   define FPP_TYPE_ID          0xC3
#   define FPP_VARIANT_NAME     "ESPixelStick-ESP32"

#else
#   define FPP_TYPE_ID          0xC2
#   define FPP_VARIANT_NAME     "ESPixelStick-ESP8266"
#endif

#define FPP_DISCOVERY_PORT 32320
static const String ulrFilename = "filename";
static const String ulrCommand  = "command";
static const String ulrPath     = "path";

//-----------------------------------------------------------------------------
c_FPPDiscovery::c_FPPDiscovery ()
{
    // DEBUG_START;

    // DEBUG_END;
} // c_FPPDiscovery

//-----------------------------------------------------------------------------
void c_FPPDiscovery::begin ()
{
    // DEBUG_START;

    do // once
    {
        StopPlaying ();

        inFileUpload = false;
        hasBeenInitialized = true;

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

#ifdef ARDUINO_ARCH_ESP8266
        wifiConnectHandler = WiFi.onStationModeGotIP ([this](const WiFiEventStationModeGotIP& event) {this->onWiFiConnect (event); });
#else
        WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiConnect (event, info); }, WiFiEvent_t::SYSTEM_EVENT_STA_CONNECTED);
#endif

    } while (false);

    sendPingPacket ();

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
void c_FPPDiscovery::Disable ()
{
    // DEBUG_START;

    IsEnabled = false;
    StopPlaying ();
    
    // DEBUG_END;

} // Disable

//-----------------------------------------------------------------------------
void c_FPPDiscovery::Enable ()
{
    // DEBUG_START;

    IsEnabled = true;
    
    // DEBUG_END;

} // Enable

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    if (IsEnabled)
    {
        // DEBUG_V ("");
        JsonObject MyJsonStatus = jsonStatus.createNestedObject (F ("FPPDiscovery"));
        MyJsonStatus[F ("FppRemoteIp")] = FppRemoteIp.toString ();
        InputFPPRemotePlayFile.GetStatus (MyJsonStatus);
    }

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ReadNextFrame (uint8_t * CurrentOutputBuffer, uint16_t CurrentOutputBufferSize)
{
    // DEBUG_START;

    if (PlayingFile())
    {
        // DEBUG_V ("");
        InputFPPRemotePlayFile.Poll (CurrentOutputBuffer, CurrentOutputBufferSize);
    }

    // DEBUG_END;
} // ReadNextFrame

//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
uint32_t read32 (uint8_t* buf, int idx) {
    uint32_t r = (int)(buf[idx + 3]) << 24;
    r |= (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}
//-----------------------------------------------------------------------------
uint32_t read24 (uint8_t* pData)
{
    return ((uint32_t)(pData[0]) |
            (uint32_t)(pData[1]) << 8 |
            (uint32_t)(pData[2]) << 16);
} // read24
//-----------------------------------------------------------------------------
uint16_t read16 (uint8_t* pData)
{
    return ((uint16_t)(pData[0]) |
            (uint16_t)(pData[1]) << 8);
} // read16

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP8266
void c_FPPDiscovery::onWiFiConnect (const WiFiEventStationModeGotIP& event)
{
#else
void c_FPPDiscovery::onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
#endif
    // DEBUG_START;

    sendPingPacket ();

    // DEBUG_END;
} // onWiFiConnect

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessReceivedUdpPacket (AsyncUDPPacket _packet)
{
    // DEBUG_START;

    FPPPacket* packet = reinterpret_cast<FPPPacket*>(_packet.data ());
    // DEBUG_V (String ("Received packet from: ") + _packet.remoteIP ().toString ());
    // DEBUG_V (String ("             Sent to: ") + _packet.localIP ().toString ());
    // DEBUG_V (String (" packet->packet_type: ") + String(packet->packet_type));

    switch (packet->packet_type)
    {
        case 0x04: //Ping Packet
        {
            // DEBUG_V (String (F ("Ping Packet")));

            FPPPingPacket* pingPacket = reinterpret_cast<FPPPingPacket*>(_packet.data ());
            // DEBUG_V (String (F ("Ping Packet subtype: ")) + String (pingPacket->ping_subtype));
            // DEBUG_V (String (F ("Ping Packet packet.versionMajor: ")) + String (pingPacket->versionMajor));
            // DEBUG_V (String (F ("Ping Packet packet.versionMinor: ")) + String (pingPacket->versionMinor));
            // DEBUG_V (String (F ("Ping Packet packet.hostName:     ")) + String (pingPacket->hostName));
            // DEBUG_V (String (F ("Ping Packet packet.hardwareType: ")) + String (pingPacket->hardwareType));

            if (pingPacket->ping_subtype == 0x01)
            {
                // DEBUG_V (String (F ("FPP Ping discovery packet")));
                // received a discover ping packet, need to send a ping out
                if (_packet.isBroadcast () || _packet.isMulticast ())
                {
                    // DEBUG_V ("Broadcast Ping Response");
                    sendPingPacket ();
                }
                else
                {
                    // DEBUG_V ("Unicast Ping Response");
                    sendPingPacket (_packet.remoteIP ());
                }
            }
            break;
        }

        case 0x01: //Multisync packet
        {
            FPPMultiSyncPacket* msPacket = reinterpret_cast<FPPMultiSyncPacket*>(_packet.data ());
            // DEBUG_V (String (F ("msPacket->sync_type: ")) + String(msPacket->sync_type));

            if (msPacket->sync_type == 0x00)
            {
                //FSEQ type, not media
                // DEBUG_V (String (F ("Received FPP FSEQ sync packet")));
                FppRemoteIp = _packet.remoteIP ();
                ProcessSyncPacket (msPacket->sync_action, msPacket->filename, msPacket->frame_number);
            }
            break;
        }

        case 0x03: //Blank packet
        {
            // DEBUG_V (String (F ("FPP Blank packet")));
            StopPlaying ();
            ProcessBlankPacket ();
            break;
        }

        default:
        {
            // DEBUG_V (String ("UnHandled PDU: packet_type:  ") + String (packet->packet_type));
            break;
        }
    }

    // DEBUG_END;
} // ProcessReceivedUdpPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessSyncPacket (uint8_t action, String FileName, uint32_t FrameId)
{
    // DEBUG_START;
    do // once
    {
        if (!AllowedToRemotePlayFiles ())
        {
            break;
        }

        // DEBUG_V (String("action: ") + String(action));

        switch (action)
        {
            case 0x00: // Start
            {
                // DEBUG_V ("Start");
                StartPlaying (FileName, FrameId);
                break;
            }

            case 0x01: // Stop
            {
                // DEBUG_V ("Stop");
                StopPlaying ();
                break;
            }

            case 0x02: // Sync
            {
                // DEBUG_V ("Sync");
                // DEBUG_V (String ("PlayingFile: ") + PlayingFile ());
                // DEBUG_V (String ("   FileName: ") + FileName);
                // DEBUG_V (String ("  IsEnabled: ") + IsEnabled);
                // DEBUG_V (String ("GetFileName: ") + InputFPPRemotePlayFile.GetFileName ());
                // DEBUG_V (String ("    FrameId: ") + FrameId);

                if (!PlayingFile() || FileName != InputFPPRemotePlayFile.GetFileName())
                {
                    // DEBUG_V ("Do Sync based Start");
                    StartPlaying (FileName, FrameId);
                }
                else if (PlayingFile())
                {
                    // DEBUG_V ("Do Sync");
                    InputFPPRemotePlayFile.Sync (FrameId);
                }
                break;
            }

            case 0x03: // Open
            {
                // DEBUG_V ("Open");
                // StartPlaying (FileName, FrameId);
                break;
            }

            default:
            {
                // DEBUG_V (String (F ("ERROR: Unknown Action: ")) + String (action));
                break;
            }

        } // switch
    } while (false);

    // DEBUG_END;
} // ProcessSyncPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessBlankPacket ()
{
    // DEBUG_START;

    if (IsEnabled)
    {
        memset (OutputMgr.GetBufferAddress(), 0x0, OutputMgr.GetBufferUsedSize ());
    }
    // DEBUG_END;
} // ProcessBlankPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::sendPingPacket (IPAddress destination)
{
    // DEBUG_START;

    FPPPingPacket packet;
    memset (packet.raw, 0, sizeof (packet));
    packet.raw[0] = 'F';
    packet.raw[1] = 'P';
    packet.raw[2] = 'P';
    packet.raw[3] = 'D';
    packet.packet_type = 0x04;
    packet.data_len = 294;
    packet.ping_version = 0x3;
    packet.ping_subtype = 0x0;
    packet.ping_hardware = FPP_TYPE_ID;

    const char* version = VERSION.c_str ();
    uint16_t v = (uint16_t)atoi (version);
    packet.versionMajor = (v >> 8) + ((v & 0xFF) << 8);
    v = (uint16_t)atoi (&version[2]);
    packet.versionMinor = (v >> 8) + ((v & 0xFF) << 8);

    packet.operatingMode = (AllowedToRemotePlayFiles ()) ? 0x08 : 0x01; // Support remote mode : Bridge Mode

    uint32_t ip = static_cast<uint32_t>(WiFi.localIP ());
    memcpy (packet.ipAddress, &ip, 4);
    strcpy (packet.hostName, config.hostname.c_str ());
    strcpy (packet.version, (VERSION + String (":") + BUILD_DATE).c_str ());
    strcpy (packet.hardwareType, FPP_VARIANT_NAME);
    packet.ranges[0] = 0;

    // DEBUG_V ("Ping to " + destination.toString());
    udp.writeTo (packet.raw, sizeof (packet), destination, FPP_DISCOVERY_PORT);

    // DEBUG_END;
} // sendPingPacket

// #define PRINT_DEBUG
#ifdef PRINT_DEBUG
//-----------------------------------------------------------------------------
static void printReq (AsyncWebServerRequest* request, bool post)
{
    // DEBUG_START;

    int params = request->params ();
    // DEBUG_V (String ("   Num Params: ") + String (params));

    for (int i = 0; i < params; i++)
    {
        // DEBUG_V (String ("current Param: ") + String (i));
        AsyncWebParameter* p = request->getParam (i);
        // DEBUG_V (String ("      p->name: ") + String (p->name()));
        // DEBUG_V (String ("     p->value: ") + String (p->value()));

        if (p->isFile ())
        { //p->isPost() is also true
            LOG_PORT.printf_P( PSTR("FILE[%s]: %s, size: %u\n"), p->name ().c_str (), p->value ().c_str (), p->size ());
        }
        else if (p->isPost ())
        {
            LOG_PORT.printf_P( PSTR("POST[%s]: %s\n"), p->name ().c_str (), p->value ().c_str ());
        }
        else
        {
            LOG_PORT.printf_P ( PSTR("GET[%s]: %s\n"), p->name ().c_str (), p->value ().c_str ());
        }
    }
    // DEBUG_END;
} // printReq
#else
#define printReq
#endif // !def PRINT_DEBUG

//-----------------------------------------------------------------------------
void c_FPPDiscovery::BuildFseqResponse (String fname, c_FileMgr::FileId fseq, String & resp)
{
    // DEBUG_START;

    DynamicJsonDocument JsonDoc (4*1024);
    JsonObject JsonData = JsonDoc.to<JsonObject> ();

    FSEQHeader fsqHeader;
    FileMgr.ReadSdFile (fseq, (byte*)&fsqHeader, sizeof (fsqHeader), 0);

    JsonData[F("Name")]            = fname;
    JsonData[F("Version")]         = String (fsqHeader.majorVersion) + "." + String (fsqHeader.minorVersion);
    JsonData[F("ID")]              = int64String (fsqHeader.id);
    JsonData[F("StepTime")]        = String (fsqHeader.stepTime);
    JsonData[F("NumFrames")]       = String (fsqHeader.TotalNumberOfFramesInSequence);
    JsonData[F("CompressionType")] = fsqHeader.compressionType;

    uint32_t maxChannel = fsqHeader.channelCount;

    if (0 != fsqHeader.numSparseRanges)
    {
        JsonArray  JsonDataRanges = JsonData.createNestedArray (F("Ranges"));

        maxChannel = 0;

        uint8_t* RangeDataBuffer = (uint8_t*)malloc (6 * fsqHeader.numSparseRanges);
        FSEQRangeEntry* CurrentFSEQRangeEntry = (FSEQRangeEntry*)RangeDataBuffer;

        FileMgr.ReadSdFile (fseq, RangeDataBuffer, sizeof (FSEQRangeEntry), fsqHeader.numCompressedBlocks * 8 + 32);

        for (int CurrentRangeIndex = 0;
             CurrentRangeIndex < fsqHeader.numSparseRanges;
             CurrentRangeIndex++, CurrentFSEQRangeEntry++)
        {
            uint32_t RangeStart  = read24 (CurrentFSEQRangeEntry->Start);
            uint32_t RangeLength = read24 (CurrentFSEQRangeEntry->Length);

            JsonObject JsonRange = JsonDataRanges.createNestedObject ();
            JsonRange[F("Start")]  = String (RangeStart);
            JsonRange[F("Length")] = String (RangeLength);

            if ((RangeStart + RangeLength - 1) > maxChannel)
            {
                maxChannel = RangeStart + RangeLength - 1;
            }
        }

        free (RangeDataBuffer);
    }

    JsonData[F("MaxChannel")]   = String (maxChannel);
    JsonData[F("ChannelCount")] = String (fsqHeader.channelCount);

    uint32_t FileOffsetToCurrentHeaderRecord = read16 ((uint8_t*)&fsqHeader.headerLen);
    uint32_t FileOffsetToStartOfSequenceData = read16 ((uint8_t*)&fsqHeader.dataOffset); // DataOffset

    // DEBUG_V (String ("FileOffsetToCurrentHeaderRecord: ") + String (FileOffsetToCurrentHeaderRecord));
    // DEBUG_V (String ("FileOffsetToStartOfSequenceData: ") + String (FileOffsetToStartOfSequenceData));

    if (FileOffsetToCurrentHeaderRecord < FileOffsetToStartOfSequenceData)
    {
        JsonArray  JsonDataHeaders = JsonData.createNestedArray (F("variableHeaders"));

        char FSEQVariableDataHeaderBuffer[sizeof (FSEQVariableDataHeader) + 1];
        memset ((uint8_t*)FSEQVariableDataHeaderBuffer, 0x00, sizeof (FSEQVariableDataHeaderBuffer));
        FSEQVariableDataHeader* pCurrentVariableHeader = (FSEQVariableDataHeader*)FSEQVariableDataHeaderBuffer;

        while (FileOffsetToCurrentHeaderRecord < FileOffsetToStartOfSequenceData)
        {
            FileMgr.ReadSdFile (fseq, (byte*)FSEQVariableDataHeaderBuffer, sizeof (FSEQVariableDataHeader), FileOffsetToCurrentHeaderRecord);

            int VariableDataHeaderTotalLength = read16 ((uint8_t*)&(pCurrentVariableHeader->length));
            int VariableDataHeaderDataLength  = VariableDataHeaderTotalLength - sizeof (FSEQVariableDataHeader);

            String HeaderTypeCode (pCurrentVariableHeader->type);

            if ((HeaderTypeCode == "mf") || (HeaderTypeCode == "sp") )
            {
                char * VariableDataHeaderDataBuffer = (char*)malloc (VariableDataHeaderDataLength + 1);
                memset (VariableDataHeaderDataBuffer, 0x00, VariableDataHeaderDataLength + 1);

                FileMgr.ReadSdFile (fseq, (byte*)VariableDataHeaderDataBuffer, VariableDataHeaderDataLength, FileOffsetToCurrentHeaderRecord);

                JsonObject JsonDataHeader = JsonDataHeaders.createNestedObject ();
                JsonDataHeader[HeaderTypeCode] = String (VariableDataHeaderDataBuffer);

                free (VariableDataHeaderDataBuffer);
            }

            FileOffsetToCurrentHeaderRecord += VariableDataHeaderTotalLength + sizeof (FSEQVariableDataHeader);
        } // while there are headers to process
    } // there are headers to process

    serializeJson (JsonData, resp);
    // DEBUG_V (String ("resp: ") + resp);

    // DEBUG_END;

} // BuildFseqResponse

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessGET (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, false);

    do // once
    {
        if (!request->hasParam (ulrPath))
        {
            request->send (404);
            // DEBUG_V ("");
            break;
        }

        // DEBUG_V ("");

        String path = request->getParam (ulrPath)->value ();

        // DEBUG_V (String ("Path: ") + path);

        if (path.startsWith (F("/api/sequence/")) && AllowedToRemotePlayFiles())
        {
            // DEBUG_V ("");

            String seq = path.substring (14);
            if (seq.endsWith (F("/meta")))
            {
                // DEBUG_V ("");

                seq = seq.substring (0, seq.length () - 5);
                StopPlaying ();

                c_FileMgr::FileId FileHandle;
                // DEBUG_V (String (" seq: ") + seq);

                if (FileMgr.OpenSdFile (seq, c_FileMgr::FileMode::FileRead, FileHandle))
                {
                    if (FileMgr.GetSdFileSize(FileHandle) > 0)
                    {
                        // DEBUG_V ("found the file. return metadata as json");
                        String resp = "";
                        BuildFseqResponse (seq, FileHandle, resp);
                        FileMgr.CloseSdFile (FileHandle);
                        request->send (200, F ("application/json"), resp);
                        break;
                    }
                }
                LOG_PORT.println (String (F ("FPP Discovery: Could not open: ")) + seq);
            }
        }
        request->send (404);

    } while (false); // do once

    // DEBUG_END;

} // ProcessGET

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessPOST (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, true);

    do // once
    {
        String path = request->getParam (ulrPath)->value ();
        // DEBUG_V (String(F("path: ")) + path);

        if (path != F("uploadFile"))
        {
            // DEBUG_V ("");
            request->send (404);
            break;
        }

        String filename = request->getParam (ulrFilename)->value ();
        // DEBUG_V (String(F("FileName: ")) + filename);

        c_FileMgr::FileId FileHandle;
        if (false == FileMgr.OpenSdFile (filename, c_FileMgr::FileMode::FileRead, FileHandle))
        {
            LOG_PORT.println (String (F ("c_FPPDiscovery::ProcessPOST: File Does Not Exist - FileName: ")) + filename);
            request->send (404);
            break;
        }

        // DEBUG_V ("BuildFseqResponse");
        String resp = "";
        BuildFseqResponse (filename, FileHandle, resp);
        FileMgr.CloseSdFile (FileHandle);
        request->send (200, F ("application/json"), resp);

    } while (false);

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFile (AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
{
    // DEBUG_START;
    //LOG_PORT.printf_P( PSTR("In ProcessFile: %s    idx: %d    RangeLength: %d    final: %d\n)",FileName.c_str(), index, RangeLength, final? 1 : 0);

    //printReq(request, false);
    request->send (404);
    // DEBUG_END;

} // ProcessFile

//-----------------------------------------------------------------------------
// the blocks come in very small (~500 bytes) we'll accumulate in a buffer
// so the writes out to SD can be more in line with what the SD file system can handle
// #define BUFFER_LEN 8192
void c_FPPDiscovery::ProcessBody (AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
    // DEBUG_START;
    printReq (request, false);

    if (!index)
    {
        // LOG_PORT.printf_P( PSTR("In process Body:    idx: %d    RangeLength: %d    total: %d\n)", index, RangeLength, total);
        printReq (request, false);

        String path = request->getParam (ulrPath)->value ();
        if (path == F("uploadFile"))
        {
            if (!request->hasParam (ulrFilename))
            {
                // DEBUG_V ("Missing Filename Parameter");
            }
            else
            {
                StopPlaying ();
                inFileUpload = true;
                // DEBUG_V (String ("request: ") + String (uint32_t (request), HEX));
                UploadFileName = String (request->getParam (ulrFilename)->value ());
                // DEBUG_V ("");
            }
        }
    }

    if (inFileUpload)
    {
        FileMgr.handleFileUpload (UploadFileName, index, data, len, total <= (index + len));

        if (index + len == total)
        {
            inFileUpload = false;
        }
    }

    // DEBUG_END;
} // ProcessBody

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetSysInfoJSON (JsonObject & jsonResponse)
{
    // DEBUG_START;

    jsonResponse[F ("HostName")]        = config.hostname;
    jsonResponse[F ("HostDescription")] = config.id;
    jsonResponse[F ("Platform")]        = F("ESPixelStick");
    jsonResponse[F ("Variant")]         = FPP_VARIANT_NAME;
    jsonResponse[F ("Mode")]            = (true == AllowedToRemotePlayFiles()) ? 8 : 1;
    jsonResponse[F ("Version")]         = VERSION + String (":") + BUILD_DATE;

    const char* version = VERSION.c_str ();
    uint16_t v = (uint16_t)atoi (version);

    jsonResponse[F ("majorVersion")] = (uint16_t)atoi (version);
    jsonResponse[F ("minorVersion")] = (uint16_t)atoi (&version[2]);
    jsonResponse[F ("typeId")]       = FPP_TYPE_ID;

    JsonObject jsonResponseUtilization = jsonResponse.createNestedObject (F("Utilization"));
    jsonResponseUtilization[F("MemoryFree")] = ESP.getFreeHeap ();
    jsonResponseUtilization[F("Uptime")]     = millis ();

    jsonResponse[F ("rssi")] = WiFi.RSSI ();
    JsonArray jsonResponseIpAddresses = jsonResponse.createNestedArray (F ("IPS"));
    jsonResponseIpAddresses.add(WiFi.localIP ().toString ());

    // DEBUG_END;

} // GetSysInfoJSON

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFPPJson (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, false);

    do // once
    {
        if (!request->hasParam (ulrCommand))
        {
            request->send (404);
            // DEBUG_V (String ("Missing Param: 'command' "));

            break;
        }

        DynamicJsonDocument JsonDoc (2048);
        JsonObject JsonData = JsonDoc.to<JsonObject> ();

        String command = request->getParam (ulrCommand)->value ();
        // DEBUG_V (String ("command: ") + command);

        if (command == F("getFPPstatus"))
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

            JsonData[F("volume")]         = 70;
            JsonData[F("media_filename")] = "";
            JsonData[F("fppd")]           = F("running");
            JsonData[F("current_song")]   = "";

            if (false == PlayingFile())
            {
                JsonData[F ("current_sequence")]  = "";
                JsonData[F ("playlist")]          = "";
                JsonData[F ("seconds_elapsed")]   = String (0);
                JsonData[F ("seconds_played")]    = String (0);
                JsonData[F ("seconds_remaining")] = String (0);
                JsonData[F ("sequence_filename")] = "";
                JsonData[F ("time_elapsed")]      = String("00:00");
                JsonData[F ("time_remaining")]    = String ("00:00");

                JsonData[F ("status")] = 0;
                JsonData[F ("status_name")] = F ("idle");

                if (IsEnabled)
                {
                    JsonData[F ("mode")] = 8;
                    JsonData[F ("mode_name")] = F ("remote");
                }
                else
                {
                    JsonData[F ("mode")] = 1;
                    JsonData[F ("mode_name")] = F ("bridge");
                }
            }
            else
            {
                InputFPPRemotePlayFile.GetStatus (JsonData);
                JsonData[F ("status")] = 1;
                JsonData[F ("status_name")] = F ("playing");

                JsonData[F ("mode")] = 8;
                JsonData[F ("mode_name")] = F ("remote");
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

        if (command == F("getSysInfo"))
        {
            GetSysInfoJSON (JsonData);

            String resp = "";
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("JsonDoc: ") + resp);
            request->send (200, F("application/json"), resp);

            break;
        }

        if (command == F("getHostNameInfo"))
        {
            JsonData[F("HostName")] = config.hostname;
            JsonData[F("HostDescription")] = config.id;

            String resp;
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("resp: ") + resp);
            request->send (200, F("application/json"), resp);

            break;
        }

        if (command == F ("getChannelOutputs"))
        {
            if (request->hasParam (F("file")))
            {
                String filename = request->getParam (F ("file"))->value ();
                if (String(F ("co-other")) == filename)
                {
                    String resp;
                    OutputMgr.GetConfig (resp);

                    // DEBUG_V (String ("resp: ") + resp);
                    request->send (200, F ("application/json"), resp);

                    break;
                }
            }
        }

        // DEBUG_V (String ("Unknown command: ") + command);
        request->send (404);

    } while (false);

    // DEBUG_END;

} // ProcessFPPJson

//-----------------------------------------------------------------------------
void c_FPPDiscovery::StartPlaying (String & filename, uint32_t frameId)
{
    // DEBUG_START;
    // DEBUG_V (String("Open:: FileName: ") + filename);

    do // once
    {
        // clear the play file tracking data
        StopPlaying ();

        if (!IsEnabled)
        {
            // DEBUG_V ("Not Enabled");
            break;
        }
        // DEBUG_V ("");

        if (inFileUpload)
        {
            // DEBUG_V ("Uploading");
            break;
        }
        // DEBUG_V ("");

        if (0 == filename.length())
        {
            // DEBUG_V("Do not have a file to start");
            break;
        }
        // DEBUG_V ("Asking for file to play");

        InputFPPRemotePlayFile.SetPlayCount (1);
        InputFPPRemotePlayFile.Start (filename, frameId);
        // LOG_PORT.println (String (F ("FPPDiscovery::Playing:  '")) + filename + "'" );

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
void c_FPPDiscovery::StopPlaying ()
{
    // DEBUG_START;

    if (PlayingFile())
    {
        // DEBUG_V ("");
        LOG_PORT.println (String (F ("FPPDiscovery::StopPlaying '")) + InputFPPRemotePlayFile.GetFileName() + "'");
        InputFPPRemotePlayFile.Stop ();

        // DEBUG_V ("");
        // blank the display
        ProcessBlankPacket ();
    }

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
bool c_FPPDiscovery::AllowedToRemotePlayFiles()
{
    // DEBUG_START;

    // DEBUG_V (String ("SdCardIsInstalled: ")  + String (FileMgr.SdCardIsInstalled ()));
    // DEBUG_V (String ("        IsEnabled: ")  + String (IsEnabled));

    // DEBUG_END;

    return (FileMgr.SdCardIsInstalled() && IsEnabled);
} // AllowedToRemotePlayFiles

c_FPPDiscovery FPPDiscovery;
