/*
* NetworkMgr.cpp - Input Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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
*   This is a factory class used to manage the Input channel. It creates and deletes
*   the Input channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "ESPixelStick.h"
#include "network/NetworkMgr.hpp"
#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"
#include "WebMgr.hpp"
#include <Int64String.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif // def ARDUINO_ARCH_ESP8266

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_NetworkMgr::c_NetworkMgr ()
{
    memset(hostname, 0x0, sizeof(hostname));
} // c_NetworkMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the Input channels into a safe state
c_NetworkMgr::~c_NetworkMgr ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_NetworkMgr

//-----------------------------------------------------------------------------
void c_NetworkMgr::AdvertiseNewState ()
{
    // DEBUG_START;

    if (PreviousState != IsConnected ())
    {
        // DEBUG_V ("Sending Advertisments");
        PreviousState = IsConnected ();
        InputMgr.NetworkStateChanged (IsConnected ());
        WebMgr.NetworkStateChanged (IsConnected ());
        FileMgr.NetworkStateChanged (IsConnected ());
        FPPDiscovery.NetworkStateChanged (IsConnected ());
        UpdateMdns(IsConnected ());
    }

    // DEBUG_END;
} // AdvertiseNewState

//-----------------------------------------------------------------------------
///< Start the module
void c_NetworkMgr::Begin ()
{
    // DEBUG_START;

    // prevent recalls
    if (true == HasBeenInitialized) { return; }
    HasBeenInitialized = true;

    // Make sure the local config is valid
    Validate ();

#ifdef SUPPORT_WIFI
    WiFiDriver.Begin ();
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
    EthernetDriver.Begin ();
#endif // def SUPPORT_ETHERNET

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_NetworkMgr::GetConfig (JsonObject & json)
{
    // DEBUG_START;

    JsonObject NetworkConfig = json[(char*)CN_network].to<JsonObject> ();

    JsonWrite(NetworkConfig, CN_hostname, hostname);

#ifdef SUPPORT_WIFI
    JsonObject NetworkWiFiConfig = NetworkConfig[(char*)CN_wifi].to<JsonObject> ();
    WiFiDriver.GetConfig (NetworkWiFiConfig);
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
    JsonWrite(NetworkConfig, CN_weus, AllowWiFiAndEthUpSimultaneously);

    JsonObject NetworkEthConfig = NetworkConfig[(char*)CN_eth].to<JsonObject> ();
    EthernetDriver.GetConfig (NetworkEthConfig);

#endif // def SUPPORT_ETHERNET

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
IPAddress c_NetworkMgr::GetlocalIP ()
{
#ifdef SUPPORT_WIFI
    if (IsWiFiConnected)
    {
        return WiFiDriver.getIpAddress();
    }
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
    if (IsEthernetConnected)
    {
        return EthernetDriver.GetIpAddress();
    }
#endif // def SUPPORT_ETHERNET

#ifdef SUPPORT_WIFI
    // Not connected: fall back to whatever the WiFi driver reports (AP IP etc.)
    return WiFiDriver.getIpAddress();
#else
    return IPAddress(INADDR_NONE);
#endif // def SUPPORT_WIFI
} // GetlocalIP

//-----------------------------------------------------------------------------
void c_NetworkMgr::GetStatus (JsonObject & json)
{
    // DEBUG_START;

    JsonObject NetworkStatus = json[(char*)CN_network].to<JsonObject> ();
    String name;
    GetHostname (name);
    JsonWrite(NetworkStatus, CN_hostname, name);

#ifdef SUPPORT_WIFI
    JsonObject NetworkWiFiStatus = NetworkStatus[(char*)CN_wifi].to<JsonObject> ();
    WiFiDriver.GetStatus (NetworkWiFiStatus);
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
    JsonObject NetworkEthStatus = NetworkStatus[(char*)CN_eth].to<JsonObject> ();
    EthernetDriver.GetStatus (NetworkEthStatus);
#endif // def SUPPORT_ETHERNET

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
///< Called from loop()
void c_NetworkMgr::Poll ()
{
    // DEBUG_START;

    do // once
    {
#ifdef SUPPORT_WIFI
        WiFiDriver.Poll ();
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
        EthernetDriver.Poll ();
#endif // def SUPPORT_ETHERNET

    } while (false);

    // DEBUG_END;
} // Process

//-----------------------------------------------------------------------------
bool c_NetworkMgr::SetConfig (JsonObject & json)
{
    // DEBUG_START;
    bool ConfigChanged = false;
    bool HostnameChanged = false;

    do // once
    {
        JsonObject network = json[(char*)CN_network];
        if (!network)
        {
            logcon (String (F ("No network config found. Use default settings")));
            // request config save
            ConfigSaveNeeded = true;
            break;
        }
        // DEBUG_V("");

        HostnameChanged = setFromJSON (hostname, network, CN_hostname);
        // DEBUG_V("");

#ifdef SUPPORT_WIFI
        JsonObject networkWiFi = network[(char*)CN_wifi];
        if (networkWiFi)
        {
            // DEBUG_V("");
            ConfigChanged |= WiFiDriver.SetConfig (networkWiFi);
        }
        else
        {
            // DEBUG_V("");
            JsonObject ssid = network[(char*)CN_ssid];
            // this may be an old style config
            if (!ssid)
            {
                logcon (String (F ("Using old style WiFi Settings")));
                // request config save
                ConfigSaveNeeded = true;
                ConfigChanged |= WiFiDriver.SetConfig (network);
            }
            else
            {
                logcon (String (F ("No network WiFi settings found. Using default WiFi Settings")));
            }
        }
#endif // def SUPPORT_WIFI

#ifdef SUPPORT_ETHERNET
        ConfigChanged = setFromJSON (AllowWiFiAndEthUpSimultaneously, network, CN_weus);

        JsonObject networkEth = network[(char*)CN_eth];
        if (networkEth)
        {
            ConfigChanged |= EthernetDriver.SetConfig (networkEth);
        }
        else
        {
            logcon (String (F ("No network Ethernet settings found. Using default Ethernet Settings")));
        }

        // DEBUG_V (String ("            IsEthernetConnected: ") + String (IsEthernetConnected));
        // DEBUG_V (String ("AllowWiFiAndEthUpSimultaneously: ") + String (AllowWiFiAndEthUpSimultaneously));
        SetWiFiEnable ();

#endif // def SUPPORT_ETHERNET

    } while (false);

    // DEBUG_V("");
    HostnameChanged |= Validate ();
    // DEBUG_V("");

    if(HostnameChanged)
    {
        // DEBUG_V(String("hostname: ") + hostname);
        String temp = String(hostname);
#ifdef SUPPORT_WIFI
        WiFiDriver.SetHostname (temp);
#endif // def SUPPORT_WIFI
#ifdef SUPPORT_ETHERNET
        EthernetDriver.SetHostname (temp);
#endif // def SUPPORT_ETHERNET
    }
    // DEBUG_V("");

    ConfigChanged |= HostnameChanged;

    // DEBUG_V (String ("ConfigChanged: ") + String (ConfigChanged));
    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
bool c_NetworkMgr:: Validate ()
{
    // DEBUG_START;
    bool Changed = false;

    // DEBUG_V (String ("hostname: \"") + hostname + "\"");
    if (0 == strlen(hostname))
    {
        #ifdef ARDUINO_ARCH_ESP8266
        String chipId = String (ESP.getChipId (), HEX);
        #else
        String chipId = int64String (ESP.getEfuseMac (), HEX);
        #endif
        // DEBUG_V ("Setting Hostname default");
        SafeStrncpy(hostname, (String(F("esps-")) + String (chipId)).c_str(), sizeof(hostname));
        Changed = true;
    }
    // DEBUG_V (String ("hostname: \"") + hostname + "\"");
    // DEBUG_V (String (" Changed: ") + String (Changed));

    // DEBUG_END;
    return Changed;
} // Validate

//-----------------------------------------------------------------------------
void c_NetworkMgr::SetWiFiIsConnected (bool newState)
{
    // DEBUG_START;

    if (IsWiFiConnected != newState)
    {
        IsWiFiConnected = newState;
        AdvertiseNewState ();
    }

    // DEBUG_END;
} // SetWiFiIsConnected

//-----------------------------------------------------------------------------
void c_NetworkMgr::SetWiFiEnable ()
{
    // DEBUG_START;

#ifdef SUPPORT_WIFI
    if (!AllowWiFiAndEthUpSimultaneously)
    {
        // DEBUG_V ("!AllowWiFiAndEthUpSimultaneously");
        if (true == IsEthernetConnected)
        {
            // DEBUG_V ("Disable");
            WiFiDriver.Disable ();
        }
        else
        {
            // DEBUG_V ("Enable");
            WiFiDriver.Enable ();
        }
    }
    else
    {
        // DEBUG_V ("AllowWiFiAndEthUpSimultaneously");
        WiFiDriver.Enable ();
    }
#endif // def SUPPORT_WIFI
    // DEBUG_END;

} // SetWiFiEnabled

//-----------------------------------------------------------------------------
void c_NetworkMgr::SetEthernetIsConnected (bool newState)
{
    // DEBUG_START;
    // DEBUG_V (String ("newState: ") + String (newState));

    if (IsEthernetConnected != newState)
    {
        IsEthernetConnected = newState;
        SetWiFiEnable ();
        AdvertiseNewState ();
    }

    // DEBUG_END;
} // SetEthernetIsConnected

//-----------------------------------------------------------------------------
void c_NetworkMgr::UpdateMdns(bool NetworkIsUp)
{
    // DEBUG_START;

    if(NetworkIsUp)
    {
        if (MDNS.begin(hostname))
        {
            // DEBUG_V("MDNS Started");
            MDNS.addService("http", "tcp", 80);
        }
        else
        {
            logcon (F("Error setting up MDNS responder!"));
            // RequestReboot(msg, 10000);
        }
    }
    else
    {
        MDNS.end();
    }
    // DEBUG_END;
}

// create a global instance of the network manager
c_NetworkMgr NetworkMgr;
