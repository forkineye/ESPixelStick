/*
* c_FPPDiscovery.cpp

* Copyright (c) 2021, 2025 Shelby Merrick
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
#include "service/FPPDiscovery.h"
#include "service/fseq.h"
#include "FileMgr.hpp"
#include "output/OutputMgr.hpp"
#include "network/NetworkMgr.hpp"
#include <Int64String.h>
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

    // StopPlaying ();
    // DEBUG_V();

    inFileUpload = false;
    hasBeenInitialized = true;

    NetworkStateChanged (NetworkMgr.IsConnected ());

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

        // Try to listen to the broadcast port
        bool Failed = true;
        if (!udp.listen (FPP_DISCOVERY_PORT))
        {
            logcon (String (F ("FAILED to subscribed to broadcast messages")));
        }
        else
        {
            Failed = false;
            logcon (String (F ("FPPDiscovery subscribed to broadcast messages on port: ")) + String(FPP_DISCOVERY_PORT));
        }

        if (!udp.listenMulticast (MulticastAddress, FPP_DISCOVERY_PORT))
        {
            logcon (String (F ("FAILED to subscribed to multicast messages")));
        }
        else
        {
            Failed = false;
            logcon (String (F ("FPPDiscovery subscribed to multicast: ")) + MulticastAddress.toString () + F(":") + String(FPP_DISCOVERY_PORT));
        }

        // did at least one binding work?
        if(Failed)
        {
            // no active bindings
            break;
        }

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
void c_FPPDiscovery::SetOperationalState (bool ActiveFlag)
{
    // DEBUG_START;

    // Do NOT call enable / disable since they call other functions.
    IsEnabled = ActiveFlag;

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    JsonWrite(jsonConfig, CN_fseqfilename, ConfiguredFileToPlay);
    JsonWrite(jsonConfig, CN_BlankOnStop,  BlankOnStop);
    JsonWrite(jsonConfig, CN_FPPoverride,  FppSyncOverride);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    if (IsEnabled)
    {
#ifdef FPP_DEBUG_ENABLED
        JsonWrite(jsonStatus, F ("pktCommand"),      MultiSyncStats.pktCommand);
        JsonWrite(jsonStatus, F ("pktSyncSeqOpen"),  MultiSyncStats.pktSyncSeqOpen);
        JsonWrite(jsonStatus, F ("pktSyncSeqStart"), MultiSyncStats.pktSyncSeqStart);
        JsonWrite(jsonStatus, F ("pktSyncSeqStop"),  MultiSyncStats.pktSyncSeqStop);
        JsonWrite(jsonStatus, F ("pktSyncSeqSync"),  MultiSyncStats.pktSyncSeqSync);
        JsonWrite(jsonStatus, F ("pktSyncMedOpen"),  MultiSyncStats.pktSyncMedOpen);
        JsonWrite(jsonStatus, F ("pktSyncMedStart"), MultiSyncStats.pktSyncMedStart);
        JsonWrite(jsonStatus, F ("pktSyncMedStop"),  MultiSyncStats.pktSyncMedStop);
        JsonWrite(jsonStatus, F ("pktSyncMedSync"),  MultiSyncStats.pktSyncMedSync);
        JsonWrite(jsonStatus, F ("pktBlank"),        MultiSyncStats.pktBlank);
        JsonWrite(jsonStatus, F ("pktPing"),         MultiSyncStats.pktPing);
        JsonWrite(jsonStatus, F ("pktPlugin"),       MultiSyncStats.pktPlugin);
        JsonWrite(jsonStatus, F ("pktFPPCommand"),   MultiSyncStats.pktFPPCommand);
        JsonWrite(jsonStatus, F ("pktHdrError"),     MultiSyncStats.pktHdrError);
        JsonWrite(jsonStatus, F ("pktUnknown"),      MultiSyncStats.pktUnknown);
        JsonWrite(jsonStatus, F ("pktLastCommand"),  MultiSyncStats.pktLastCommand);

        JsonWrite(jsonStatus, F ("ProcessFPPJson"),    SystemDebugStats.ProcessFPPJson);
        JsonWrite(jsonStatus, F ("ProcessFPPDJson"),   SystemDebugStats.ProcessFPPDJson);
        JsonWrite(jsonStatus, F ("CmdGetFPPstatus"),   SystemDebugStats.CmdGetFPPstatus);
        JsonWrite(jsonStatus, F ("CmdGetSysInfoJSON"), SystemDebugStats.CmdGetSysInfoJSON);
        JsonWrite(jsonStatus, F ("CmdGetHostname"),    SystemDebugStats.CmdGetHostname);
        JsonWrite(jsonStatus, F ("CmdGetConfig"),      SystemDebugStats.CmdGetConfig);
        JsonWrite(jsonStatus, F ("CmdNotFound"),       SystemDebugStats.CmdNotFound);
#endif // def FPP_DEBUG_ENABLED

        // DEBUG_V ("Is Enabled");
        JsonObject MyJsonStatus = jsonStatus[F ("FPPDiscovery")].to<JsonObject> ();
        JsonWrite(MyJsonStatus, F ("FppRemoteIp"), FppRemoteIp.toString ());
        if (AllowedToPlayRemoteFile())
        {
            InputFPPRemote->GetFppRemotePlayStatus (MyJsonStatus);
        }
    }
    else
    {
        // DEBUG_V ("Not Enabled");
    }

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessReceivedUdpPacket (AsyncUDPPacket UDPpacket)
{
    // DEBUG_START;
    do // once
    {
        // We're in an upload, can't be bothered
        if (inFileUpload)
        {
            // DEBUG_V ("In Upload, Ignore message");
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
            // DEBUG_V ("Invalid FPP header");
            MultiSyncStats.pktHdrError++;
            break;
        }
        // DEBUG_V ();

        struct timeval tv;
        gettimeofday (&tv, NULL);
        MultiSyncStats.lastReceiveTime = tv.tv_sec;
        MultiSyncStats.pktLastCommand = fppPacket->packet_type;

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
                MultiSyncStats.pktUnknown++;

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
        if (!AllowedToPlayRemoteFile ())
        {
            // DEBUG_V ("Not allowed to play remote files");
            break;
        }

        // DEBUG_V (String("action: ") + String(action));

        switch (action)
        {
            case SYNC_PKT_START:
            {
                // DEBUG_V ("Sync::Start");
                // DEBUG_V (String ("      FileName: ") + FileName);
                // DEBUG_V (String ("SecondsElapsed: ") + SecondsElapsed);

                MultiSyncStats.pktSyncSeqStart++;
                StartPlaying (FileName, SecondsElapsed);
                break;
            }

            case SYNC_PKT_STOP:
            {
                // DEBUG_V ("Sync::Stop");
                // DEBUG_V (String ("      FileName: ") + FileName);
                // DEBUG_V (String ("SecondsElapsed: ") + SecondsElapsed);

                MultiSyncStats.pktSyncSeqStop++;
                if(BlankOnStop)
                {
                    ProcessBlankPacket();
                }
                else
                {
                    StopPlaying ();
                }

                if(FppSyncOverride)
                {
                    InputFPPRemote->SetBackgroundFile();
                }
                break;
            }

            case SYNC_PKT_SYNC:
            {
                // DEBUG_V ("Sync");
                // DEBUG_V (String ("   PlayingFile: ") + PlayingFile ());
                // DEBUG_V (String ("      FileName: ") + FileName);
                // DEBUG_V (String ("     IsEnabled: ") + IsEnabled);
                // DEBUG_V (String ("   GetFileName: ") + InputFPPRemote.GetFileName ());
                // DEBUG_V (String ("SecondsElapsed: ") + SecondsElapsed);

                /*
                logcon (String(float(millis()/1000.0)) + "," +
                                  String(InputFPPRemote.GetLastFrameId()) + "," +
                                  String (seconds_elapsed) + "," +
                                  String (FrameId) + "," +
                                  String(InputFPPRemote.GetTimeOffset(),5));
                */
                MultiSyncStats.pktSyncSeqSync++;
                // DEBUG_V (String ("SecondsElapsed: ") + String (SecondsElapsed));
                if (AllowedToPlayRemoteFile())
                {
                    InputFPPRemote->FppSyncRemoteFilePlay (FileName, SecondsElapsed);
                }
                break;
            }

            case SYNC_PKT_OPEN:
            {
                // DEBUG_V ("Sync::Open");
                // DEBUG_V (String ("      FileName: ") + FileName);
                // DEBUG_V (String ("SecondsElapsed: ") + SecondsElapsed);
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
        StopPlaying ();
        OutputMgr.ClearBuffer();
    }
    // DEBUG_END;
} // ProcessBlankPacket

//-----------------------------------------------------------------------------
bool c_FPPDiscovery::PlayingFile ()
{
    bool Response = false;
    if (AllowedToPlayRemoteFile())
    {
        Response = !InputFPPRemote->IsIdle ();
    }
    return Response;
} // PlayingFile

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
    String Hostname;
    NetworkMgr.GetHostname (Hostname);
    strcpy (packet.hostName, Hostname.c_str ());
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
        const AsyncWebParameter* p = request->getParam (i);
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

    JsonDocument JsonDoc;
    JsonObject JsonData = JsonDoc.to<JsonObject> ();

    // DEBUG_V(String("FileHandle: ") + String(fseq));

    FSEQRawHeader fsqHeader;
    FileMgr.ReadSdFile (fseq, (byte*)&fsqHeader, sizeof (fsqHeader), size_t(0));

    JsonWrite(JsonData, F ("Name"),            fname);
    JsonWrite(JsonData, CN_Version,            String (fsqHeader.majorVersion) + "." + String (fsqHeader.minorVersion));
    JsonWrite(JsonData, F ("ID"),              int64String (read64 (fsqHeader.id, 0)));
    JsonWrite(JsonData, F ("StepTime"),        String (fsqHeader.stepTime));
    JsonWrite(JsonData, F ("NumFrames"),       String (read32 (fsqHeader.TotalNumberOfFramesInSequence, 0)));
    JsonWrite(JsonData, F ("CompressionType"), fsqHeader.compressionType);

    static const int TIME_STR_CHAR_COUNT = 32;
    char timeStr[TIME_STR_CHAR_COUNT];
    struct tm tm = *gmtime (&MultiSyncStats.lastReceiveTime);
    // BUGBUG -- trusting the provided `tm` structure values contain valid data ... use `snprintf` to mitigate.
    int actuallyWritten = snprintf (timeStr, TIME_STR_CHAR_COUNT,
        "%4d-%.2d-%.2d %.2d:%.2d:%.2d",
        1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    // TODO: assert ((actuallyWritten > 0) && (actuallyWritten < TIME_STR_CHAR_COUNT))
    if ((actuallyWritten > 0) && (actuallyWritten < TIME_STR_CHAR_COUNT)) {
        JsonWrite(JsonData, F ("lastReceiveTime"), timeStr);
    }

    JsonWrite(JsonData, F ("pktCommand"),      MultiSyncStats.pktCommand);
    JsonWrite(JsonData, F ("pktSyncSeqOpen"),  MultiSyncStats.pktSyncSeqOpen);
    JsonWrite(JsonData, F ("pktSyncSeqStart"), MultiSyncStats.pktSyncSeqStart);
    JsonWrite(JsonData, F ("pktSyncSeqStop"),  MultiSyncStats.pktSyncSeqStop);
    JsonWrite(JsonData, F ("pktSyncSeqSync"),  MultiSyncStats.pktSyncSeqSync);
    JsonWrite(JsonData, F ("pktSyncMedOpen"),  MultiSyncStats.pktSyncMedOpen);
    JsonWrite(JsonData, F ("pktSyncMedStart"), MultiSyncStats.pktSyncMedStart);
    JsonWrite(JsonData, F ("pktSyncMedStop"),  MultiSyncStats.pktSyncMedStop);
    JsonWrite(JsonData, F ("pktSyncMedSync"),  MultiSyncStats.pktSyncMedSync);
    JsonWrite(JsonData, F ("pktBlank"),        MultiSyncStats.pktBlank);
    JsonWrite(JsonData, F ("pktPing"),         MultiSyncStats.pktPing);
    JsonWrite(JsonData, F ("pktPlugin"),       MultiSyncStats.pktPlugin);
    JsonWrite(JsonData, F ("pktFPPCommand"),   MultiSyncStats.pktFPPCommand);
    JsonWrite(JsonData, F ("pktError"),        MultiSyncStats.pktHdrError);

    uint32_t maxChannel = read32 (fsqHeader.channelCount, 0);

    if (0 != fsqHeader.numSparseRanges)
    {
        JsonArray  JsonDataRanges = JsonData[F ("Ranges")].to<JsonArray> ();

        maxChannel = 0;

        uint8_t* RangeDataBuffer = (uint8_t*)malloc (sizeof(FSEQRawRangeEntry) * fsqHeader.numSparseRanges);
        FSEQRawRangeEntry* CurrentFSEQRangeEntry = (FSEQRawRangeEntry*)RangeDataBuffer;

        FileMgr.ReadSdFile (fseq, RangeDataBuffer, sizeof (FSEQRawRangeEntry), size_t(fsqHeader.numCompressedBlocks * 8 + 32));

        for (int CurrentRangeIndex = 0;
             CurrentRangeIndex < fsqHeader.numSparseRanges;
             CurrentRangeIndex++, CurrentFSEQRangeEntry++)
        {
            uint32_t RangeStart  = read24 (CurrentFSEQRangeEntry->Start);
            uint32_t RangeLength = read24 (CurrentFSEQRangeEntry->Length);

            JsonObject JsonRange = JsonDataRanges.add<JsonObject> ();
            JsonWrite(JsonRange, F ("Start"),  String (RangeStart));
            JsonWrite(JsonRange, F ("Length"), String (RangeLength));

            if ((RangeStart + RangeLength - 1) > maxChannel)
            {
                maxChannel = RangeStart + RangeLength - 1;
            }
        }

        free (RangeDataBuffer);
    }

    JsonWrite(JsonData, F ("MaxChannel"),   String (maxChannel));
    JsonWrite(JsonData, F ("ChannelCount"), String (read32 (fsqHeader.channelCount,0)));

    uint32_t FileOffsetToCurrentHeaderRecord = read16 (fsqHeader.VariableHdrOffset);
    uint32_t FileOffsetToStartOfSequenceData = read16 (fsqHeader.dataOffset); // DataOffset

    // DEBUG_V (String ("FileOffsetToCurrentHeaderRecord: ") + String (FileOffsetToCurrentHeaderRecord));
    // DEBUG_V (String ("FileOffsetToStartOfSequenceData: ") + String (FileOffsetToStartOfSequenceData));

    if (FileOffsetToCurrentHeaderRecord < FileOffsetToStartOfSequenceData)
    {
        JsonArray  JsonDataHeaders = JsonData[F ("variableHeaders")].to<JsonArray> ();

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

                JsonObject JsonDataHeader = JsonDataHeaders.add<JsonObject> ();
                JsonWrite(JsonDataHeader, HeaderTypeCode.c_str(), String (VariableDataHeaderDataBuffer));

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
        String path = request->url();
        if (path.startsWith(F("/fpp/")))
        {
            path = path.substring(4);
        }

        if (request->hasParam (ulrPath))
        {
            path = request->getParam (ulrPath)->value ();
        }

        // DEBUG_V (String ("Path: ") + path);

        if (path.startsWith (F ("/api/sequence/")) && AllowedToPlayRemoteFile())
        {
            // DEBUG_V (emptyString);

            String seq = path.substring (14);
            if (seq.endsWith (F ("/meta")))
            {
                // DEBUG_V (emptyString);
                seq = seq.substring (0, seq.length () - 5);
                // DEBUG_V("Stop Input");
                StopPlaying ();

                c_FileMgr::FileId FileHandle;
                // DEBUG_V (String (" seq: ") + seq);

                if (FileMgr.OpenSdFile (seq, c_FileMgr::FileMode::FileRead, FileHandle, -1))
                {
                    if (FileMgr.GetSdFileSize(FileHandle) > 0)
                    {
                        // DEBUG_V ("found the file. return metadata as json");
                        String resp = emptyString;
                        BuildFseqResponse (seq, FileHandle, resp);
                        FileMgr.CloseSdFile (FileHandle);
                        request->send (200, CN_applicationSLASHjson, resp);
                        break;
                    }
                }
                logcon (String (F ("Could not open: '")) + seq + F("' for reading"));
            }
        }
        else if (path.startsWith (F ("/api/system/status")))
        {
            String Response;
            JsonDocument JsonDoc;
            JsonObject JsonData = JsonDoc.to<JsonObject> ();
            GetStatusJSON(JsonData, true);
            serializeJson (JsonDoc, Response);
            // DEBUG_V (String ("JsonDoc: ") + Response);
            request->send (200, CN_applicationSLASHjson, Response);
            break;
        }
        else if (path.startsWith (F ("/api/system/info")))
        {
            String Response;
            JsonDocument JsonDoc;
            JsonObject JsonData = JsonDoc.to<JsonObject> ();
            GetSysInfoJSON(JsonData);
            serializeJson (JsonDoc, Response);
            // DEBUG_V (String ("JsonDoc: ") + Response);
            request->send (200, CN_applicationSLASHjson, Response);
            break;
        }
        // DEBUG_V(String("Unknown Request: '") + path + "'");
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
            // DEBUG_V ("Not a valid path");
            request->send (404);
            break;
        }

        String filename = request->getParam (CN_filename)->value ();
        // DEBUG_V (String(F ("FileName: ")) + filename);

        c_FileMgr::FileId FileHandle;
        if (false == FileMgr.OpenSdFile (filename, c_FileMgr::FileMode::FileRead, FileHandle, -1))
        {
            logcon (String (F ("c_FPPDiscovery::ProcessPOST: File Does Not Exist - FileName: ")) + filename);
            request->send (404);
            break;
        }

        // DEBUG_V ("BuildFseqResponse");
        String resp = emptyString;
        BuildFseqResponse (filename, FileHandle, resp);
        FileMgr.CloseSdFile (FileHandle);
        request->send (200, CN_applicationSLASHjson, resp);

    } while (false);

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFile (
    AsyncWebServerRequest* request,
    String filename,
    uint32_t index,
    uint8_t* data,
    uint32_t len,
    bool final,
    uint32_t ContentLength)
{
    // DEBUG_START;

    printReq(request, false);

    do // once
    {
        if (false == FileMgr.SdCardIsInstalled())
        {
            request->send (404);
            break;
        }

        uint32_t ContentLength = 0;
        if(request->hasHeader(F("Content-Length")))
        {
            ContentLength = atoi (request->getHeader(F("Content-Length"))->value().c_str());
        }

        // DEBUG_V (String ("         name: ") + filename);
        // DEBUG_V (String ("        index: ") + String (index));
        // DEBUG_V (String ("          len: ") + String (len));
        // DEBUG_V (String ("        final: ") + String (final));
        // DEBUG_V (String ("ContentLength: ") + String (ContentLength));

        if(!inFileUpload)
        {
            if(0 != index)
            {
                // DEBUG_V("Ignore non start file request. Index = " + String(index) + " Filename: '" + filename + "'");
                request->send (500);
                break;
            }
            // DEBUG_V (String ("         name: ") + filename);
            // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));
            // DEBUG_V("wait for the player to become idle");
            // DEBUG_V (String ("ContentLength: ") + String (ContentLength));
            // DEBUG_V("Stop Input");
            StopPlaying();
            inFileUpload = true;
            UploadFileName = filename;
            InputMgr.SetOperationalState(false);
            OutputMgr.PauseOutputs(true);
        }
        else if(!UploadFileName.equals(filename))
        {
            if(0 != index)
            {
                // DEBUG_V("Ignore unexpected file fragment for '" + filename + "'");
                request->send (500);
                break;
            }

            // DEBUG_V("New file starting. Aborting: '" + UploadFileName + "' and starting '" + filename + "'");
            FileMgr.AbortSdFileUpload();
            UploadFileName = filename;
        }

        // DEBUG_V("Write the file block");
        writeFailed = !FileMgr.handleFileUpload (UploadFileName, index, data, len, final, ContentLength);

        if(writeFailed)
        {
            // DEBUG_V("WriteFailed");
            request->send (500);
        }

        // DEBUG_V();
        if (final || writeFailed)
        {
            inFileUpload = false;
            UploadFileName = emptyString;
            writeFailed = false;
            memset(OutputMgr.GetBufferAddress(), 0x00, OutputMgr.GetBufferSize());
            InputMgr.SetOperationalState(true);
            OutputMgr.PauseOutputs(false);
            FeedWDT();
            delay(1000);
            FeedWDT();
            // DEBUG_V("Allow file to play");
            StartPlaying(UploadFileName, 0.0);
        }

    } while (false);

    // DEBUG_END;

} // ProcessFile

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessBody (
    AsyncWebServerRequest* request,
    uint8_t* data,
    uint32_t len,
    uint32_t index,
    uint32_t total)
{
    // DEBUG_START;
    printReq (request, false);

    // DEBUG_V(String("  len: ") + String(len));
    // DEBUG_V(String("index: ") + String(index));
    // DEBUG_V(String("total: ") + String(total));

    do // once
    {
        // is this the first packet?
        if (0 == index)
        {
            // DEBUG_V("New Download");

            if(!request->hasParam(ulrPath))
            {
                // DEBUG_V("Missing URL Path param");
                request->send (404);
                break;
            }

            String path = request->getParam (ulrPath)->value ();
            if (path == F ("uploadFile"))
            {
                if (!request->hasParam (CN_filename))
                {
                    // DEBUG_V ("Missing Filename Parameter");
                    request->send (404);
                    break;
                }
                UploadFileName = String (request->getParam (CN_filename)->value ());
                // DEBUG_V (emptyString);
            }

            writeFailed = false;

            // DEBUG_V (String ("         name: ") + UploadFileName);
            // DEBUG_V (String ("        index: ") + String (index));
            // DEBUG_V (String ("          len: ") + String (len));
            // DEBUG_V (String ("        total: ") + String (total));

        } // end 0 == index

        ProcessFile (request, UploadFileName, index, data, len, (total <= (index + len)), 0);

    } while (false);

    // DEBUG_END;
} // ProcessBody

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetSysInfoJSON (JsonObject & jsonResponse)
{
    // DEBUG_START;
    String Hostname;
    NetworkMgr.GetHostname (Hostname);

    JsonWrite(jsonResponse, CN_HostName,           Hostname);
    JsonWrite(jsonResponse, F ("HostDescription"), config.id);
    JsonWrite(jsonResponse, CN_Platform,           String(CN_ESPixelStick));
    JsonWrite(jsonResponse, F ("Variant"),         FPP_VARIANT_NAME);
    JsonWrite(jsonResponse, F ("Mode"),            String((true == AllowedToPlayRemoteFile()) ? CN_remote : CN_bridge));
    JsonWrite(jsonResponse, CN_Version,            VERSION);

    const char* version = VERSION.c_str ();

    JsonWrite(jsonResponse, F ("majorVersion"), (uint16_t)atoi (version));
    JsonWrite(jsonResponse, F ("minorVersion"), (uint16_t)atoi (&version[2]));
    JsonWrite(jsonResponse, F ("typeId"),       FPP_TYPE_ID);
#ifdef SUPPORT_UNZIP
    JsonWrite(jsonResponse, F ("zip"),          true);
#else
    JsonWrite(jsonResponse, F ("zip"),          false);
#endif // def SUPPORT_UNZIP

    JsonObject jsonResponseUtilization = jsonResponse[F ("Utilization")].to<JsonObject> ();
    JsonWrite(jsonResponseUtilization, F ("MemoryFree"), ESP.getFreeHeap ());
    JsonWrite(jsonResponseUtilization, F ("Uptime"),     millis ());

    JsonWrite(jsonResponse, CN_rssi, WiFi.RSSI ());
    JsonArray jsonResponseIpAddresses = jsonResponse[F ("IPS")].to<JsonArray> ();
    jsonResponseIpAddresses.add(WiFi.localIP ().toString ());

    // DEBUG_END;

} // GetSysInfoJSON

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GetStatusJSON (JsonObject & JsonData, bool adv)
{
    // DEBUG_START;
    JsonObject JsonDataMqtt = JsonData[F ("MQTT")].to<JsonObject>();

    JsonWrite(JsonDataMqtt, F ("configured"), false);
    JsonWrite(JsonDataMqtt, F ("connected"),  false);

    JsonObject JsonDataCurrentPlaylist = JsonData[F ("current_playlist")].to<JsonObject> ();

    JsonWrite(JsonDataCurrentPlaylist, CN_count,          "0");
    JsonWrite(JsonDataCurrentPlaylist, F ("description"), emptyString);
    JsonWrite(JsonDataCurrentPlaylist, F ("index"),       "0");
    JsonWrite(JsonDataCurrentPlaylist, CN_playlist,       emptyString);
    JsonWrite(JsonDataCurrentPlaylist, CN_type,           emptyString);

    // DEBUG_V();

    JsonWrite(JsonData, F ("volume"),         70);
    JsonWrite(JsonData, F ("media_filename"), emptyString);
    JsonWrite(JsonData, F ("fppd"),           F ("running"));
    JsonWrite(JsonData, F ("current_song"),   emptyString);

    if (false == PlayingFile())
    {
        JsonWrite(JsonData, CN_current_sequence,   emptyString);
        JsonWrite(JsonData, CN_playlist,           emptyString);
        JsonWrite(JsonData, CN_seconds_elapsed,    String (0));
        JsonWrite(JsonData, CN_seconds_played,     String (0));
        JsonWrite(JsonData, CN_seconds_remaining,  String (0));
        JsonWrite(JsonData, CN_sequence_filename,  emptyString);
        JsonWrite(JsonData, CN_time_elapsed,       String("00:00"));
        JsonWrite(JsonData, CN_time_remaining,     String ("00:00"));
        JsonWrite(JsonData, CN_status,             0);
        JsonWrite(JsonData, CN_status_name,        F ("idle"));

        if (IsEnabled)
        {
            JsonWrite(JsonData, CN_mode,      8);
            JsonWrite(JsonData, CN_mode_name, String(CN_remote));
        }
        else
        {
            JsonWrite(JsonData, CN_mode,      1);
            JsonWrite(JsonData, CN_mode_name, String(CN_bridge));
        }
    }
    else
    {
        // DEBUG_V();
        if (AllowedToPlayRemoteFile())
        {
            // DEBUG_V();
            InputFPPRemote->GetStatus (JsonData);
        }
        JsonWrite(JsonData, CN_status,      1);
        JsonWrite(JsonData, CN_status_name, F ("playing"));

        JsonWrite(JsonData, CN_mode,      8);
        JsonWrite(JsonData, CN_mode_name, String(CN_remote));
    }
    // DEBUG_V();

    if (adv)
    {
        // DEBUG_V();
        JsonObject JsonDataAdvancedView = JsonData[F ("advancedView")].to<JsonObject> ();
        GetSysInfoJSON (JsonDataAdvancedView);
        // DEBUG_V();
    }
    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFPPJson (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, false);

    SystemDebugStats.ProcessFPPJson++;

    do // once
    {
        if (!request->hasParam (ulrCommand))
        {
            request->send (404);
            // DEBUG_V (String ("Missing Param: 'command' "));

            break;
        }

        JsonDocument JsonDoc;
        JsonObject JsonData = JsonDoc.to<JsonObject> ();

        String command = request->getParam (ulrCommand)->value ();
        // DEBUG_V (String ("command: ") + command);

        if (command.equals(F ("getFPPstatus")))
        {
            SystemDebugStats.CmdGetFPPstatus++;
            String adv = CN_false;
            if (request->hasParam (CN_advancedView))
            {
                adv = request->getParam (CN_advancedView)->value ();
            }

            GetStatusJSON(JsonData, adv.equals(CN_true));

            String Response;
            serializeJson (JsonDoc, Response);
            // DEBUG_V (String ("JsonDoc: ") + Response);
            request->send (200, CN_applicationSLASHjson, Response);

            break;
        }

        if (command.equals(F ("getSysInfo")))
        {
            SystemDebugStats.CmdGetSysInfoJSON++;
            GetSysInfoJSON (JsonData);

            String resp = emptyString;
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("JsonDoc: ") + resp);
            request->send (200, CN_applicationSLASHjson, resp);

            break;
        }

        if (command.equals(("getHostNameInfo")))
        {
            SystemDebugStats.CmdGetHostname++;
            String Hostname;
            NetworkMgr.GetHostname (Hostname);

            JsonWrite(JsonData, CN_HostName,           Hostname);
            JsonWrite(JsonData, F ("HostDescription"), config.id);

            String resp;
            serializeJson (JsonData, resp);
            // DEBUG_V (String ("resp: ") + resp);
            request->send (200, CN_applicationSLASHjson, resp);

            break;
        }

        if (command.equals(F ("getChannelOutputs")))
        {
            if (request->hasParam (CN_file))
            {
                String filename = request->getParam (CN_file)->value ();
                if (String(F ("co-other")).equals(filename))
                {
                    SystemDebugStats.CmdGetConfig++;
                    String resp;
                    OutputMgr.GetConfig (resp);

                    // DEBUG_V (String ("resp: ") + resp);
                    request->send (200, CN_applicationSLASHjson, resp);

                    break;
                }
            }
        }

        // DEBUG_V (String ("Unknown command: ") + command);
        request->send (404);
        SystemDebugStats.CmdNotFound++;

    } while (false);

    // DEBUG_END;

} // ProcessFPPJson

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ProcessFPPDJson (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printReq(request, false);

    SystemDebugStats.ProcessFPPDJson++;

    do // once
    {
        if (!request->hasParam (ulrCommand))
        {
            request->send (404);
            // DEBUG_V (String ("Missing Param: 'command' "));

            break;
        }

        // DEBUG_V (String ("Unknown command: ") + command);
        request->send (404);
        SystemDebugStats.CmdNotFound++;

    } while (false);

    // DEBUG_END;

} // ProcessFPPDJson

//-----------------------------------------------------------------------------
bool c_FPPDiscovery::SetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    setFromJSON (ConfiguredFileToPlay, jsonConfig, CN_fseqfilename);
    setFromJSON (BlankOnStop,          jsonConfig, CN_BlankOnStop);
    setFromJSON (FppSyncOverride,      jsonConfig, CN_FPPoverride);

    // DEBUG_END;

    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_FPPDiscovery::StartPlaying (String & FileName, float SecondsElapsed)
{
    // DEBUG_START;
    // DEBUG_V (String("Open:: FileName: ") + FileName);
    // DEBUG_V (String ("IsEnabled '") + String(IsEnabled));

    do // once
    {
        if (!IsEnabled)
        {
            // DEBUG_V ("Not Enabled");
            break;
        }
        // DEBUG_V (emptyString);

        if (inFileUpload)
        {
            // DEBUG_V ("Uploading");
            break;
        }
        // DEBUG_V (emptyString);

        if (FileName.isEmpty())
        {
            // DEBUG_V("Do not have a file to start");
            break;
        }
        // DEBUG_V ("Asking for file to play");

        if (AllowedToPlayRemoteFile())
        {
            // DEBUG_V ("Ask FSM to start playing");
            InputFPPRemote->FppStartRemoteFilePlay (FileName, SecondsElapsed);
        }

    } while (false);

    // DEBUG_END;

} // StartPlaying

//-----------------------------------------------------------------------------
// Wait for idle.
void c_FPPDiscovery::StopPlaying ()
{
    // DEBUG_START;
    // DEBUG_V (String ("Current Task Priority: ") + String(uxTaskPriorityGet(NULL)));
    // DEBUG_V (String ("FPPDiscovery::StopPlaying '") + InputFPPRemote->GetFileName() + "'");
    // DEBUG_V (String ("IsEnabled '") + String(IsEnabled));

    if(AllowedToPlayRemoteFile())
    {
        InputFPPRemote->FppStopRemoteFilePlay();
    }

    // DEBUG_END;

} // StopPlaying

//-----------------------------------------------------------------------------
void c_FPPDiscovery::GenerateFppSyncMsg(uint8_t Action, const String & FileName, uint32_t CurrentFrame, const float & ElpsedTime)
{
    // xDEBUG_START;

    do // once
    {
        if((Action == SYNC_PKT_SYNC) && (CurrentFrame & 0x7))
        {
            // only send sync every 8th frame
            break;
        }

        FPPMultiSyncPacket SyncPacket;

        SyncPacket.header[0] = 'F';
        SyncPacket.header[1] = 'P';
        SyncPacket.header[2] = 'P';
        SyncPacket.header[3] = 'D';
        SyncPacket.packet_type = CTRL_PKT_SYNC;
        write16 ((uint8_t*)&SyncPacket.data_len, sizeof(SyncPacket));

        SyncPacket.sync_action = Action;
        SyncPacket.sync_type = SYNC_FILE_SEQ;
        write32((uint8_t*)&SyncPacket.frame_number, CurrentFrame);
        SyncPacket.seconds_elapsed = ElpsedTime;

        // copy the file name and make sure a truncated file name has a proper line termination.
        strncpy(SyncPacket.filename, FileName.c_str(), size_t(sizeof(SyncPacket.filename)-1));
        SyncPacket.filename[sizeof(SyncPacket.filename)-1] = 0x00;

        if(NetworkMgr.IsConnected())
        {
            udp.writeTo (SyncPacket.raw, sizeof (SyncPacket), IPAddress(255,255,255,255), FPP_DISCOVERY_PORT);
            udp.writeTo (SyncPacket.raw, sizeof (SyncPacket), MulticastAddress, FPP_DISCOVERY_PORT);
        }
    } while(false);


    // xDEBUG_END;
} // GenerateFppSyncMsg

//-----------------------------------------------------------------------------
void c_FPPDiscovery::SetInputFPPRemotePlayFile (c_InputFPPRemote * value)
{
    // DEBUG_START;

    InputFPPRemote = value;

    // DEBUG_END;

} // SetInputFPPRemotePlayFile

//-----------------------------------------------------------------------------
void c_FPPDiscovery::ForgetInputFPPRemotePlayFile ()
{
    // DEBUG_START;

    InputFPPRemote = nullptr;

    // DEBUG_END;

} // SetInputFPPRemotePlayFile

//-----------------------------------------------------------------------------
bool c_FPPDiscovery::AllowedToPlayRemoteFile()
{
    // DEBUG_START;

    bool Response = false;

    if(InputFPPRemote &&
       FileMgr.SdCardIsInstalled() &&
       IsEnabled)
    {
        Response = InputFPPRemote->AllowedToPlayRemoteFile();
    }

    // DEBUG_END;
    return Response;
} // AllowedToPlayRemoteFile

c_FPPDiscovery FPPDiscovery;
