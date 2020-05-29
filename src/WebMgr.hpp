// only include this file once
#pragma once
/*
* WiFiMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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

#include <Arduino.h>
#include "EFUpdate.h"
#include <ESPAsyncWebServer.h>

class c_WebMgr
{
public:
    c_WebMgr ();
    virtual ~c_WebMgr ();

    void      Begin           (config_t * NewConfig); ///< set up the operating environment based on the current config (or defaults)
    void      ValidateConfig  (config_t * NewConfig);

private:

    config_t      * config = nullptr;       // Current configuration
    EFUpdate        efupdate;
//    uint8_t * WSframetemp;
//    uint8_t * confuploadtemp;

    /// Valid "Simple" message types
    enum SimpleMessage 
    {
        GET_STATUS = 'J'
    };

    void   init             ();
    void   procSimple       (uint8_t* data, AsyncWebSocketClient* client);
    void   procJSON         (uint8_t* data, AsyncWebSocketClient* client);
    void   onWsEvent        (AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
    void   onFirmwareUpload (AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);
    String GetConfiguration ();

protected:

}; // c_WebMgr

extern c_WebMgr WebMgr;
