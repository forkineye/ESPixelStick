/*
* pEthernetDriver->cpp - Output Management class
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

#include "ETH_m.h"
#include "NetworkMgr.hpp"
#include "EthernetDriver.hpp"

/*****************************************************************************/
/* FSM                                                                       */
/*****************************************************************************/
static fsm_Eth_state_Boot              fsm_Eth_state_Boot_imp;
static fsm_Eth_state_PoweringUp        fsm_Eth_state_PoweringUp_imp;
static fsm_Eth_state_ConnectingToEth   fsm_Eth_state_ConnectingToEth_imp;
static fsm_Eth_state_ConnectedToEth    fsm_Eth_state_ConnectedToEth_imp;
static fsm_Eth_state_ConnectionFailed  fsm_Eth_state_ConnectionFailed_imp;
static fsm_Eth_state_DeviceInitFailed  fsm_Eth_state_DeviceInitFailed_imp;

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_EthernetDriver::c_EthernetDriver ()
{
    // DEBUG_START;

    fsm_Eth_state_Boot_imp.SetParent (this);
    fsm_Eth_state_PoweringUp_imp.SetParent (this);
    fsm_Eth_state_ConnectingToEth_imp.SetParent (this);
    fsm_Eth_state_ConnectedToEth_imp.SetParent (this);
    fsm_Eth_state_ConnectionFailed_imp.SetParent (this);
    fsm_Eth_state_DeviceInitFailed_imp.SetParent (this);

    // this gets called pre-setup so there is nothing we can do here.
    fsm_Eth_state_Boot_imp.Init ();

    // DEBUG_END;
} // c_EthernetDriver

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_EthernetDriver::~c_EthernetDriver ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_EthernetDriver

//-----------------------------------------------------------------------------
void c_EthernetDriver::AnnounceState ()
{
    // DEBUG_START;

    String StateName;
    pCurrentFsmState->GetStateName (StateName);
    logcon (String (F ("Entering State: ")) + StateName);

    // DEBUG_END;

} // AnnounceState

//-----------------------------------------------------------------------------
///< Start the module
void c_EthernetDriver::Begin ()
{
    // DEBUG_START;

    // Setup Ethernet Handlers
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onEthStartHandler      (event, info); }, WiFiEvent_t::ARDUINO_EVENT_ETH_START);
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onEthConnectHandler    (event, info); }, WiFiEvent_t::ARDUINO_EVENT_ETH_GOT_IP);
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onEthDisconnectHandler (event, info); }, WiFiEvent_t::ARDUINO_EVENT_ETH_DISCONNECTED);

    // set up the poll interval
    NextPollTime = millis () + PollInterval;

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_EthernetDriver::connectEth ()
{
    // DEBUG_START;

    String Hostname;
    NetworkMgr.GetHostname (Hostname);

    // DEBUG_V (String ("Hostname: ") + Hostname);
    if (0 != Hostname.length ())
    {
        // DEBUG_V (String ("Setting ETH hostname: ") + Hostname);

        // ETH_m.config (INADDR_NONE, INADDR_NONE, INADDR_NONE);
        ETH_m.setHostname (Hostname.c_str ());
    }

    logcon (String (F ("Ethernet Connecting as ")) + Hostname);

    // DEBUG_END;
} // connectEth

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    json[CN_ip]      = ip.toString ();
    json[CN_netmask] = netmask.toString ();
    json[CN_gateway] = gateway.toString ();
    json[CN_dhcp]    = UseDhcp;

    json[F ("phy_adr")]  = phy_addr;
    json[F ("power")]    = power_pin;
    json[F ("mdc")]      = mdc_pin;
    json[F ("mdio")]     = mdio_pin;
    json[F ("phy_type")] = phy_type;
    json[F ("clk_mode")] = clk_mode;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetHostname (String & Name)
{
    Name = ETH_m.getHostname ();
} // GetHostname

//-----------------------------------------------------------------------------
IPAddress c_EthernetDriver::GetIpAddress ()
{
    return ETH_m.localIP ();
} // GetIpAddress

//-----------------------------------------------------------------------------
IPAddress c_EthernetDriver::GetIpSubNetMask ()
{
    return ETH_m.subnetMask ();
} // GetIpSubNetMask

//-----------------------------------------------------------------------------
String c_EthernetDriver::GetMacAddress ()
{
    return ETH_m.macAddress ();
} // GetMacAddress

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    String Hostname;
    GetHostname (Hostname);
    jsonStatus[CN_hostname] = Hostname;

    jsonStatus[CN_ip] = GetIpAddress ().toString ();
    jsonStatus[CN_subnet] = GetIpSubNetMask ().toString ();
    jsonStatus[CN_mac] = GetMacAddress ();
    jsonStatus[CN_connected] = IsConnected ();

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
bool c_EthernetDriver::IsConnected ()
{
    // DEBUG_V("");

    return (pCurrentFsmState == &fsm_Eth_state_ConnectedToEth_imp);

} // IsConnected

//------------------------------------------------------------------------------
void c_EthernetDriver::NetworkStateChanged (bool NetworkState)
{
    // DEBUG_START;

    NetworkMgr.SetEthernetIsConnected (NetworkState);

    // DEBUG_END;

} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_EthernetDriver::onEthConnectHandler (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_START;

    pCurrentFsmState->OnConnect ();

    // DEBUG_END;

} // onEthConnectHandler

//-----------------------------------------------------------------------------
void c_EthernetDriver::onEthDisconnectHandler (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_START;

    pCurrentFsmState->OnDisconnect ();

    // DEBUG_END;

} // onEthDisconnectHandler

//-----------------------------------------------------------------------------
void c_EthernetDriver::onEthStartHandler (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_START;

    String Hostname;
    NetworkMgr.GetHostname (Hostname);
    ETH_m.setHostname (Hostname.c_str ());

    // DEBUG_END;

} // onEthStartHandler

//-----------------------------------------------------------------------------
void c_EthernetDriver::Poll ()
{
    // DEBUG_START;

    if (millis () > NextPollTime)
    {
        // DEBUG_V ("Polling");
        NextPollTime += PollInterval;
        pCurrentFsmState->Poll ();
    }

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_EthernetDriver::reset ()
{
    // DEBUG_START;

    logcon (F ("Ethernet Reset has been requested"));

    // Disconnect Ethernet if connected
    fsm_Eth_state_Boot_imp.Init ();
    // if (esp_eth_disable () != ESP_OK)
    {
        logcon (F ("Could not disconnect Ethernet"));
    }

    NetworkStateChanged (false);

    // DEBUG_END;
} // reset

//-----------------------------------------------------------------------------
bool c_EthernetDriver::SetConfig (JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;
    JsonObject networkEth = json[CN_eth];

    String sIP = ip.toString ();
    String sGateway = gateway.toString ();
    String sNetmask = netmask.toString ();

    ConfigChanged |= setFromJSON (sIP,      networkEth, CN_ip);
    ConfigChanged |= setFromJSON (sNetmask, networkEth, CN_netmask);
    ConfigChanged |= setFromJSON (sGateway, networkEth, CN_gateway);
    ConfigChanged |= setFromJSON (UseDhcp,  networkEth, CN_dhcp);

    ip.fromString (sIP);
    gateway.fromString (sGateway);
    netmask.fromString (sNetmask);

    // DEBUG_V ("     ip: " + ip);
    // DEBUG_V ("gateway: " + gateway);
    // DEBUG_V ("netmask: " + netmask);

    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_EthernetDriver::SetUpIp ()
{
    // DEBUG_START;
    do // once
    {
        if (true == UseDhcp)
        {
            logcon (F ("Connecting to Ethernet using DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());

        if (temp == ip)
        {
            logcon (F ("NETWORK: ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            break;
        }

        if ((ip      == ETH_m.localIP ())    &&
            (netmask == ETH_m.subnetMask ()) &&
            (gateway == ETH_m.gatewayIP ()))
        {
            // DEBUG_V ("correct IP is already set");
            break;
        }

        // DEBUG_V ("     ip: " + ip.toString ());
        // DEBUG_V ("netmask: " + netmask.toString ());
        // DEBUG_V ("gateway: " + gateway.toString ());

        // We didn't use DNS, so just set it to our configured gateway
        ETH_m.config (ip, gateway, netmask, gateway);

        logcon (F ("Connecting to Ethernet with Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
void c_EthernetDriver::StartEth ()
{
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
    if (false == ETH_m.begin (phy_addr, power_pin, mdc_pin, mdio_pin, phy_type, clk_mode))
    {
        fsm_Eth_state_DeviceInitFailed_imp.Init ();
    }
} // StartEth

//-----------------------------------------------------------------------------
int c_EthernetDriver::ValidateConfig ()
{
    // DEBUG_START;

    int response = 0;

    // DEBUG_END;

    return response;

} // ValidateConfig

/*****************************************************************************/
//  FSM Code
/*****************************************************************************/
/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    // pEthernetDriver->AnnounceState ();
    pEthernetDriver->SetFsmStartTime (millis ());

    // DEBUG_END;

} // fsm_Eth_state_Boot::Init

/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Poll ()
{
    // DEBUG_START;

    // Start trying to connect to based on input config
    fsm_Eth_state_PoweringUp_imp.Init ();

    // DEBUG_END;
} // fsm_Eth_state_boot

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_PoweringUp::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->SetFsmStartTime (millis ());

    pinMode (gpio_num_t::GPIO_NUM_15, OUTPUT);
    digitalWrite (gpio_num_t::GPIO_NUM_15, LOW);

    // DEBUG_END;

} // fsm_Eth_state_PoweringUp::Init

/*****************************************************************************/
void fsm_Eth_state_PoweringUp::Poll ()
{
    // DEBUG_START;

    // Start trying to connect to based on input config
    fsm_Eth_state_ConnectingToEth_imp.Init ();

    // this may throw the connected handler
    pEthernetDriver->StartEth ();
    pEthernetDriver->connectEth ();

    // DEBUG_END;
} // fsm_Eth_state_PoweringUp

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->SetFsmStartTime (millis ());

    // DEBUG_END;

} // fsm_Eth_state_ConnectingToEthUsingConfig::Init

/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::Poll ()
{
    // DEBUG_START;

    // wait for the connection to complete via the callback function
    uint32_t CurrentTimeMS = millis ();

    // @TODO Ethernet connection timeout is currently hardcoded. Add
    // to network config.
    if (CurrentTimeMS - pEthernetDriver->GetFsmStartTime () > (60000))
    {
        // logcon (F ("Ethernet Failed to connect"));
        fsm_Eth_state_ConnectionFailed_imp.Init ();
    }

    // DEBUG_END;
} // fsm_Eth_state_ConnectingToEth::Poll

/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::OnConnect ()
{
    // DEBUG_START;

    fsm_Eth_state_ConnectedToEth_imp.Init ();

    // DEBUG_END;

} // fsm_Eth_state_ConnectingToEth::OnConnect

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_ConnectedToEth::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->SetFsmStartTime (millis ());

    pEthernetDriver->SetUpIp ();

    // pEthernetDriver->setIpAddress (ETH_m.localIP ());
    // pEthernetDriver->setIpSubNetMask (ETH_m.subnetMask ());
    // pEthernetDriver->setMacAddress (ETH_m.macAddress ());

    logcon (String (F ("Ethernet Connected with IP: ")) + pEthernetDriver->GetIpAddress ().toString ());

    pEthernetDriver->NetworkStateChanged (true);

    // DEBUG_END;

} // fsm_Eth_state_ConnectedToEth::Init

/*****************************************************************************/
void fsm_Eth_state_ConnectedToEth::OnDisconnect ()
{
    // DEBUG_START;

    fsm_Eth_state_ConnectionFailed_imp.Init ();
    pEthernetDriver->NetworkStateChanged (false);

    // DEBUG_END;

} // fsm_Eth_state_ConnectedToEth::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_ConnectionFailed::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->NetworkStateChanged (false);

    ETH_m.stop ();

    // DEBUG_END;

} // fsm_Eth_state_ConnectionFailed::Init

/*****************************************************************************/
void fsm_Eth_state_ConnectionFailed::Poll ()
{
    // DEBUG_START;

    // take some recovery action
    fsm_Eth_state_ConnectingToEth_imp.Init ();

    ETH_m.start ();

    // DEBUG_END;
} // fsm_Eth_state_ConnectionFailed::Poll

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_DeviceInitFailed::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->NetworkStateChanged (false);

    // DEBUG_END;

} // fsm_Eth_state_ConnectionFailed::Init

#endif // def SUPPORT_ETHERNET
