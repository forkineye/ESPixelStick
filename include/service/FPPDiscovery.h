#pragma once
/*
* c_FPPDiscovery.h
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2018, 2025 Shelby Merrick
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

#include "ESPixelStick.h"
#include "input/InputMgr.hpp"
#include "input/InputFPPRemote.h"

#ifdef ESP32
#	include <WiFi.h>
#	include <AsyncUDP.h>
#elif defined (ESP8266)
#	include <ESPAsyncUDP.h>
#	include <ESP8266WiFi.h>
#	include <ESP8266WiFiMulti.h>
#else
#	error Platform not supported
#endif

#include <ESPAsyncWebServer.h>

class c_FPPDiscovery
{
private:

    AsyncUDP udp;
    void ProcessReceivedUdpPacket (AsyncUDPPacket _packet);
    void ProcessSyncPacket (uint8_t action, String filename, float seconds_elapsed);
    void ProcessBlankPacket ();
    bool PlayingFile ();

    bool inFileUpload = false;
    bool writeFailed = false;
    bool hasBeenInitialized = false;
    bool IsEnabled = false;
    bool BlankOnStop = false;
    bool StopInProgress = false;
    bool FppSyncOverride = false;
    String  ConfiguredFileToPlay;
    String UploadFileName;
    IPAddress FppRemoteIp = IPAddress (uint32_t(0));
    c_InputFPPRemote * InputFPPRemote = nullptr;
    const IPAddress MulticastAddress = IPAddress (239, 70, 80, 80);

    void GetStatusJSON           (JsonObject& jsonResponse, bool advanced);
    void BuildFseqResponse       (String fname, c_FileMgr::FileId fseq, String & resp);
    void StopPlaying             ();
    void StartPlaying            (String & FileName, float SecondsElapsed);
    bool AllowedToPlayRemoteFile ();
    void GetDriverName           (String & Name) { Name = "FPPD"; }

    struct MultiSyncStats_t
    {
        time_t   lastReceiveTime;
        uint32_t pktCommand;
        uint32_t pktLastCommand;
        uint32_t pktSyncSeqOpen;
        uint32_t pktSyncSeqStart;
        uint32_t pktSyncSeqStop;
        uint32_t pktSyncSeqSync;
        uint32_t pktSyncMedOpen;
        uint32_t pktSyncMedStart;
        uint32_t pktSyncMedStop;
        uint32_t pktSyncMedSync;
        uint32_t pktBlank;
        uint32_t pktPing;
        uint32_t pktPlugin;
        uint32_t pktFPPCommand;
        uint32_t pktHdrError;
        uint32_t pktUnknown;
    };
    MultiSyncStats_t MultiSyncStats;

#   define SYNC_FILE_SEQ        0
#   define SYNC_FILE_MEDIA      1

#   define CTRL_PKT_CMD         0 // deprecated in favor of FPP Commands
#   define CTRL_PKT_SYNC        1
#   define CTRL_PKT_EVENT       2 // deprecated in favor of FPP Commands
#   define CTRL_PKT_BLANK       3
#   define CTRL_PKT_PING        4
#   define CTRL_PKT_PLUGIN      5
#   define CTRL_PKT_FPPCOMMAND  6

#define FPP_DEBUG_ENABLED
struct SystemDebugStats_t
{
    uint32_t ProcessFPPJson = 0;
    uint32_t ProcessFPPDJson = 0;
    uint32_t CmdGetFPPstatus = 0;
    uint32_t CmdGetSysInfoJSON = 0;
    uint32_t CmdGetHostname = 0;
    uint32_t CmdGetConfig = 0;
    uint32_t CmdNotFound = 0;
};
SystemDebugStats_t SystemDebugStats;

public:
    c_FPPDiscovery ();
    virtual ~c_FPPDiscovery() {}

    void begin ();

    void ProcessFPPJson      (AsyncWebServerRequest* request);
    void ProcessFPPDJson     (AsyncWebServerRequest* request);
    void ProcessGET          (AsyncWebServerRequest* request);
    void ProcessPOST         (AsyncWebServerRequest* request);
    void ProcessFile         (AsyncWebServerRequest* request, String filename, uint32_t index, uint8_t* data, uint32_t len, bool final, uint32_t contentLength = 0);
    void ProcessBody         (AsyncWebServerRequest* request, uint8_t* data, uint32_t len, uint32_t index, uint32_t total);
    void sendPingPacket      (IPAddress destination = IPAddress(255, 255, 255, 255));
    void PlayFile            (String & FileToPlay);
    void Enable              (void);
    void Disable             (void);
    void GetStatus           (JsonObject& jsonStatus);
    void NetworkStateChanged (bool NewNetworkState);
    void SetOperationalState (bool ActiveFlag);
    bool SetConfig           (JsonObject& jsonConfig);
    void GetConfig           (JsonObject& jsonConfig);

    void SetInputFPPRemotePlayFile    (c_InputFPPRemote * value);
    void ForgetInputFPPRemotePlayFile ();
    void GenerateFppSyncMsg           (uint8_t Action, const String & FileName, uint32_t CurrentFrame, const float & ElpsedTime);
    void GetSysInfoJSON               (JsonObject& jsonResponse);
    bool PlayingAfile                 () {return PlayingFile();}

#   define SYNC_PKT_START       0
#   define SYNC_PKT_STOP        1
#   define SYNC_PKT_SYNC        2
#   define SYNC_PKT_OPEN        3

};

extern c_FPPDiscovery FPPDiscovery;
