/*
* WiFiDriver.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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

#include "network/WiFiDriver.hpp"
#include "network/NetworkMgr.hpp"
#include "FileMgr.hpp"

//-----------------------------------------------------------------------------
/*
    There are three ways to define the default Network Name and PassPhrase

    1) Create a secrets.h file and place it in the network directory with the WiFiDriver.cpp file
    2) Use platformio_user.ini to define the WiFi Credentials for your platform
    3) Edit the strings below directly
*/

#if __has_include("secrets.h")
#   include "secrets.h"
#endif //  __has_include("secrets.h")

#if !defined(SECRETS_SSID)
#   define SECRETS_SSID DEFAULT_SSID_NOT_SET
#endif // SECRETS_SSID
#if !defined(SECRETS_PASS)
#   define SECRETS_PASS "DEFAULT_PASSPHRASE_NOT_SET"
#endif // SECRETS_PASS

/* Fallback configuration if config.json is empty or fails */
const String default_ssid       = SECRETS_SSID;
const String default_passphrase = SECRETS_PASS;

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
fsm_WiFi_state_ConnectingUsingDefaults fsm_WiFi_state_ConnectingUsingDefaults_imp;
fsm_WiFi_state_ConnectedToAP           fsm_WiFi_state_ConnectedToAP_imp;
fsm_WiFi_state_ConnectingAsAP          fsm_WiFi_state_ConnectingAsAP_imp;
fsm_WiFi_state_ConnectedToSta          fsm_WiFi_state_ConnectedToSta_imp;
fsm_WiFi_state_ConnectionFailed        fsm_WiFi_state_ConnectionFailed_imp;
fsm_WiFi_state_Disabled                fsm_WiFi_state_Disabled_imp;

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_WiFiDriver::c_WiFiDriver ()
{
    fsm_WiFi_state_Boot_imp.SetParent (this);
    fsm_WiFi_state_ConnectingUsingConfig_imp.SetParent (this);
    fsm_WiFi_state_ConnectingUsingDefaults_imp.SetParent (this);
    fsm_WiFi_state_ConnectedToAP_imp.SetParent (this);
    fsm_WiFi_state_ConnectingAsAP_imp.SetParent (this);
    fsm_WiFi_state_ConnectedToSta_imp.SetParent (this);
    fsm_WiFi_state_ConnectionFailed_imp.SetParent (this);
    fsm_WiFi_state_Disabled_imp.SetParent (this);

    fsm_WiFi_state_Boot_imp.Init ();
} // c_WiFiDriver

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_WiFiDriver::~c_WiFiDriver()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_WiFiDriver

//-----------------------------------------------------------------------------
void c_WiFiDriver::AnnounceState ()
{
    // DEBUG_START;

    String StateName;
    pCurrentFsmState->GetStateName (StateName);
    logcon (String (F ("WiFi Entering State: ")) + StateName);

    // DEBUG_END;

} // AnnounceState

//-----------------------------------------------------------------------------
///< Start the module
void c_WiFiDriver::Begin ()
{
    // DEBUG_START;

    // save the pointer to the config
    // config = NewConfig;

    if (FileMgr.SdCardIsInstalled())
    {
        JsonDocument jsonConfigDoc;
        // DEBUG_V ("read the sdcard config");
        if (FileMgr.ReadSdFile (F("wificonfig.json"), jsonConfigDoc))
        {
            // DEBUG_V ("Process the sdcard config");
            JsonObject jsonConfig = jsonConfigDoc.as<JsonObject> ();

            // copy the fields of interest into the local structure
            setFromJSON (ssid,         jsonConfig, CN_ssid);
            setFromJSON (passphrase,   jsonConfig, CN_passphrase);
            setFromJSON (ap_ssid,       jsonConfig, CN_ap_ssid);
            setFromJSON (ap_passphrase, jsonConfig, CN_ap_passphrase);

            ConfigSaveNeeded = true;

            FileMgr.DeleteSdFile (F ("wificonfig.json"));
        }
        else
        {
            // DEBUG_V ("ERROR: Could not read SD card config");
        }
    }

    // Disable persistant credential storage and configure SDK params
    WiFi.persistent (false);

    if(ap_ssid.equals(emptyString))
    {
        String Hostname;
        NetworkMgr.GetHostname (Hostname);
        ap_ssid = "ESPixelStick-AP-" + String (Hostname);
    }

#ifdef ARDUINO_ARCH_ESP8266
    wifi_set_sleep_type (NONE_SLEEP_T);
    // https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html#setoutputpower
    // https://github.com/esp8266/Arduino/issues/6366
    // AI Thinker FCC certification performed at 17dBm
    WiFi.setOutputPower (16);

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
    NextPoll.StartTimer(PollInterval, false);

    // Main loop should start polling for us
    // pCurrentFsmState->Poll ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_WiFiDriver::connectWifi (const String & current_ssid, const String & current_passphrase)
{
    // DEBUG_START;

    do // once
    {
        // WiFi reset flag is set which will be handled in the next iteration of the main loop.
        // Ignore connect request to lessen boot spam.
        if (ResetWiFi)
        {
            // DEBUG_V ("WiFi Reset Requested");
            break;
        }

        SetUpIp();

        String Hostname;
        NetworkMgr.GetHostname(Hostname);
        // DEBUG_V(String("Hostname: ") + Hostname);

        // Hostname must be set after the mode on ESP8266 and before on ESP32
#ifdef ARDUINO_ARCH_ESP8266
        WiFi.disconnect();
        // DEBUG_V("");

        // Switch to station mode
        WiFi.mode(WIFI_STA);
        // DEBUG_V ("");

        if (0 != Hostname.length())
        {
            // DEBUG_V (String ("Setting WiFi hostname: ") + Hostname);
            WiFi.hostname(Hostname);
        }
        // DEBUG_V("");
#else
        WiFi.persistent(false);
        // DEBUG_V("");
        WiFi.disconnect(true);
        // DEBUG_V("");

        if (0 != Hostname.length())
        {
            // DEBUG_V(String("Setting WiFi hostname: ") + Hostname);
            WiFi.hostname(Hostname);
        }

        // DEBUG_V("Setting WiFi Mode to STA");
        WiFi.enableAP(false);
        // DEBUG_V();
        WiFi.enableSTA(true);

#endif
        // DEBUG_V (String ("      ssid: ") + current_ssid);
        // DEBUG_V (String ("passphrase: ") + current_passphrase);
        // DEBUG_V (String ("  hostname: ") + Hostname);

    	logcon (String(F ("Connecting to '")) +
               current_ssid +
               String(F("' as ")) +
               Hostname);

        WiFi.setSleep(false);
        // DEBUG_V("");
        WiFi.begin(current_ssid.c_str(), current_passphrase.c_str());
    } while (false);

    // DEBUG_END;

} // connectWifi

//-----------------------------------------------------------------------------
void c_WiFiDriver::Disable ()
{
    // DEBUG_START;

    if (pCurrentFsmState != &fsm_WiFi_state_Disabled_imp)
    {
        // DEBUG_V ();
        WiFi.enableSTA (false);
        WiFi.enableAP (false);
        fsm_WiFi_state_Disabled_imp.Init ();
    }

    // DEBUG_END;

} // Disable

//-----------------------------------------------------------------------------
void c_WiFiDriver::Enable ()
{
    // DEBUG_START;

    if (pCurrentFsmState == &fsm_WiFi_state_Disabled_imp)
    {
        // DEBUG_V ();
        WiFi.enableSTA (true);
        WiFi.enableAP (false);
        fsm_WiFi_state_ConnectionFailed_imp.Init ();
    }
    else
    {
        // DEBUG_V (String ("WiFi is not disabled"));
    }

    // DEBUG_END;
} // Enable

//-----------------------------------------------------------------------------
void c_WiFiDriver::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    JsonWrite(json, CN_ssid,          ssid);
    JsonWrite(json, CN_passphrase,    passphrase);
    JsonWrite(json, CN_ap_ssid,       ap_ssid);
    JsonWrite(json, CN_ap_passphrase, ap_passphrase);

#ifdef ARDUINO_ARCH_ESP8266
    IPAddress Temp = ip;
    JsonWrite(json, CN_ip,           Temp.toString ());
    Temp = netmask;
    JsonWrite(json, CN_netmask,      Temp.toString ());
    Temp = gateway;
    JsonWrite(json, CN_gateway,      Temp.toString ());
    Temp = primaryDns;
    JsonWrite(json, CN_dnsp,         Temp.toString ());
    Temp = secondaryDns;
    JsonWrite(json, CN_dnss,         Temp.toString ());
#else
    JsonWrite(json, CN_ip,           ip.toString ());
    JsonWrite(json, CN_netmask,      netmask.toString ());
    JsonWrite(json, CN_gateway,      gateway.toString ());
    JsonWrite(json, CN_dnsp,         primaryDns.toString ());
    JsonWrite(json, CN_dnss,         secondaryDns.toString ());
#endif // !def ARDUINO_ARCH_ESP8266

    JsonWrite(json, CN_StayInApMode, StayInApMode);
    JsonWrite(json, CN_dhcp,         UseDhcp);
    JsonWrite(json, CN_sta_timeout,  sta_timeout);
    JsonWrite(json, CN_ap_channel,   ap_channelNumber);
    JsonWrite(json, CN_ap_fallback,  ap_fallbackIsEnabled);
    JsonWrite(json, CN_ap_timeout,   ap_timeout);
    JsonWrite(json, CN_ap_reboot,    RebootOnWiFiFailureToConnect);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_WiFiDriver::GetHostname (String & name)
{
#ifdef ARDUINO_ARCH_ESP8266
    name = WiFi.hostname ();
#else
    name = WiFi.getHostname ();
#endif // def ARDUINO_ARCH_ESP8266

} // GetWiFiHostName

//-----------------------------------------------------------------------------
void c_WiFiDriver::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;
    String Hostname;
    GetHostname (Hostname);
    JsonWrite(jsonStatus, CN_hostname,  Hostname);
    JsonWrite(jsonStatus, CN_rssi,      WiFi.RSSI ());
    JsonWrite(jsonStatus, CN_ip,        getIpAddress ().toString ());
    JsonWrite(jsonStatus, CN_subnet,    getIpSubNetMask ().toString ());
    JsonWrite(jsonStatus, CN_mac,       WiFi.macAddress ());
    JsonWrite(jsonStatus, CN_ssid,      WiFi.SSID ());
    JsonWrite(jsonStatus, CN_connected, IsWiFiConnected ());

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP32

//-----------------------------------------------------------------------------
void c_WiFiDriver::onWiFiStaConn (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_V ("ESP has associated with the AP");
} // onWiFiStaConn

//-----------------------------------------------------------------------------
void c_WiFiDriver::onWiFiStaDisc (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_V ("ESP has disconnected from the AP");
} // onWiFiStaDisc

#endif // def ARDUINO_ARCH_ESP32

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP8266
void c_WiFiDriver::onWiFiConnect (const WiFiEventStationModeGotIP& event)
{
#else
void c_WiFiDriver::onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
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
void c_WiFiDriver::onWiFiDisconnect (const WiFiEventStationModeDisconnected & event)
{
#else
void c_WiFiDriver::onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
#endif
    // DEBUG_START;

    pCurrentFsmState->OnDisconnect ();

    // DEBUG_END;

} // onWiFiDisconnect

//-----------------------------------------------------------------------------
void c_WiFiDriver::Poll ()
{
    // DEBUG_START;

    if (NextPoll.IsExpired())
    {
        // DEBUG_V ("Start Poll");
        NextPoll.StartTimer(PollInterval, false);
        // displayFsmState ();
        pCurrentFsmState->Poll ();
        // displayFsmState ();
        // DEBUG_V ("End Poll");
    }

    if (ResetWiFi)
    {
        ResetWiFi = false;
        reset ();
    }

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_WiFiDriver::reset ()
{
    // DEBUG_START;

    // Reset address in case we're switching from static to dhcp
    // WiFi.config (0u, 0u, 0u);
    // DEBUG_V("");

    if (IsWiFiConnected ())
    {
        // DEBUG_V("");
        NetworkMgr.SetWiFiIsConnected (false);
    }
    // DEBUG_V("");

    fsm_WiFi_state_Boot_imp.Init ();

    // DEBUG_END;
} // reset

//-----------------------------------------------------------------------------
bool c_WiFiDriver::SetConfig (JsonObject & json)
{
    // DEBUG_START;

    // DEBUG_V(String("ap_channelNumber: ") + String(ap_channelNumber));
    bool ConfigChanged = false;

    String sIp = ip.toString ();
    String sGateway = gateway.toString ();
    String sNetmask = netmask.toString ();
    String sdnsp = primaryDns.toString ();
    String sdnss = secondaryDns.toString ();
    
    ConfigChanged |= setFromJSON (ssid, json, CN_ssid);
    ConfigChanged |= setFromJSON (passphrase, json, CN_passphrase);
    ConfigChanged |= setFromJSON (sIp, json, CN_ip);
    ConfigChanged |= setFromJSON (sNetmask, json, CN_netmask);
    ConfigChanged |= setFromJSON (sGateway, json, CN_gateway);
    ConfigChanged |= setFromJSON (sdnsp, json, CN_dnsp);
    ConfigChanged |= setFromJSON (sdnss, json, CN_dnss);
    ConfigChanged |= setFromJSON (UseDhcp, json, CN_dhcp);
    ConfigChanged |= setFromJSON (ap_ssid, json, CN_ap_ssid);
    ConfigChanged |= setFromJSON (ap_passphrase, json, CN_ap_passphrase);
    ConfigChanged |= setFromJSON (ap_channelNumber, json, CN_ap_channel);
    ConfigChanged |= setFromJSON (sta_timeout, json, CN_sta_timeout);
    ConfigChanged |= setFromJSON (ap_fallbackIsEnabled, json, CN_ap_fallback);
    ConfigChanged |= setFromJSON (ap_timeout, json, CN_ap_timeout);
    ConfigChanged |= setFromJSON (RebootOnWiFiFailureToConnect, json, CN_ap_reboot);
    ConfigChanged |= setFromJSON (StayInApMode, json, CN_StayInApMode);

    // DEBUG_V ("                     ip: " + ip);
    // DEBUG_V (                "gateway: " + gateway);
    // DEBUG_V (                "netmask: " + netmask);
    // DEBUG_V (String("ap_channelNumber: ") + String(ap_channelNumber));
    // String StateName;
    // pCurrentFsmState->GetStateName(StateName);
    // DEBUG_V(String("       CurrState: ") + StateName);
    
    ip.fromString (sIp);
    gateway.fromString (sGateway);
    netmask.fromString (sNetmask);
    primaryDns.fromString (sdnsp);
    secondaryDns.fromString (sdnss);

    if((passphrase.length() < 8) && (passphrase.length() > 0))
    {
        logcon (String (F ("WiFi Passphrase is too short. Using Empty String")));
        passphrase = emptyString;
    }

    if(ConfigChanged)
    {
        // DEBUG_V("WiFi Settings changed");
        if(pCurrentFsmState == &fsm_WiFi_state_ConnectedToAP_imp)
        {
            // DEBUG_V("need to cycle the WiFi to move to new STA settings");
            WiFi.disconnect();
        }
        else if(pCurrentFsmState == &fsm_WiFi_state_ConnectedToSta_imp)
        {
            // DEBUG_V("need to cycle the WiFi to move to new AP settings");
            WiFi.softAPdisconnect();
        }
    }

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_WiFiDriver::SetFsmState (fsm_WiFi_state* NewState)
{
    // DEBUG_START;
    // DEBUG_V (String ("pCurrentFsmState: ") + String (uint32_t (pCurrentFsmState), HEX));
    // DEBUG_V (String ("        NewState: ") + String (uint32_t (NewState), HEX));
    pCurrentFsmState = NewState;
    // displayFsmState ();
    // DEBUG_END;

} // SetFsmState

//-----------------------------------------------------------------------------
void c_WiFiDriver::SetHostname (String & )
{
    // DEBUG_START;

    // Need to reset the WiFi sub system if the host name changes
    reset ();

    // DEBUG_END;
} // SetHostname

//-----------------------------------------------------------------------------
void c_WiFiDriver::SetUpIp ()
{
    // DEBUG_START;

    do // once
    {
        if (true == UseDhcp)
        {
            logcon (F ("Using DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());
        // DEBUG_V ("     ip: " + ip.toString());
        // DEBUG_V ("netmask: " + netmask.toString ());
        // DEBUG_V ("gateway: " + gateway.toString ());

        if (temp == ip)
        {
            logcon (F ("ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            break;
        }

        if ((ip == WiFi.localIP ()) &&
            (netmask == WiFi.subnetMask ()) &&
            (gateway == WiFi.gatewayIP ()))
        {
            // DEBUG_V();
            // correct IP is already set
            break;
        }

        // use gateway if primary DNS is not defined
        if(primaryDns == INADDR_NONE)
        {
            primaryDns = gateway;
        }

        // We didn't use DNS, so just set it to our configured gateway
        WiFi.config (ip, gateway, netmask, primaryDns, secondaryDns);

        logcon (F ("Using Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
int c_WiFiDriver::ValidateConfig ()
{
    // DEBUG_START;

    int response = 0;

    if (0 == ssid.length ())
    {
        ssid = ssid;
        // DEBUG_V ();
        response++;
    }

    if (0 == passphrase.length ())
    {
        passphrase = passphrase;
        // DEBUG_V ();
        response++;
    }

    if (sta_timeout < 5)
    {
        sta_timeout = CLIENT_TIMEOUT;
        // DEBUG_V ();
        response++;
    }

    if (ap_timeout < 15)
    {
        ap_timeout = AP_TIMEOUT;
        // DEBUG_V ();
        response++;
    }

    // DEBUG_END;

    return response;

} // ValidateConfig

/*****************************************************************************/
//  FSM Code
/*****************************************************************************/
/*****************************************************************************/
// Waiting for polling to start
void fsm_WiFi_state_Boot::Poll ()
{
    /// DEBUG_START;

    // Start trying to connect to the AP
    /// DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
    fsm_WiFi_state_ConnectingUsingConfig_imp.Init ();
    // pWiFiDriver->displayFsmState ();

    /// DEBUG_END;
} // fsm_WiFi_state_boot

/*****************************************************************************/
// Waiting for polling to start
void fsm_WiFi_state_Boot::Init ()
{
    // DEBUG_START;

    // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
    pWiFiDriver->SetFsmState (this);

    // This can get called before the system is up and running.
    // No log port available yet
    // pWiFiDriver->AnnounceState ();

    // DEBUG_END;
} // fsm_WiFi_state_Boot::Init

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingConfig::Poll ()
{
    /// DEBUG_START;

    // wait for the connection to complete via the callback function

    if (WiFi.status () != WL_CONNECTED)
    {
        if (pWiFiDriver->GetFsmTimer().IsExpired())
        {
            /// DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
            logcon (F ("WiFi Failed to connect using Configured Credentials"));
            fsm_WiFi_state_ConnectingUsingDefaults_imp.Init ();
        }
    }

    /// DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingConfig::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingConfig::Init ()
{
    // DEBUG_START;
    String CurrentSsid = pWiFiDriver->GetConfig_ssid ();
    String CurrentPassphrase = pWiFiDriver->GetConfig_passphrase ();
    // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));

    if ((0 == CurrentSsid.length ()) || (String("null") == CurrentSsid))
    {
        // DEBUG_V ();
        fsm_WiFi_state_ConnectingUsingDefaults_imp.Init ();
    }
    else
    {
        pWiFiDriver->SetFsmState (this);
        pWiFiDriver->AnnounceState ();
        pWiFiDriver->GetFsmTimer().StartTimer(1000 * pWiFiDriver->Get_sta_timeout(), false);

        pWiFiDriver->connectWifi (CurrentSsid, CurrentPassphrase);
    }
    // pWiFiDriver->displayFsmState ();

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
    /// DEBUG_START;

    // wait for the connection to complete via the callback function
    if (WiFi.status () != WL_CONNECTED)
    {
        if (pWiFiDriver->GetFsmTimer().IsExpired())
        {
            // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
            logcon (F ("WiFi Failed to connect using default Credentials"));
            fsm_WiFi_state_ConnectingAsAP_imp.Init ();
        }
    }

    /// DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingDefaults::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingDefaults::Init ()
{
    // DEBUG_START;

    // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));

    if(!default_ssid.equals(DEFAULT_SSID_NOT_SET))
    {
        pWiFiDriver->SetFsmState (this);
        pWiFiDriver->AnnounceState ();
        pWiFiDriver->GetFsmTimer().StartTimer(1000 * pWiFiDriver->Get_sta_timeout (), false);
        pWiFiDriver->connectWifi (default_ssid, default_passphrase);
    }
    else
    {
        // no defauult ssid was set at compile time. Just move on to AP mode
        fsm_WiFi_state_ConnectingAsAP_imp.Init ();
    }
    // pWiFiDriver->displayFsmState ();

    // DEBUG_END;
} // fsm_WiFi_state_ConnectingUsingDefaults::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingUsingDefaults::OnConnect ()
{
    // DEBUG_START;

    // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
    fsm_WiFi_state_ConnectedToAP_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectingUsingDefaults::OnConnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingAsAP::Poll ()
{
    /// DEBUG_START;

    if (0 != WiFi.softAPgetStationNum ())
    {
        fsm_WiFi_state_ConnectedToSta_imp.Init();
    }
    else
    {
        if (pWiFiDriver->GetFsmTimer().IsExpired())
        {
            if( false == pWiFiDriver->Get_ap_StayInApMode())
            {
                logcon (F ("WiFi STA Failed to connect"));
                fsm_WiFi_state_ConnectionFailed_imp.Init ();
            }
        }
    }

    /// DEBUG_END;
} // fsm_WiFi_state_ConnectingAsAP::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectingAsAP::Init ()
{
    // DEBUG_START;

    pWiFiDriver->SetFsmState (this);
    pWiFiDriver->AnnounceState ();
    pWiFiDriver->GetFsmTimer ().StartTimer(1000 * pWiFiDriver->Get_ap_timeout (), false);

    if (true == pWiFiDriver->Get_ap_fallbackIsEnabled() || pWiFiDriver->Get_ap_StayInApMode())
    {
        WiFi.enableSTA(false);
        WiFi.enableAP(true);
        if(pWiFiDriver->ap_ssid.equals(emptyString))
        {
            String Hostname;
            NetworkMgr.GetHostname (Hostname);
            pWiFiDriver->ap_ssid = "ESPixelStick-" + String (Hostname);
        }
        // DEBUG_V(String("ap_channelNumber: ") + String(pWiFiDriver->ap_channelNumber));
        WiFi.softAP (pWiFiDriver->ap_ssid.c_str (), pWiFiDriver->ap_passphrase.c_str (), int(pWiFiDriver->ap_channelNumber));

        pWiFiDriver->setIpAddress (WiFi.localIP ());
        pWiFiDriver->setIpSubNetMask (WiFi.subnetMask ());

        logcon (String (F ("WiFi SOFTAP:       ssid: '")) + pWiFiDriver->ap_ssid);
        logcon (String (F ("WiFi SOFTAP: IP Address: '")) + pWiFiDriver->getIpAddress ().toString ());
    }
    else
    {
        logcon (String (F ("WiFi SOFTAP: Not enabled")));
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
    /// DEBUG_START;

    // did we get silently disconnected?
    if (WiFi.status () != WL_CONNECTED)
    {
        // DEBUG_V ("WiFi Handle Silent Disconnect");
        WiFi.reconnect ();
    }

    /// DEBUG_END;
} // fsm_WiFi_state_ConnectedToAP::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::Init ()
{
    // DEBUG_START;

    // DEBUG_V (String ("this: ") + String (uint32_t (this), HEX));
    pWiFiDriver->SetFsmState (this);
    pWiFiDriver->AnnounceState ();

    // WiFiDriver.SetUpIp ();

    pWiFiDriver->setIpAddress( WiFi.localIP () );
    pWiFiDriver->setIpSubNetMask( WiFi.subnetMask () );

    logcon (String (F ("Connected with IP: ")) + pWiFiDriver->getIpAddress ().toString ());

    pWiFiDriver->SetIsWiFiConnected (true);
    NetworkMgr.SetWiFiIsConnected (true);

    // DEBUG_END;
} // fsm_WiFi_state_ConnectedToAP::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToAP::OnDisconnect ()
{
    // DEBUG_START;

    logcon (F ("WiFi Lost the connection to the AP"));
    fsm_WiFi_state_ConnectionFailed_imp.Init ();

    // DEBUG_END;

} // fsm_WiFi_state_ConnectedToAP::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::Poll ()
{
    /// DEBUG_START;

    // did we get silently disconnected?
    if (0 == WiFi.softAPgetStationNum ())
    {
        logcon (F ("WiFi Lost the connection to the STA"));
        if(pWiFiDriver->Get_ap_StayInApMode())
        {
            fsm_WiFi_state_ConnectingAsAP_imp.Init ();
        }
        else
        {
            fsm_WiFi_state_ConnectionFailed_imp.Init ();
        }
    }

    /// DEBUG_END;
} // fsm_WiFi_state_ConnectedToSta::Poll

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::Init ()
{
    // DEBUG_START;

    pWiFiDriver->SetFsmState (this);
    pWiFiDriver->AnnounceState ();

    // WiFiDriver.SetUpIp ();

    pWiFiDriver->setIpAddress (WiFi.softAPIP ());
    pWiFiDriver->setIpSubNetMask (IPAddress (255, 255, 255, 0));

    logcon (String (F ("Connected to STA with IP: ")) + pWiFiDriver->getIpAddress ().toString ());

    pWiFiDriver->SetIsWiFiConnected (true);
    NetworkMgr.SetWiFiIsConnected (true);

    // DEBUG_END;
} // fsm_WiFi_state_ConnectedToSta::Init

/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectedToSta::OnDisconnect ()
{
    // DEBUG_START;

    logcon (F ("WiFi STA Disconnected"));
    if(pWiFiDriver->Get_ap_StayInApMode())
    {
        fsm_WiFi_state_ConnectingAsAP_imp.Init ();
    }
    else
    {
        fsm_WiFi_state_ConnectionFailed_imp.Init ();
    }

    // DEBUG_END;

} // fsm_WiFi_state_ConnectedToSta::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_ConnectionFailed::Init ()
{
    // DEBUG_START;

    pWiFiDriver->SetFsmState (this);
    pWiFiDriver->AnnounceState ();

    if (pWiFiDriver->IsWiFiConnected())
    {
        pWiFiDriver->SetIsWiFiConnected (false);
        NetworkMgr.SetWiFiIsConnected (false);
    }

    if (true == pWiFiDriver->Get_RebootOnWiFiFailureToConnect())
    {
        logcon (F ("WiFi Requesting Reboot"));
        RequestReboot(100000);
    }
    else
    {
        // DEBUG_V ("WiFi Reboot Disabled. Try Again");

        // start over
        fsm_WiFi_state_Boot_imp.Init ();
    }

    // DEBUG_END;

} // fsm_WiFi_state_ConnectionFailed::Init

/*****************************************************************************/
/*****************************************************************************/
// Wait for events
void fsm_WiFi_state_Disabled::Init ()
{
    // DEBUG_START;

    pWiFiDriver->SetFsmState (this);
    pWiFiDriver->AnnounceState ();

    if (pWiFiDriver->IsWiFiConnected ())
    {
        pWiFiDriver->SetIsWiFiConnected (false);
        NetworkMgr.SetWiFiIsConnected (false);
    }

    // DEBUG_END;

} // fsm_WiFi_state_Disabled::Init

//-----------------------------------------------------------------------------
