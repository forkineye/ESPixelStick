/*
* NetworkMgr.cpp - Input Management class
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
*   This is a factory class used to manage the Input channel. It creates and deletes
*   the Input channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"
#include "NetworkMgr.hpp"
#include "../input/InputMgr.hpp"
#include "../service/FPPDiscovery.h"
#include "../WebMgr.hpp"

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_NetworkMgr::c_NetworkMgr ()
{


} // c_NetworkMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the Input channels into a safe state
c_NetworkMgr::~c_NetworkMgr ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_NetworkMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_NetworkMgr::Begin ()
{
    // DEBUG_START;

    // prevent recalls
    if (true == HasBeenInitialized) { return; }
    HasBeenInitialized = true;

    WiFiDriver.Begin ();

#ifdef SUPPORT_ETHERNET
    EthernetDriver.Begin ();
#endif // def SUPPORT_ETHERNET

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_NetworkMgr::GetConfig (JsonObject & json)
{
    // DEBUG_START;

    JsonObject NetworkConfig = json.createNestedObject (CN_network);

    NetworkConfig[CN_hostname] = config.hostname;

    JsonObject NetworkWiFiConfig = NetworkConfig.createNestedObject (CN_wifi);
    WiFiDriver.GetConfig (NetworkWiFiConfig);

#ifdef SUPPORT_ETHERNET
    JsonObject NetworkEthConfig = NetworkConfig.createNestedObject (CN_eth);
    WiFiDriver.GetConfig (NetworkEthConfig);

#endif // def SUPPORT_ETHERNET

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
void c_NetworkMgr::GetStatus (JsonObject & json)
{
    // DEBUG_START;

    JsonObject NetworkStatus = json.createNestedObject (CN_network);
    String name;
    WiFiDriver.GetWiFiHostname (name);
    NetworkStatus[CN_hostname] = name;

    JsonObject NetworkWiFiStatus = NetworkStatus.createNestedObject (CN_wifi);
    WiFiDriver.GetStatus (NetworkWiFiStatus);

#ifdef SUPPORT_ETHERNET
    JsonObject NetworkEthStatus = NetworkStatus.createNestedObject (CN_eth);
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
        WiFiDriver.Poll ();

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

    do // once
    {
        if (!json.containsKey (CN_network))
        {
            logcon (String (F ("No network config found. Use default settings")));
            // request config save
            ConfigSaveNeeded = true;
            break;
        }

        JsonObject network = json[CN_network];

        ConfigChanged |= setFromJSON (config.hostname, network, CN_hostname);

        if (network.containsKey (CN_wifi))
        {
            JsonObject networkWiFi = network[CN_wifi];
            ConfigChanged |= WiFiDriver.SetConfig (networkWiFi);
        }
        else
        {
            // this may be an old style config
            if (network.containsKey (CN_ssid))
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

#ifdef SUPPORT_ETHERNET
        if (network.containsKey (CN_Eth))
        {
            JsonObject networkEth = network[CN_eth];
            ConfigChanged |= EthernetDriver.SetConfig (networkEth);



            JsonObject networkEth = network[CN_Eth];

            String ip = config.Network.Eth.ip.toString ();
            String gateway = config.Network.Eth.gateway.toString ();
            String netmask = config.Network.Eth.netmask.toString ();

            ConfigChanged |= setFromJSON (ip, networkEth, CN_ip);
            ConfigChanged |= setFromJSON (netmask, networkEth, CN_netmask);
            ConfigChanged |= setFromJSON (gateway, networkEth, CN_gateway);
            ConfigChanged |= setFromJSON (config.Network.WiFi.UseDhcp, networkEth, CN_dhcp);

            // DEBUG_V ("     ip: " + ip);
            // DEBUG_V ("gateway: " + gateway);
            // DEBUG_V ("netmask: " + netmask);

            config.Network.Eth.ip.fromString (ip);
            config.Network.Eth.gateway.fromString (gateway);
            config.Network.Eth.netmask.fromString (netmask);
        }
        else
        {
            logcon (String (F ("No network Ethernet settings found. Using default Ethernet Settings")));
        }
#endif // def SUPPORT_ETHERNET


    } while (false);

    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_NetworkMgr::SetWiFiIsConnected (bool newState)
{
    // DEBUG_START;

    if (IsWiFiConnected != newState)
    {
        IsWiFiConnected = newState;
        InputMgr.NetworkStateChanged (newState);
        WebMgr.NetworkStateChanged (newState);
        FPPDiscovery.NetworkStateChanged (newState);
    }

    // DEBUG_END;
} // SetWiFiIsConnected

//-----------------------------------------------------------------------------
void c_NetworkMgr::SetEthernetIsConnected (bool newState)
{
    InputMgr.NetworkStateChanged (newState);
    WebMgr.NetworkStateChanged (newState);
    FPPDiscovery.NetworkStateChanged (newState);
} // SetEthernetIsConnected

// create a global instance of the network manager
c_NetworkMgr NetworkMgr;
