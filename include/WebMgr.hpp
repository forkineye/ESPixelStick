#pragma once
/*
* WebMgr.hpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2024 Shelby Merrick
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
#include "EFUpdate.h"
#include "output/OutputMgr.hpp"
#include <ESPAsyncWebServer.h>
#include <EspalexaDevice.h>


class c_WebMgr
{
public:
    c_WebMgr ();
    virtual ~c_WebMgr ();

    void Begin           (config_t * NewConfig); ///< set up the operating environment based on the current config (or defaults)
    void ValidateConfig  (config_t * NewConfig);
    void Process         ();

    void onAlexaMessage        (EspalexaDevice * pDevice);
    void RegisterAlexaCallback (DeviceCallbackFunction cb);
    bool IsAlexaCallbackValid  () { return (nullptr != pAlexaCallback); }
    void FirmwareUpload        (AsyncWebServerRequest* request, String filename, uint32_t index, uint8_t* data, uint32_t len, bool final);
    void NetworkStateChanged   (bool NewNetworkState);
    void GetDriverName         (String & Name) { Name = "WebMgr"; }
    void CreateAdminInfoFile ();
    void GetFseqFileListHandler(AsyncWebServerRequest *request);

private:

    EFUpdate               efupdate;
    DeviceCallbackFunction pAlexaCallback = nullptr;
    EspalexaDevice *       pAlexaDevice   = nullptr;
    bool                   HasBeenInitialized = false;

#ifdef ARDUINO_ARCH_ESP32
#   define     STATUS_DOC_SIZE 4000
#else
#   define     STATUS_DOC_SIZE 2500
#endif // def ARDUINO_ARCH_ESP32

    void init ();
    void processCmdGet              (JsonObject & jsonCmd);
    bool processCmdSet              (JsonObject & jsonCmd);
    void processCmdOpt              (JsonObject & jsonCmd);
    void processCmdDelete           (JsonObject & jsonCmd);
    void processCmdSetTime          (JsonObject & jsonCmd);

    void GetConfiguration           ();
    void GetOptions                 ();

    void ProcessXJRequest           (AsyncWebServerRequest * client);
    void ProcessSetTimeRequest      (time_t DateTime);

    void GetDeviceOptions           ();
    void GetInputOptions            ();
    void GetOutputOptions           ();

    using WebJsonDocument = JsonDocument;

    WebJsonDocument *WebJsonDoc = nullptr;
    size_t GetFseqFileListChunk(uint8_t *buffer, size_t maxlen, size_t index);
    c_FileMgr::FileId FileHandle = c_FileMgr::INVALID_FILE_HANDLE;
    size_t TotalFileSizeToTransfer = 0;
    size_t NumberOfBytesTransfered = 0;
}; // c_WebMgr

extern c_WebMgr WebMgr;
