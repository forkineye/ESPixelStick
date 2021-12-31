#pragma once
/*
* NetworkMgr.hpp - Input Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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
*   This is a factory class used to manage the input channels. It creates and deletes
*   the input channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"
#include "WiFiDriver.hpp"
#ifdef SUPPORT_ETHERNET
#   include "EthernetDriver.hpp"
#endif // def SUPPORT_ETHERNET

class c_NetworkMgr
{
public:
    c_NetworkMgr ();
    virtual ~c_NetworkMgr ();

    void Begin                ();
    void GetConfig            (JsonObject & json);
    void GetStatus            (JsonObject & json);
    bool SetConfig            (JsonObject & json);
    void Poll                 ();
    void GetDriverName        (String & Name) { Name = "NetworkMgr"; }

    void SetWiFiIsConnected     (bool newState);
    void SetEthernetIsConnected (bool newState);

    bool IsConnected () { return (IsWiFiConnected || IsEthernetConnected); }

private:

    c_WiFiDriver    WiFiDriver;
#ifdef SUPPORT_ETHERNET
    c_WiFiDriver    EthernetDriver;
#endif // def SUPPORT_ETHERNET

    bool    HasBeenInitialized  = false;
    bool    IsWiFiConnected     = false;
    bool    IsEthernetConnected = false;

}; // c_NetworkMgr

extern c_NetworkMgr NetworkMgr;
