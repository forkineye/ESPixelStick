/*
* WiFiMgr.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
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

#ifdef ARDUINO_ARCH_ESP8266
#   include <eagle_soc.h>
#   include <ets_sys.h>
#else
#   include <esp_wifi.h>
#endif // def ARDUINO_ARCH_ESP8266

#include "WiFiMgr.hpp"

//-----------------------------------------------------------------------------
// Create secrets.h with a #define for SECRETS_SSID and SECRETS_PASS
// or delete the #include and enter the strings directly below.
// #include "secrets.h"
#ifndef SECRETS_SSID
#   define SECRETS_SSID "DEFAULT_SSID_NOT_SET"
#   define SECRETS_PASS "DEFAULT_PASSPHRASE_NOT_SET"
#endif // ndef SECRETS_SSID

/* Fallback configuration if config->json is empty or fails */
const char ssid[]       = SECRETS_SSID;
const char passphrase[] = SECRETS_PASS;

/// Radio configuration
/** ESP8266 radio configuration routines that are executed at startup. */
/* Disabled for now, possible flash wear issue. Need to research further
RF_PRE_INIT() {
#ifdef ARDUINO_ARCH_ESP8266
    system_phy_set_powerup_option(3);   // Do full RF calibration on power-up
    system_phy_set_max_tpw(82);         // Set max TX power
#else
    esp_phy_erase_cal_data_in_nvs(); // Do full RF calibration on power-up
    esp_wifi_set_max_tx_power (78);  // Set max TX power
#endif
}
*/

/*****************************************************************************/
/* FSM                                                                       */
/*****************************************************************************/
fsm_WiFi_state_Boot                    fsm_WiFi_state_Boot_imp;
fsm_WiFi_state_ConnectingUsingConfig   fsm_WiFi_state_ConnectingUsingConfig_imp;
fsm_WiFi_state_ConnectingUsingDefaults fsm_WiFi_state_ConnectingDefault_imp;
fsm_WiFi_state_ConnectedToAP           fsm_WiFi_state_ConnectedToAP_imp;
fsm_WiFi_state_ConnectingAsAP          fsm_WiFi_state_ConnectingAsAP_imp;
fsm_WiFi_state_ConnectedToSta          fsm_WiFi_state_ConnectedToSta_imp;
fsm_WiFi_state_ConnectionFailed        fsm_WiFi_state_ConnectionFailed_imp;

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_WiFiMgr::c_WiFiMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
    fsm_WiFi_state_Boot_imp.Init ();
} // c_WiFiMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_WiFiMgr::~c_WiFiMgr()
{
 // DEBUG_START;

 // DEBUG_END;

} // ~c_WiFiMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_WiFiMgr::Begin (config_t* NewConfig)
{
    // DEBUG_START;

    // save the pointer to the config
    config = NewConfig;

    // Disable persistant credential storage and configure SDK params
    WiFi.persistent (false);

#ifdef ARDUINO_ARCH_ESP8266
    wifi_set_sleep_type (NONE_SLEEP_T);
#elif defined(ARDUINO_ARCH_ESP32)
    esp_wifi_set_ps (WIFI_PS_NONE);
#endif

    // DEBUG_V("");
    if (0 != config->hostname.length())
    {
#ifdef ARDUINO_ARCH_ESP8266
        WiFi.hostname (config->hostname);
#else
        WiFi.setHostname (config->hostname.c_str ());
#endif
    }
    // DEBUG_V ("");

    // Setup WiFi Handlers
#ifdef ARDUINO_ARCH_ESP8266
    wifiConnectHandler    = WiFi.onStationModeGotIP        ([this](const WiFiEventStationModeGotIP& event) {this->onWiFiConnect (event); });
    wifiDisconnectHandler = WiFi.onStationModeDisconnected ([this](const WiFiEventStationModeDisconnected& event) {this->onWiFiDisconnect (event); });
#else
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiConnect    (event, info);}, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiDisconnect (event, info);}, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
#endif

    // set up the poll interval
    NextPollTime = millis () + PollInterval;

    // get the FSM moving
    pCurrentFsmState->Poll ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_WiFiMgr::GetStatus (JsonObject & jsonStatus)
{
 // DEBUG_START;

    jsonStatus["rssi"]     = WiFi.RSSI ();
    jsonStatus["ip"]       = getIpAddress ().toString ();
    jsonStatus["subnet"]   = getIpSubNetMask ().toString ();
    jsonStatus["mac"]      = WiFi.macAddress ();
#ifdef ARDUINO_ARCH_ESP8266
    jsonStatus["hostname"] = WiFi.hostname ();
#else
    jsonStatus["hostname"] = WiFi.getHostname ();
#endif // def ARDUINO_ARCH_ESP8266
    jsonStatus["ssid"]     = WiFi.SSID ();

 // DEBUG_END;
} // GetStatus


//-----------------------------------------------------------------------------
void c_WiFiMgr::connectWifi ()
{
    // DEBUG_START;
    LOG_PORT.println (String(F ("\nWiFi Connecting to ")) + 
                      String(config->ssid) + 
                      String (F (" as ")) + 
                      String (config->hostname));

    WiFi.begin (config->ssid.c_str (), config->passphrase.c_str ());

    // DEBUG_END;
} // connectWifi

//-----------------------------------------------------------------------------
void c_WiFiMgr::reset ()
{
    // DEBUG_START;

    LOG_PORT.println (F ("WiFi Reset has been requested"));

    fsm_WiFi_state_Boot_imp.Init ();

    // DEBUG_END;
} // reset

//-----------------------------------------------------------------------------
void c_WiFiMgr::SetUpIp ()
{
    // DEBUG_START;

    do // once
    {
        if (true == config->UseDhcp)
        {
            LOG_PORT.println (F ("WiFi Connected with DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());
        // DEBUG_V ("     ip: " + config->ip.toString());
        // DEBUG_V ("netmask: " + config->netmask.toString ());
        // DEBUG_V ("gateway: " + config->gateway.toString ());

        if (temp == config->ip)
        {
            LOG_PORT.println (F ("WiFI: ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            config->UseDhcp = true;
            break;
        }

        if ((config->ip      == WiFi.localIP ())    &&
            (config->netmask == WiFi.subnetMask ()) &&
            (config->gateway == WiFi.gatewayIP ()))
        {
            // correct IP is already set
            break;
        }
        // We don't use DNS, so just set it to our gateway
        WiFi.config (config->ip, config->gateway, config->netmask, config->gateway);

        LOG_PORT.println (F ("Connected with Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP8266
void c_WiFiMgr::onWiFiConnect (const WiFiEventStationModeGotIP& event)
{
#else
void c_WiFiMgr::onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
#endif
    // DEBUG_START;

    pCurrentFsmState->OnConnect ();

    // DEBUG_END;
} // onWiFiConnect

//-----------------------------------------------------------------------------
/// WiFi Disconnect Handler
#ifdef ARDUINO_ARCH_ESP8266
/** Attempt to re-connect every 2 seconds */
void c_WiFiMgr::onWiFiDisconnect (const WiFiEventStationModeDisconnected & event)
{
#else
void c_WiFiMgr::onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
#endif
    // DEBUG_START;

    pCurrentFsmState->OnDisconnect ();

    // DEBUG_END;

} // onWiFiDisconnect

//-----------------------------------------------------------------------------
int c_WiFiMgr::ValidateConfig (config_t* NewConfig)
{
    // DEBUG_START;

    int response = 0;

    if (!NewConfig->ssid.length ())
    {
        NewConfig->ssid = ssid;
     // DEBUG_V ();
        response++;
    }

    if (!NewConfig->passphrase.length ())
    {
        NewConfig->passphrase = passphrase;
     // DEBUG_V ();
        response++;
    }

    if (NewConfig->sta_timeout < 5)
    {
        NewConfig->sta_timeout = CLIENT_TIMEOUT;
     // DEBUG_V ();
        response++;
    }

    if (NewConfig->ap_timeout < 15)
    {
        NewConfig->ap_timeout = AP_TIMEOUT;
     // DEBUG_V ();
        response++;
    }

    // DEBUG_END;

    return response;

} // ValidateConfig

//-----------------------------------------------------------------------------
void c_WiFiMgr::AnnounceState ()
{
    // DEBUG_START;

    String StateName;
    pCurrentFsmState->GetStateName (StateName);
    LOG_PORT.println (String (F ("\nWiFi Entering State: ")) + StateName);

    // DEBUG_END;

} // AnnounceState

//-----------------------------------------------------------------------------
void c_WiFiMgr::Poll ()
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
void fsm_WiFi_state_Boot::Poll ()
{
    // DEBUG_START;

    // Start trying to connect to the AP
    fsm_WiFi_state_ConnectingUsingConfig_imp.Init ();

    // DEBUG_END;
} // fsm_WiFi_state_boot

/*****************************************************************************/
// Waiting for polling to start
void fsm_WiFi_state_Boot::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState( this );

    // This can get called before the system is up and running.
    // No log port available yet
    // WiFiMgr.AnnounceState ();

    // DEBUG_END;
} // fsm_WiFi_state_Boot::Init

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingConfig::Poll ()
{
    // DEBUG_START;

    // wait for the connection to complete via the callback function
    uint32_t CurrentTimeMS = millis ();

    if (WiFi.status () != WL_CONNECTED)
    {
        if (CurrentTimeMS - WiFiMgr.GetFsmStartTime() > (1000 * WiFiMgr.GetConfigPtr()->sta_timeout))
        {
            LOG_PORT.println (F ("\nWiFi Failed to connect using Configured Credentials"));
            fsm_WiFi_state_ConnectingDefault_imp.Init ();
        }
    }

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingConfig::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingConfig::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();
    WiFiMgr.SetFsmStartTime (millis ());

    // Switch to station mode and disconnect just in case
    WiFi.mode (WIFI_STA);
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP8266
    WiFi.disconnect ();
#else
    WiFi.persistent (false);
    // DEBUG_V ("");
    WiFi.disconnect (true);
#endif
    // DEBUG_V ("");

    WiFiMgr.connectWifi ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectingUsingConfig::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingConfig::OnConnect ()
{
    // DEBUG_START;

    fsm_WiFi_state_ConnectedToAP_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectingUsingConfig::OnConnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingDefaults::Poll ()
{
    // DEBUG_START;

    // wait for the connection to complete via the callback function
    uint32_t CurrentTimeMS = millis ();

    if (WiFi.status () != WL_CONNECTED)
    {
        if (CurrentTimeMS - WiFiMgr.GetFsmStartTime () > (1000 * WiFiMgr.GetConfigPtr ()->sta_timeout))
        {
            LOG_PORT.println (F ("\nWiFi Failed to connect using default Credentials"));
            fsm_WiFi_state_ConnectingAsAP_imp.Init ();
        }
    }

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingDefaults::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingDefaults::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();
    WiFiMgr.SetFsmStartTime (millis ());

    // Switch to station mode and disconnect just in case
    // DEBUG_V ("");

    config_t* config = WiFiMgr.GetConfigPtr ();

    // DEBUG_V (String ("ssid: ") + String (ssid));
    // DEBUG_V (String ("passphrase: ") + String (passphrase));
    config->ssid = ssid;
    config->passphrase = passphrase;

    WiFiMgr.connectWifi ();

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingDefaults::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingDefaults::OnConnect ()
{
    // DEBUG_START;

    fsm_WiFi_state_ConnectedToAP_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectingUsingDefaults::OnConnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingAsAP::Poll ()
{
    // DEBUG_START;

    if (0 != WiFi.softAPgetStationNum ())
    {
        fsm_WiFi_state_ConnectedToSta_imp.Init();
    }
    else
    {
        LOG_PORT.print (".");

        if (millis () - WiFiMgr.GetFsmStartTime () > (1000 * WiFiMgr.GetConfigPtr ()->ap_timeout))
        {
            LOG_PORT.println (F ("\nWiFi STA Failed to connect"));
            fsm_WiFi_state_ConnectionFailed_imp.Init ();
        }
    }

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingAsAP::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingAsAP::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();

    if (true == WiFiMgr.GetConfigPtr ()->ap_fallbackIsEnabled)
    {
        WiFi.mode (WIFI_AP);

        String ssid = "ESPixelStick " + String (WiFiMgr.GetConfigPtr ()->hostname);
        WiFi.softAP (ssid.c_str ());

        IPAddress CurrentIpAddress = WiFi.softAPIP ();
        IPAddress CurrentSubnetMask = IPAddress (255, 255, 255, 0);

        WiFiMgr.setIpAddress (CurrentIpAddress);
        WiFiMgr.setIpSubNetMask (CurrentSubnetMask);

        LOG_PORT.println (String (F ("WiFi SOFTAP: IP Address: '")) + CurrentIpAddress.toString ());
    }
    else
    {
        LOG_PORT.println (String (F ("WiFi SOFTAP: Not enabled")));
        fsm_WiFi_state_ConnectionFailed_imp.Init ();
    }

    // DEBUG_END;

} // fsm_WiFi_state_ConnectingAsAP::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingAsAP::OnConnect ()
{
 // DEBUG_START;

    fsm_WiFi_state_ConnectedToSta_imp.Init ();

 // DEBUG_END;

} // fsm_WiFi_state_ConnectingAsAP::OnConnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::Poll ()
{
    // DEBUG_START;

    // did we get silently disconnected?
    if (WiFi.status () != WL_CONNECTED)
    {
     // DEBUG_V ("WiFi Handle Silent Disconnect");
        WiFi.reconnect ();
    }

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingAsAP::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();

    WiFiMgr.SetUpIp ();

    config_t * config = WiFiMgr.GetConfigPtr ();
    IPAddress localIp = WiFi.localIP ();
    config->ip = localIp;
    config->netmask = WiFi.subnetMask ();
    config->gateway = WiFi.gatewayIP ();

    // DEBUG_V (String ("localIp: ") + localIp.toString ());
    LOG_PORT.println (String (F ("WiFi Connected with IP: ")) + localIp.toString ());

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingAsAP::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::OnDisconnect ()
{
    // DEBUG_START;

    LOG_PORT.println (F ("*** WiFi Disconnected ***"));
    fsm_WiFi_state_ConnectionFailed_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectedToAP::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::Poll ()
{
   // DEBUG_START;

    // did we get silently disconnected?
    if (0 == WiFi.softAPgetStationNum ())
    {
        fsm_WiFi_state_ConnectionFailed_imp.Init ();
    }

    // DEBUG_END;
} // fsm_WiFi_state_ConnectedToSta::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::Init ()
{
    // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();

    WiFiMgr.SetUpIp ();

    IPAddress CurrentIpAddress = WiFi.softAPIP ();
    IPAddress CurrentSubnetMask = WiFi.subnetMask ();
    LOG_PORT.println (String (F ("\nWiFi Connected to STA with IP: ")) + CurrentIpAddress.toString ());

    WiFiMgr.setIpAddress (CurrentIpAddress);
    WiFiMgr.setIpSubNetMask (CurrentSubnetMask);

    // DEBUG_END;
} // fsm_WiFi_state_ConnectedToSta::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::OnDisconnect ()
{
    // DEBUG_START;

    LOG_PORT.println (F ("WiFi STA Disconnected"));
    fsm_WiFi_state_ConnectionFailed_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectedToSta::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectionFailed::Init ()
{
 // DEBUG_START;

    WiFiMgr.SetFsmState (this);
    WiFiMgr.AnnounceState ();

    if (true == WiFiMgr.GetConfigPtr ()->RebootOnWiFiFailureToConnect)
    {
        extern bool reboot;
        LOG_PORT.println (F ("WiFi Requesting Reboot"));

        reboot = true;
    }
    else
    {
        LOG_PORT.println (F ("WiFi Reboot Disable."));

        // start over
        fsm_WiFi_state_Boot_imp.Init ();
    }

 // DEBUG_END;

} // fsm_WiFi_state_ConnectionFailed::Init
//-----------------------------------------------------------------------------

// create a global instance of the WiFi Manager
c_WiFiMgr WiFiMgr;
