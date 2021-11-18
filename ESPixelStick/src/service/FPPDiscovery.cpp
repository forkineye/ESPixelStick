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

#include <Arduino.h>
#include "FPPDiscovery.h"
#include "fseq.h"

#include <Int64String.h>
#include "../FileMgr.hpp"
#include "../WiFiMgr.hpp"
#include "../output/OutputMgr.hpp"
#include <time.h>

#ifdef ARDUINO_ARCH_ESP32
#   define FPP_TYPE_ID          0xC3
#   define FPP_VARIANT_NAME     (String(CN_ESPixelStick) + "-ESP32")

#else
#   define FPP_TYPE_ID          0xC2
#   define FPP_VARIANT_NAME     (String(CN_ESPixelStick) + "-ESP8266")
#endif

#define FPP_DISCOVERY_PORT 32320
static const String ulrCommand  = "command";
static const String ulrPath     = "path";

//-----------------------------------------------------------------------------
c_FPPDiscovery::c_FPPDiscovery ()
{
    // DEBUG_START;
    memset ((void*)&MultiSyncStats, 0x00, sizeof (MultiSyncStats));
    // DEBUG_END;
} // c_FPPDiscovery

//-----------------------------------------------------------------------------
void c_FPPDiscovery::begin ()
{
    // DEBUG_START;

    StopPlaying ();

    inFileUpload = false;
    hasBeenInitialized = true;

    NetworkStateChanged (WiFiMgr.IsWiFiConnected ());

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
// Configure and start the web server
void c_FPPDiscovery::NetworkStateChanged (bool NewNetworkState)
{
    // DEBUG_START;

    do // once
    {
        if (false == NewNetworkState)
        {
            break;
        }

        // DEBUG_V ();

        IPAddress address = IPAddress (239, 70, 80, 80);
        bool fail = false;

        // Try to listen to the broadcast port
        if (!udp.listen (FPP_DISCOVERY_PORT))
        {
            logcon (String (F ("FAILED to subscribed to broadcast messages")));
            fail = true;
            break;
        }
        //logcon (String (F ("FPPDiscovery subscribed to broadcast")));

        if (!udp.listenMulticast (address, FPP_DISCOVERY_PORT))
        {
            logcon (String (F ("FAILED to subscribed to multicast messages")));
            fail = true;
            break;
        }
        //logcon (String (F ("FPPDiscovery subscribed to multicast: ")) + address.toString ());

        if (!fail)
            logcon (String (F ("Listening on port ")) + String(FPP_DISCOVERY_PORT));

        udp.onPacket (std::bind (&c_FPPDiscovery::ProcessReceivedUdpPacket, this, std::placeholders::_1));

        sendPingPacket ();

    } while (false);

    // DEBUG_END;

} // NetworkStateChanged

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
        // DEBUG_V ("Is Enabled");
        JsonObject MyJsonStatus = jsonStatus.createNestedObject (F ("FPPDiscovery"));
        MyJsonStatus[F ("FppRemoteIp")] = FppRemoteIp.toString ();
        if (InputFPPRemotePlayFile)
        {
            InputFPPRemotePlayFile->GetStatus (MyJsonStatus);
        }
    }
    else
    {
        // DEBUG_V ("Not Enabled");
    }

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ReadNextFrame (uint8_t * CurrentOutputBuffer, uint16_t CurrentOutputBufferSize)
{
    // DEBUG_START;

    if (InputFPPRemotePlayFile)
    {
        InputFPPRemotePlayFile->Poll (CurrentOutputBuffer, CurrentOutputBufferSize);
    }

    // DEBUG_END;
} // ReadNextFrame

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessReceivedUdpPacket (AsyncUDPPacket UDPpacket)
{
    // DEBUG_START;
    do // once
    {
        // We're in an upload, can't be bothered
        if (inFileUpload)
        {
            // DEBUG_V ("")
            break;
        }

        FPPPacket* fppPacket = reinterpret_cast<FPPPacket*>(UDPpacket.data ());
        // DEBUG_V (String ("Received UDP packet from: ") + UDPpacket.remoteIP ().toString ());
        // DEBUG_V (String ("                 Sent to: ") + UDPpacket.localIP ().toString ());
        // DEBUG_V (String ("         FPP packet_type: ") + String(fppPacket->packet_type));

        if ((fppPacket->header[0] != 'F') ||
            (fppPacket->header[1] != 'P') ||
            (fppPacket->header[2] != 'P') ||
            (fppPacket->header[3] != 'D'))
        {
            MultiSyncStats.pktError++;
            break;
        }

        struct timeval tv;
        gettimeofday (&tv, NULL);
        MultiSyncStats.lastReceiveTime = tv.tv_sec;

        switch (fppPacket->packet_type)
        {
            case CTRL_PKT_CMD: // deprecated in favor of FPP Commands
            {
                MultiSyncStats.pktCommand++;
                // DEBUG_V ("Unsupported PDU: CTRL_PKT_CMD");
                break;
            }

            case CTRL_PKT_SYNC:
            {
                FPPMultiSyncPacket* msPacket = reinterpret_cast<FPPMultiSyncPacket*>(UDPpacket.data ());
                // DEBUG_V (String (F ("msPacket->sync_type: ")) + String(msPacket->sync_type));

                if (msPacket->sync_type == SYNC_FILE_SEQ)
                {
                    // FSEQ type, not media
                    // DEBUG_V (String (F ("Received FPP FSEQ sync packet")));
                    FppRemoteIp = UDPpacket.remoteIP ();
                    ProcessSyncPacket (msPacket->sync_action, String (msPacket->filename), msPacket->seconds_elapsed);
                }
                else if (msPacket->sync_type == SYNC_FILE_MEDIA)
                {
                    // DEBUG_V (String (F ("Unsupported SYNC_FILE_MEDIA message.")));
                }
                else
                {
                    // DEBUG_V (String (F ("Unexpected Multisync msPacket->sync_type: ")) + String (msPacket->sync_type));
                }

                break;
            }

            case CTRL_PKT_EVENT: // deprecated in favor of FPP Commands
            {
                // DEBUG_V ("Unsupported PDU: CTRL_PKT_EVENT");
                break;
            }

            case CTRL_PKT_BLANK:
            {
                // DEBUG_V (String (F ("FPP Blank packet")));
                // StopPlaying ();
                MultiSyncStats.pktBlank++;
                ProcessBlankPacket ();
                break;
            }

            case CTRL_PKT_PING:
            {
                // DEBUG_V (String (F ("Ping Packet")));

                MultiSyncStats.pktPing++;
                FPPPingPacket* pingPacket = reinterpret_cast<FPPPingPacket*>(UDPpacket.data ());

                // DEBUG_V (String (F ("Ping Packet subtype: ")) + String (pingPacket->ping_subtype));
                // DEBUG_V (String (F ("Ping Packet packet.versionMajor: ")) + String (pingPacket->versionMajor));
                // DEBUG_V (String (F ("Ping Packet packet.versionMinor: ")) + String (pingPacket->versionMinor));
                // DEBUG_V (String (F ("Ping Packet packet.hostName:     ")) + String (pingPacket->hostName));
                // DEBUG_V (String (F ("Ping Packet packet.hardwareType: ")) + String (pingPacket->hardwareType));

                if (pingPacket->ping_subtype == 0x01)
                {
                    // DEBUG_V (String (F ("FPP Ping discovery packet")));
                    // received a discover ping packet, need to send a ping out
                    if (UDPpacket.isBroadcast () || UDPpacket.isMulticast ())
                    {
                        // DEBUG_V ("Broadcast Ping Response");
                        sendPingPacket ();
                    }
                    else
                    {
                        // DEBUG_V ("Unicast Ping Response");
                        sendPingPacket (UDPpacket.remoteIP ());
                    }
                }
                else
                {
                    // DEBUG_V (String (F ("Unexpected Ping sub type: ")) + String (pingPacket->ping_subtype));
                }
                break;
            }

            case CTRL_PKT_PLUGIN:
            {
                // DEBUG_V ("Unsupported PDU: CTRL_PKT_PLUGIN");
                MultiSyncStats.pktPlugin++;
                break;
            }

            case CTRL_PKT_FPPCOMMAND:
            {
                // DEBUG_V ("Unsupported PDU: CTRL_PKT_FPPCOMMAND");
                MultiSyncStats.pktFPPCommand++;
                break;
            }

            default:
            {
                // DEBUG_V (String ("UnHandled PDU: packet_type:  ") + String (fppPacket->packet_type));
                break;
            }
        } // switch (fppPacket->packet_type)
    } while (false);

    // DEBUG_END;
} // ProcessReceivedUdpPacket

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessSyncPacket (uint8_t action, String FileName, float SecondsElapsed)
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
            case SYNC_PKT_START:
            {
                // DEBUG_V ("Sync::Start");
                // DEBUG_V (String ("   FileName: ") + FileName);
                // DEBUG_V (String ("    FrameId: ") + FrameId);

                MultiSyncStats.pktSyncSeqStart++;
                StartPlaying (FileName, SecondsElapsed);
                break;
            }

            case SYNC_PKT_STOP:
            {
                // DEBUG_V ("Sync::Stop");
                // DEBUG_V (String ("   FileName: ") + FileName);
                // DEBUG_V (String ("    FrameId: ") + FrameId);

                MultiSyncStats.pktSyncSeqStop++;
                StopPlaying ();
                break;
            }

            case SYNC_PKT_SYNC:
            {
                // DEBUG_V ("Sync");
                // DEBUG_V (String ("PlayingFile: ") + PlayingFile ());
                // DEBUG_V (String ("   FileName: ") + FileName);
                // DEBUG_V (String ("  IsEnabled: ") + IsEnabled);
                // DEBUG_V (String ("GetFileName: ") + InputFPPRemotePlayFile.GetFileName ());
                // DEBUG_V (String ("    FrameId: ") + FrameId);

                /*
                logcon (String(float(millis()/1000.0)) + "," +
                                  String(InputFPPRemotePlayFile.GetLastFrameId()) + "," +
                                  String (seconds_elapsed) + "," +
                                  String (FrameId) + "," +
                                  String(InputFPPRemotePlayFile.GetTimeOffset(),5));
                */
                MultiSyncStats.pktSyncSeqSync++;
                // DEBUG_V (String ("SecondsElapsed: ") + String (SecondsElapsed));
                if (InputFPPRemotePlayFile)
                {
                    InputFPPRemotePlayFile->Sync (FileName, SecondsElapsed);
                }
                break;
            }

            case SYNC_PKT_OPEN:
            {
                // DEBUG_V ("Sync::Open");
                // DEBUG_V (String ("   FileName: ") + FileName);
                // DEBUG_V (String ("    FrameId: ") + FrameId);
                // StartPlaying (FileName, FrameId);
                MultiSyncStats.pktSyncSeqOpen++;
                break;
            }

            default:
            {
                // DEBUG_V (String (F ("Sync: ERROR: Unknown Action: ")) + String (action));
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

    packet.operatingMode = (FileMgr.SdCardIsInstalled ()) ? 0x08 : 0x01; // Support remote mode : Bridge Mode

    uint32_t ip = static_cast<uint32_t>(WiFi.localIP ());
    memcpy (packet.ipAddress, &ip, 4);
    strcpy (packet.hostName, config.hostname.c_str ());
    strcpy (packet.version, VERSION.c_str());
    strcpy (packet.hardwareType, FPP_VARIANT_NAME.c_str());
    packet.ranges[0] = 0;

    // DEBUG_V ("Send Ping to " + destination.toString());
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
#define printReq(a,b)
#endif // !def PRINT_DEBUG

//-----------------------------------------------------------------------------
void c_FPPDiscovery::BuildFseqResponse (String fname, c_FileMgr::FileId fseq, String & resp)
{
    // DEBUG_START;

    DynamicJsonDocument JsonDoc (4*1024);
    JsonObject JsonData = JsonDoc.to<JsonObject> ();

    FSEQRawHeader fsqHeader;
    FileMgr.ReadSdFile (fseq, (byte*)&fsqHeader, sizeof (fsqHeader), 0);

    JsonData[F ("Name")]            = fname;
    JsonData[CN_Version]            = String (fsqHeader.majorVersion) + "." + String (fsqHeader.minorVersion);
    JsonData[F ("ID")]              = int64String (read64 (fsqHeader.id, 0));
    JsonData[F ("StepTime")]        = String (fsqHeader.stepTime);
    JsonData[F ("NumFrames")]       = String (read32 (fsqHeader.TotalNumberOfFramesInSequence, 0));
    JsonData[F ("CompressionType")] = fsqHeader.compressionType;

    char timeStr[32];
    memset (timeStr, 0, sizeof (timeStr));
    struct tm tm = *gmtime (&MultiSyncStats.lastReceiveTime);
    sprintf (timeStr, "%4d-%.2d-%.2d %.2d:%.2d:%.2d",
        1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    JsonData[F ("lastReceiveTime")] = timeStr;
    JsonData[F ("pktCommand")]      = MultiSyncStats.pktCommand;
    JsonData[F ("pktSyncSeqOpen")]  = MultiSyncStats.pktSyncSeqOpen;
    JsonData[F ("pktSyncSeqStart")] = MultiSyncStats.pktSyncSeqStart;
    JsonData[F ("pktSyncSeqStop")]  = MultiSyncStats.pktSyncSeqStop;
    JsonData[F ("pktSyncSeqSync")]  = MultiSyncStats.pktSyncSeqSync;
    JsonData[F ("pktSyncMedOpen")]  = MultiSyncStats.pktSyncMedOpen;
    JsonData[F ("pktSyncMedStart")] = MultiSyncStats.pktSyncMedStart;
    JsonData[F ("pktSyncMedStop")]  = MultiSyncStats.pktSyncMedStop;
    JsonData[F ("pktSyncMedSync")]  = MultiSyncStats.pktSyncMedSync;
    JsonData[F ("pktBlank")]        = MultiSyncStats.pktBlank;
    JsonData[F ("pktPing")]         = MultiSyncStats.pktPing;
    JsonData[F ("pktPlugin")]       = MultiSyncStats.pktPlugin;
    JsonData[F ("pktFPPCommand")]   = MultiSyncStats.pktFPPCommand;
    JsonData[F ("pktError")]        = MultiSyncStats.pktError;

    uint32_t maxChannel = read32 (fsqHeader.channelCount, 0);

    if (0 != fsqHeader.numSparseRanges)
    {
        JsonArray  JsonDataRanges = JsonData.createNestedArray (F ("Ranges"));

        maxChannel = 0;

        uint8_t* RangeDataBuffer = (uint8_t*)malloc (sizeof(FSEQRawRangeEntry) * fsqHeader.numSparseRanges);
        FSEQRawRangeEntry* CurrentFSEQRangeEntry = (FSEQRawRangeEntry*)RangeDataBuffer;

        FileMgr.ReadSdFile (fseq, RangeDataBuffer, sizeof (FSEQRawRangeEntry), fsqHeader.numCompressedBlocks * 8 + 32);

        for (int CurrentRangeIndex = 0;
             CurrentRangeIndex < fsqHeader.numSparseRanges;
             CurrentRangeIndex++, CurrentFSEQRangeEntry++)
        {
            uint32_t RangeStart  = read24 (CurrentFSEQRangeEntry->Start);
            uint32_t RangeLength = read24 (CurrentFSEQRangeEntry->Length);

            JsonObject JsonRange = JsonDataRanges.createNestedObject ();
            JsonRange[F ("Start")]  = String (RangeStart);
            JsonRange[F ("Length")] = String (RangeLength);

            if ((RangeStart + RangeLength - 1) > maxChannel)
            {
                maxChannel = RangeStart + RangeLength - 1;
            }
        }

        free (RangeDataBuffer);
    }

    JsonData[F ("MaxChannel")]   = String (maxChannel);
    JsonData[F ("ChannelCount")] = String (read32 (fsqHeader.channelCount,0));

    uint32_t FileOffsetToCurrentHeaderRecord = read16 (fsqHeader.VariableHdrOffset);
    uint32_t FileOffsetToStartOfSequenceData = read16 (fsqHeader.dataOffset); // DataOffset

    // DEBUG_V (String ("FileOffsetToCurrentHeaderRecord: ") + String (FileOffsetToCurrentHeaderRecord));
    // DEBUG_V (String ("FileOffsetToStartOfSequenceData: ") + String (FileOffsetToStartOfSequenceData));

    if (FileOffsetToCurrentHeaderRecord < FileOffsetToStartOfSequenceData)
    {
        JsonArray  JsonDataHeaders = JsonData.createNestedArray (F ("variableHeaders"));

        char FSEQVariableDataHeaderBuffer[sizeof (FSEQRawVariableDataHeader) + 1];
        memset ((uint8_t*)FSEQVariableDataHeaderBuffer, 0x00, sizeof (FSEQVariableDataHeaderBuffer));
        FSEQRawVariableDataHeader* pCurrentVariableHeader = (FSEQRawVariableDataHeader*)FSEQVariableDataHeaderBuffer;

        while (FileOffsetToCurrentHeaderRecord < FileOffsetToStartOfSequenceData)
        {
            FileMgr.ReadSdFile (fseq, (byte*)FSEQVariableDataHeaderBuffer, sizeof (FSEQRawVariableDataHeader), FileOffsetToCurrentHeaderRecord);

            int VariableDataHeaderTotalLength = read16 ((uint8_t*)&(pCurrentVariableHeader->length));
            int VariableDataHeaderDataLength  = VariableDataHeaderTotalLength - sizeof (FSEQRawVariableDataHeader);

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

            FileOffsetToCurrentHeaderRecord += VariableDataHeaderTotalLength + sizeof (FSEQRawVariableDataHeader);
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

        if (path.startsWith (F ("/api/sequence/")) && AllowedToRemotePlayFiles())
        {
            // DEBUG_V ("");

            String seq = path.substring (14);
            if (seq.endsWith (F ("/meta")))
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
                logcon (String (F ("Could not open: ")) + seq);
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
        // DEBUG_V (String(F ("path: ")) + path);

        if (path != F ("uploadFile"))
        {
            // DEBUG_V ("");
            request->send (404);
            break;
        }

        String filename = request->getParam (CN_filename)->value ();
        // DEBUG_V (String(F ("FileName: ")) + filename);

        c_FileMgr::FileId FileHandle;
        if (false == FileMgr.OpenSdFile (filename, c_FileMgr::FileMode::FileRead, FileHandle))
        {
            logcon (String (F ("c_FPPDiscovery::ProcessPOST: File Does Not Exist - FileName: ")) + filename);
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
        // LOG_PORT.printf("len: %u / index: %u / total: %u\n", len, index, total);
        printReq (request, false);

        String path = request->getParam (ulrPath)->value ();
        if (path == F ("uploadFile"))
        {
            if (!request->hasParam (CN_filename))
            {
                // DEBUG_V ("Missing Filename Parameter");
            }
            else
            {
                StopPlaying ();
                inFileUpload = true;
                // DEBUG_V (String ("request: ") + String (uint32_t (request), HEX));
                UploadFileName = String (request->getParam (CN_filename)->value ());
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

    jsonResponse[CN_HostName]           = config.hostname;
    jsonResponse[F ("HostDescription")] = config.id;
    jsonResponse[CN_Platform]           = CN_ESPixelStick;
    jsonResponse[F ("Variant")]         = FPP_VARIANT_NAME;
    jsonResponse[F ("Mode")]            = (true == AllowedToRemotePlayFiles()) ? CN_remote : CN_bridge;
    jsonResponse[CN_Version]            = VERSION;

    const char* version = VERSION.c_str ();

    jsonResponse[F ("majorVersion")] = (uint16_t)atoi (version);
    jsonResponse[F ("minorVersion")] = (uint16_t)atoi (&version[2]);
    jsonResponse[F ("typeId")]       = FPP_TYPE_ID;

    JsonObject jsonResponseUtilization = jsonResponse.createNestedObject (F ("Utilization"));
    jsonResponseUtilization[F ("MemoryFree")] = ESP.getFreeHeap ();
    jsonResponseUtilization[F ("Uptime")]     = millis ();

    jsonResponse[CN_rssi] = WiFi.RSSI ();
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

        if (command == F ("getFPPstatus"))
        {
            String adv = CN_false;
            if (request->hasParam (CN_advancedView))
            {
                adv = request->getParam (CN_advancedView)->value ();
            }

            JsonObject JsonDataMqtt = JsonData.createNestedObject(F ("MQTT"));

            JsonDataMqtt[F ("configured")] = false;
            JsonDataMqtt[F ("connected")]  = false;

            JsonObject JsonDataCurrentPlaylist = JsonData.createNestedObject (F ("current_playlist"));

            JsonDataCurrentPlaylist[CN_count]          = "0";
            JsonDataCurrentPlaylist[F ("description")] = "";
            JsonDataCurrentPlaylist[F ("index")]       = "0";
            JsonDataCurrentPlaylist[CN_playlist]       = "";
            JsonDataCurrentPlaylist[CN_type]           = "";

            JsonData[F ("volume")]         = 70;
            JsonData[F ("media_filename")] = "";
            JsonData[F ("fppd")]           = F ("running");
            JsonData[F ("current_song")]   = "";

            if (false == PlayingFile())
            {
                JsonData[CN_current_sequence]  = "";
                JsonData[CN_playlist]          = "";
                JsonData[CN_seconds_elapsed]   = String (0);
                JsonData[CN_seconds_played]    = String (0);
                JsonData[CN_seconds_remaining] = String (0);
                JsonData[CN_sequence_filename] = "";
                JsonData[CN_time_elapsed]      = String("00:00");
                JsonData[CN_time_remaining]    = String ("00:00");

                JsonData[CN_status] = 0;
                JsonData[CN_status_name] = F ("idle");

                if (IsEnabled)
                {
                    JsonData[CN_mode] = 8;
                    JsonData[CN_mode_name] = CN_remote;
                }
                else
                {
                    JsonData[CN_mode] = 1;
                    JsonData[CN_mode_name] = CN_bridge;
                }
            }
            else
            {
                if (InputFPPRemotePlayFile)
                {
                    InputFPPRemotePlayFile->GetStatus (JsonData);
                }
                JsonData[CN_status] = 1;
                JsonData[CN_status_name] = F ("playing");

                JsonData[CN_mode] = 8;
                JsonData[CN_mode_name] = CN_remote;
            }

            if (adv == CN_true)
            {
                JsonObject JsonDataAdvancedView = JsonData.createNestedObject (F ("advancedView"));
                GetSysInfoJSON (JsonDataAdvancedView);
            }

            String Response;
            serializeJson (JsonDoc, Response);
            // DEBUG_V (String ("JsonDoc: ") + Response);
            request->send (200, F ("application/json"), Response);

            break;
        }

        if (command == F ("getSysInfo"))
        {
            GetSysInfoJSON (JsonData);

            String resp = "";
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("JsonDoc: ") + resp);
            request->send (200, F ("application/json"), resp);

            break;
        }

        if (command == F ("getHostNameInfo"))
        {
            JsonData[CN_HostName] = config.hostname;
            JsonData[F ("HostDescription")] = config.id;

            String resp;
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("resp: ") + resp);
            request->send (200, F ("application/json"), resp);

            break;
        }

        if (command == F ("getChannelOutputs"))
        {
            if (request->hasParam (CN_file))
            {
                String filename = request->getParam (CN_file)->value ();
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
void c_FPPDiscovery::StartPlaying (String & FileName, float SecondsElapsed)
{
    // DEBUG_START;
    // DEBUG_V (String("Open:: FileName: ") + FileName);

    do // once
    {
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

        if (0 == FileName.length())
        {
            // DEBUG_V("Do not have a file to start");
            break;
        }
        // DEBUG_V ("Asking for file to play");

        if (InputFPPRemotePlayFile)
        {
            InputFPPRemotePlayFile->Start (FileName, SecondsElapsed, 1);
        }

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
void c_FPPDiscovery::StopPlaying ()
{
    // DEBUG_START;

    // logcon (String (F ("FPPDiscovery::StopPlaying '")) + InputFPPRemotePlayFile.GetFileName() + "'");
    if (InputFPPRemotePlayFile)
    {
        InputFPPRemotePlayFile->Stop ();
    }

    // DEBUG_V ("");

    // ProcessBlankPacket ();

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
