/*
* EthernetMgr.cpp - Output Management class
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
*/

#include "../ESPixelStick.h"

#ifdef SUPPORT_ETHERNET

#include "EthernetMgr.hpp"
#include <ETH.h>
#include "esp_eth.h"

#include "input/InputMgr.hpp"
#include "WebMgr.hpp"
#include "service/FPPDiscovery.h"

// Ethernet connection status as ETH doesn't currently have a status function
static bool eth_connected = false;

void EthTrackingEvent (WiFiEvent_t event)
{
    switch (event)
    {
        case SYSTEM_EVENT_ETH_START:
        {
            eth_connected = false;
            Serial.println ("ETH Started");
            //set eth hostname here
            // ETH.setHostname("esp32-ethernet");
            break;
        }
        case SYSTEM_EVENT_ETH_CONNECTED:
        {
            Serial.println ("ETH Connected");
            break;
        }
        case SYSTEM_EVENT_ETH_GOT_IP:
        {
            Serial.print ("ETH MAC: ");
            Serial.print (ETH.macAddress ());
            Serial.print (", IPv4: ");
            Serial.print (ETH.localIP ());
            if (ETH.fullDuplex ())
            {
                Serial.print (", FULL_DUPLEX");
            }
            Serial.print (", ");
            Serial.print (ETH.linkSpeed ());
            Serial.println ("Mbps");
            eth_connected = true;
            break;
        }

        case SYSTEM_EVENT_ETH_DISCONNECTED:
        {
            Serial.println ("ETH Disconnected");
            eth_connected = false;
            break;
        }

        case SYSTEM_EVENT_ETH_STOP:
        {
            Serial.println ("ETH Stopped");
            eth_connected = false;
            break;
        }

        default:
            break;
    } // switch (event)

} // EthTrackingEvent

/*****************************************************************************/
/* FSM                                                                       */
/*****************************************************************************/
fsm_Eth_state_Boot                    fsm_Eth_state_Boot_imp;
fsm_Eth_state_ConnectingToEth         fsm_Eth_state_ConnectingToEth_imp;
fsm_Eth_state_ConnectedToEth          fsm_Eth_state_ConnectedToEth_imp;

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_EthernetMgr::c_EthernetMgr ()
{
    ip = IPAddress ((uint32_t)0);
    netmask = IPAddress ((uint32_t)0);
    gateway = IPAddress ((uint32_t)0);
    UseDhcp = true;

    // this gets called pre-setup so there is nothing we can do here.
    fsm_Eth_state_Boot_imp.Init ();
} // c_EthernetMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_EthernetMgr::~c_EthernetMgr ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_EthernetMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_EthernetMgr::Begin ()
{
    // DEBUG_START;

    // Setup WiFi Handlers
    WiFi.onEvent (EthTrackingEvent);

    // set up the poll interval
    NextPollTime = millis () + PollInterval;

    // get the FSM moving
    pCurrentFsmState->Poll ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_EthernetMgr::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    jsonStatus[CN_ip] = getIpAddress ().toString ();
    jsonStatus[CN_subnet] = getIpSubNetMask ().toString ();
    jsonStatus[CN_mac] = getMacAddress ();
    jsonStatus[CN_hostname] = getHostname ();

    // DEBUG_END;
} // GetStatus

//------------------------------------------------------------------------------
// Broadcast network state change to relevant managers
void c_EthernetMgr::NetworkStateChanged (bool NetworkState)
{
    // DEBUG_START;

    InputMgr.NetworkStateChanged (NetworkState);
    WebMgr.NetworkStateChanged (NetworkState);
    FPPDiscovery.NetworkStateChanged (NetworkState);

    // DEBUG_END;

} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_EthernetMgr::connectEth ()
{
    // DEBUG_START;

    // @TODO The ethernet setup currently runs against default hardware setup.
    // Rather than carry the configuration here, these defaults can be
    // overridden as -D statement upon compilation. This should be documented at
    // some point.
    // begin(uint8_t phy_addr=ETH_PHY_ADDR, 
    //       int power=ETH_PHY_POWER,
    //       int mdc=ETH_PHY_MDC, 
    //       int mdio=ETH_PHY_MDIO, 
    //       eth_phy_type_t type=ETH_PHY_TYPE,
    //       eth_clock_mode_t clk_mode=ETH_CLK_MODE);
    // if (!eth_connected)
    // esp_eth_disable();
    ETH.begin ();

    DEBUG_V (String ("config.hostname: ") + config.hostname);
    if (0 != config.hostname.length ())
    {
        DEBUG_V (String ("Setting ETH hostname: ") + config.hostname);

        // ETH.config (INADDR_NONE, INADDR_NONE, INADDR_NONE);
        ETH.setHostname (config.hostname.c_str ());
    }
    EthernetMgr.setHostname (ETH.getHostname ());

    logcon.println (String (F ("\nEthernet Connecting as ")) + config.hostname);

    // DEBUG_END;
} // connectEth

//-----------------------------------------------------------------------------
void c_EthernetMgr::reset ()
{
    // DEBUG_START;

    logcon.println (F ("Ethernet Reset has been requested"));

    // Reset connection statuses
    SetIsEthConnected (false);

    // Disconnect Ethernet if connected
    fsm_Eth_state_Boot_imp.Init ();
    if (esp_eth_disable () != ESP_OK)
    {
        logcon.println (F ("Could not disconnect Ethernet"));
    }

    NetworkStateChanged (false);

    // DEBUG_END;
} // reset

//-----------------------------------------------------------------------------
bool c_EthernetMgr::IsConnected ()
{
    DEBUG_V("");

    return (pCurrentFsmState == &fsm_Eth_state_ConnectedToEth_imp);

} // IsConnected

//-----------------------------------------------------------------------------
void c_EthernetMgr::SetUpIp ()
{
    // DEBUG_START;
    do // once
    {
        if (true == config.UseDhcp)
        {
            logcon.println (F ("Connecting to Ethernet using DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());
        // DEBUG_V ("     ip: " + config.ip.toString());
        // DEBUG_V ("netmask: " + config.netmask.toString ());
        // DEBUG_V ("gateway: " + config.gateway.toString ());

        if (temp == config.ip)
        {
            logcon.println (F ("NETWORK: ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            break;
        }

        if ((config.ip == ETH.localIP ()) &&
            (config.netmask == ETH.subnetMask ()) &&
            (config.gateway == ETH.gatewayIP ()))
        {
            // correct IP is already set
            break;
        }

        // We didn't use DNS, so just set it to our configured gateway
        ETH.config (config.ip, config.gateway, config.netmask, config.gateway);

        logcon.println (F ("Connecting to Ethernet with Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
int c_EthernetMgr::ValidateConfig ()
{
    // DEBUG_START;

    int response = 0;

    // DEBUG_END;

    return response;

} // ValidateConfig

//-----------------------------------------------------------------------------
void c_EthernetMgr::AnnounceState ()
{
    // DEBUG_START;

    String StateName;
    pCurrentFsmState->GetStateName (StateName);
    logcon.println (String (F ("\nEthernet Network Entering State: ")) + StateName);

    // DEBUG_END;

} // AnnounceState

//-----------------------------------------------------------------------------
void c_EthernetMgr::Poll ()
{
    // DEBUG_START;

    if (millis () > NextPollTime)
    {
        // DEBUG_V ("");
        NextPollTime += PollInterval;
        pCurrentFsmState->Poll ();
    }

    // DEBUG_END;

} // Poll

/*****************************************************************************/
//  FSM Code
/*****************************************************************************/
/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Poll ()
{
    // DEBUG_START;

    // Start trying to connect to based on input config
    fsm_Eth_state_ConnectingToEth_imp.Init ();
    // DEBUG_END;
} // fsm_Eth_state_boot

/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Init ()
{
    DEBUG_START;

    EthernetMgr.SetFsmState (this);

    // This can get called before the system is up and running.
    // No log port available yet
    // EthernetMgr.AnnounceState ();

    DEBUG_END;
} // fsm_Eth_state_Boot::Init

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectingToEth::Poll ()
{
    // DEBUG_START;
    // DEBUG_V ("Eth config poll");
    // wait for the connection to complete via the callback function
    uint32_t CurrentTimeMS = millis ();

    // Check ethernet status first
    if (!eth_connected)
    {
        // @TODO Ethernet connection timeout is currently hardcoded to 10. Add
        // to network config.
        if (CurrentTimeMS - EthernetMgr.GetFsmStartTime () > (5000))
        {
            logcon.println (F ("\nEthernet Failed to connect"));
            fsm_Eth_state_Boot_imp.Init ();
        }
    }

    // DEBUG_END;
} // fsm_Eth_state_ConnectingToEth::Poll

/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectingToEth::Init ()
{
    DEBUG_START;

    EthernetMgr.SetFsmState (this);
    EthernetMgr.AnnounceState ();
    EthernetMgr.SetFsmStartTime (millis ());

    // do something useful

    DEBUG_END;

} // fsm_Eth_state_ConnectingToEthUsingConfig::Init

/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectingToEth::OnConnect ()
{
    DEBUG_START;

    fsm_Eth_state_ConnectedToEth_imp.Init ();

    DEBUG_END;

} // fsm_Eth_state_ConnectingToEth::OnConnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectedToEth::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
} // fsm_Eth_state_ConnectedToEth::Poll

/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectedToEth::Init ()
{
    DEBUG_START;

    EthernetMgr.SetFsmState (this);
    EthernetMgr.AnnounceState ();

    EthernetMgr.SetUpIp ();

    EthernetMgr.setIpAddress (ETH.localIP ());
    EthernetMgr.setIpSubNetMask (ETH.subnetMask ());
    EthernetMgr.setMacAddress (ETH.macAddress ());

    logcon.println (String (F ("Ethernet Connected with IP: ")) + EthernetMgr.getIpAddress ().toString ());

    EthernetMgr.NetworkStateChanged (EthernetMgr.IsConnected ());

    DEBUG_END;

} // fsm_Eth_state_ConnectedToEth::Init

/*****************************************************************************/
// Wait for events
void fsm_Eth_state_ConnectedToEth::OnDisconnect ()
{
    DEBUG_START;

    logcon.println (F ("Ethernet lost the connection"));
    fsm_Eth_state_Boot_imp.Init ();

    DEBUG_END;

} // fsm_Eth_state_ConnectedToEth::OnDisconnect

//-----------------------------------------------------------------------------
// create a global instance of the Ethernet Manager
c_EthernetMgr EthernetMgr;

#endif // def SUPPORT_ETHERNET
