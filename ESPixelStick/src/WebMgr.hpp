#pragma once
/*
* WebMgr.hpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022Shelby Merrick
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
#include <ESPAsyncWebServer.h>
#include <EspalexaDevice.h>
#include "output/OutputMgr.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   if __has_include("SD.h")
#       include <SD.h>
#   endif //  __has_include("SD.h")
#else
#   if __has_include("SDFS.h")
#       include <SDFS.h>
#   endif //  __has_include("SDFS.h")
#endif // def ARDUINO_ARCH_ESP32

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

private:

    EFUpdate               efupdate;
    DeviceCallbackFunction pAlexaCallback = nullptr;
    EspalexaDevice *       pAlexaDevice   = nullptr;
    char *pWebSocketFrameCollectionBuffer = nullptr;
    bool                   HasBeenInitialized = false;

#define WebSocketFrameCollectionBufferSize (OM_MAX_CONFIG_SIZE + 100)

    /// Valid "Simple" message types
    enum SimpleMessage
    {
        GET_STATUS = 'J',
        GET_ADMIN = 'A',
        DO_RESET = '6',
        DO_FACTORYRESET = '7',
        PING = 'P',
    };

    void init ();
    void onWsEvent                  (AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, uint32_t len);
    void ProcessVseriesRequests     (AsyncWebSocketClient  * client);
    void ProcessGseriesRequests     (AsyncWebSocketClient  * client);
    void ProcessReceivedJsonMessage (AsyncWebSocketClient  * client);
    void processCmd                 (AsyncWebSocketClient  * client,  JsonObject & jsonCmd );
    void processCmdGet              (JsonObject & jsonCmd);
    bool processCmdSet              (JsonObject & jsonCmd);
    void processCmdOpt              (JsonObject & jsonCmd);
    void processCmdDelete           (JsonObject & jsonCmd);
    void processCmdSetTime          (JsonObject & jsonCmd);

    void GetConfiguration           ();
    void GetOptions                 ();
    void ProcessXseriesRequests     (AsyncWebSocketClient * client);
    void ProcessXARequest           (AsyncWebSocketClient * client);
    void ProcessXJRequest           (AsyncWebSocketClient * client);

    void GetDeviceOptions           ();
    void GetInputOptions            ();
    void GetOutputOptions           ();

#ifdef BOARD_HAS_PSRAM

    struct SpiRamAllocator
    {
        void *allocate(uint32_t size)
        {
            return ps_malloc(size);
        }

        void deallocate(void *pointer)
        {
            free(pointer);
        }

        void *reallocate(void *ptr, uint32_t new_size)
        {
            return ps_realloc(ptr, new_size);
        }
    };

    using WebJsonDocument = BasicJsonDocument<SpiRamAllocator>;
#else
    using WebJsonDocument = DynamicJsonDocument;
#endif // def BOARD_HAS_PSRAM

    WebJsonDocument *WebJsonDoc = nullptr;

}; // c_WebMgr

extern c_WebMgr WebMgr;
