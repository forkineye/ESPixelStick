/*
* WiFiMgr.cpp - Output Management class
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

#include "ESPixelStick.h"

#ifdef ARDUINO_ARCH_ESP8266
#   include <eagle_soc.h>
#   include <ets_sys.h>
#else
#   include <esp_wifi.h>
#endif // def ARDUINO_ARCH_ESP8266

#include "WiFiMgr.hpp"
#include "input/InputMgr.hpp"
#include "WebMgr.hpp"
#include "service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
// Create secrets.h with a #define for SECRETS_SSID and SECRETS_PASS
// or delete the #include and enter the strings directly below.
#include "secrets.h"
#ifndef SECRETS_SSID
#   define SECRETS_SSID "DEFAULT_SSID_NOT_SET"
#   define SECRETS_PASS "DEFAULT_PASSPHRASE_NOT_SET"
#endif // ndef SECRETS_SSID

/* Fallback configuration if config->json is empty or fails */
const String ssid       = SECRETS_SSID;
const String passphrase = SECRETS_PASS;

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

    // DEBUG_V ("");

    // Setup WiFi Handlers
#ifdef ARDUINO_ARCH_ESP8266
    wifiConnectHandler    = WiFi.onStationModeGotIP        ([this](const WiFiEventStationModeGotIP& event) {this->onWiFiConnect (event); });
    wifiDisconnectHandler = WiFi.onStationModeDisconnected ([this](const WiFiEventStationModeDisconnected& event) {this->onWiFiDisconnect (event); });
#else
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onWiFiStaConn (event, info); }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onWiFiStaDisc (event, info); }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onWiFiConnect    (event, info);}, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onWiFiDisconnect (event, info);}, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
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

    jsonStatus[CN_rssi]   = WiFi.RSSI ();
    jsonStatus[CN_ip]     = getIpAddress ().toString ();
    jsonStatus[CN_subnet] = getIpSubNetMask ().toString ();
    jsonStatus[CN_mac]    = WiFi.macAddress ();
#ifdef ARDUINO_ARCH_ESP8266
    jsonStatus[CN_hostname] = WiFi.hostname ();
#else
    jsonStatus[CN_hostname] = WiFi.getHostname ();
#endif // def ARDUINO_ARCH_ESP8266
    jsonStatus[CN_ssid]       = WiFi.SSID ();

 // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_WiFiMgr::connectWifi (const String & ssid, const String & passphrase)
{
    // DEBUG_START;

    // WiFi reset flag is set which will be handled in the next iteration of the main loop.
    // Ignore connect request to lessen boot spam.
    if (ResetWiFi)
        return;

    // disconnect just in case
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.disconnect ();
#else
    WiFi.persistent (false);
    // DEBUG_V ("");
    WiFi.disconnect (true);
#endif
    // DEBUG_V ("");

    // DEBUG_V (String ("config->hostname: ") + config->hostname);
    if (0 != config->hostname.length ())
    {
        // DEBUG_V (String ("Setting WiFi hostname: ") + config->hostname);
        WiFi.hostname (config->hostname);
    }

    // Switch to station mode
    WiFi.mode (WIFI_STA);
    // DEBUG_V ("");

    NewLogToCon (String(F ("\nWiFi Connecting to '")) +
                      ssid +
                      String (F ("' as ")) +
                      config->hostname);

    WiFi.begin (ssid.c_str (), passphrase.c_str ());

    // DEBUG_END;
} // connectWifi

//-----------------------------------------------------------------------------
void c_WiFiMgr::reset ()
{
    // DEBUG_START;

    // The WiFi states announce what they're doing, cleans up boot log a bit.
    //NewLogToCon (F ("WiFi Reset has been requested"));

    fsm_WiFi_state_Boot_imp.Init ();
    if (IsWiFiConnected())
    {
        InputMgr.NetworkStateChanged (false);
    }

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
            NewLogToCon (F ("WiFi Connected with DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());
        // DEBUG_V ("     ip: " + config->ip.toString());
        // DEBUG_V ("netmask: " + config->netmask.toString ());
        // DEBUG_V ("gateway: " + config->gateway.toString ());

        if (temp == config->ip)
        {
            NewLogToCon (F ("WiFI: ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            break;
        }

        if ((config->ip      == WiFi.localIP ())    &&
            (config->netmask == WiFi.subnetMask ()) &&
            (config->gateway == WiFi.gatewayIP ()))
        {
            // correct IP is already set
            break;
        }
        // We didn't use DNS, so just set it to our configured gateway
        WiFi.config (config->ip, config->gateway, config->netmask, config->gateway);

        NewLogToCon (F ("Connected with Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP32

//-----------------------------------------------------------------------------
void c_WiFiMgr::onWiFiStaConn (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_V ("ESP has associated with the AP");
} // onWiFiStaConn

//-----------------------------------------------------------------------------
void c_WiFiMgr::onWiFiStaDisc (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_V ("ESP has disconnected from the AP");
} // onWiFiStaDisc

#endif // def ARDUINO_ARCH_ESP32

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

    if (0 == NewConfig->ssid.length ())
    {
        NewConfig->ssid = ssid;
        // DEBUG_V ();
        response++;
    }

    if (0 == NewConfig->passphrase.length ())
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
    NewLogToCon (String (F ("\nWiFi Entering State: ")) + StateName);

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
            NewLogToCon (F ("\nWiFi Failed to connect using Configured Credentials"));
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

    if ((0 == config.ssid.length ()) || (String("null") == config.ssid))
    {
        fsm_WiFi_state_ConnectingDefault_imp.Init ();
    }
    else
    {
        WiFiMgr.SetFsmState (this);
        WiFiMgr.AnnounceState ();
        WiFiMgr.SetFsmStartTime (millis ());

        WiFiMgr.connectWifi (config.ssid, config.passphrase);
    }

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
            NewLogToCon (F ("\nWiFi Failed to connect using default Credentials"));
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

    WiFiMgr.connectWifi (ssid, passphrase);

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
            NewLogToCon (F ("\nWiFi STA Failed to connect"));
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

        WiFiMgr.setIpAddress (WiFi.localIP ());
        WiFiMgr.setIpSubNetMask (WiFi.subnetMask ());

        NewLogToCon (String (F ("WiFi SOFTAP: IP Address: '")) + WiFiMgr.getIpAddress().toString ());
    }
    else
    {
        NewLogToCon (String (F ("WiFi SOFTAP: Not enabled")));
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

    WiFiMgr.setIpAddress( WiFi.localIP () );
    WiFiMgr.setIpSubNetMask( WiFi.subnetMask () );

    NewLogToCon (String (F ("WiFi Connected with IP: ")) + WiFiMgr.getIpAddress ().toString ());

    WiFiMgr.SetIsWiFiConnected (true);
    InputMgr.NetworkStateChanged (true);
    WebMgr.NetworkStateChanged (true);
    FPPDiscovery.NetworkStateChanged (true);

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingAsAP::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::OnDisconnect ()
{
    // DEBUG_START;

    NewLogToCon (F ("WiFi Lost the connection to the AP"));
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
        NewLogToCon (F ("WiFi Lost the connection to the STA"));
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

    WiFiMgr.setIpAddress (WiFi.softAPIP ());
    WiFiMgr.setIpSubNetMask (IPAddress (255, 255, 255, 0));

    NewLogToCon (String (F ("\nWiFi Connected to STA with IP: ")) + WiFiMgr.getIpAddress ().toString ());

    WiFiMgr.SetIsWiFiConnected (true);
    InputMgr.NetworkStateChanged (true);
    WebMgr.NetworkStateChanged (true);

    // DEBUG_END;
} // fsm_WiFi_state_ConnectedToSta::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::OnDisconnect ()
{
    // DEBUG_START;

    NewLogToCon (F ("WiFi STA Disconnected"));
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

    if (WiFiMgr.IsWiFiConnected())
    {
        WiFiMgr.SetIsWiFiConnected (false);
        InputMgr.NetworkStateChanged (false);
    }
    else
    {
        if (true == WiFiMgr.GetConfigPtr ()->RebootOnWiFiFailureToConnect)
        {
            extern bool reboot;
            NewLogToCon (F ("WiFi Requesting Reboot"));

            reboot = true;
        }
        else
        {
            NewLogToCon (F ("WiFi Reboot Disabled."));

            // start over
            fsm_WiFi_state_Boot_imp.Init ();
        }
    }

 // DEBUG_END;

} // fsm_WiFi_state_ConnectionFailed::Init
//-----------------------------------------------------------------------------

// create a global instance of the WiFi Manager
c_WiFiMgr WiFiMgr;
