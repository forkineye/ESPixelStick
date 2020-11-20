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
#include "secrets.h"
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

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_WiFiMgr::c_WiFiMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
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
    wifiConnectHandler = WiFi.onStationModeGotIP ([this](const WiFiEventStationModeGotIP& event) {this->onWiFiConnect (event); });
#else
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiConnect    (event, info);}, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiDisconnect (event, info);}, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
#endif

    //TODO: Setup MQTT / Auxiliary service Handlers?

    // Fallback to default SSID and passphrase if we fail to connect
    initWifi ();

    if (WiFi.status () != WL_CONNECTED)
    {
        LOG_PORT.println (F ("*** Timeout - Reverting to default SSID ***"));
        config->ssid = ssid;
        config->passphrase = passphrase;
        initWifi ();
    }
    // DEBUG_V ("");

    // If we fail again, go SoftAP or reboot
    if (WiFi.status () != WL_CONNECTED)
    {
        // config->ap_fallbackIsEnabled = true;
        if (config->ap_fallbackIsEnabled)
        {
            LOG_PORT.println (F ("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode (WIFI_AP);
            String ssid = "ESPixelStick " + String (config->hostname);
            WiFi.softAP (ssid.c_str ());
            CurrentIpAddress = WiFi.softAPIP ();
            CurrentSubnetMask = IPAddress (255, 255, 255, 0);
            LOG_PORT.println (String(F ("*** SOFTAP: IP Address: '")) + CurrentIpAddress.toString() + F("' ***"));
        }
        else
        {
            LOG_PORT.println (F ("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart ();
        }
    }
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP8266
    wifiDisconnectHandler = WiFi.onStationModeDisconnected ([this](const WiFiEventStationModeDisconnected& event) {this->onWiFiDisconnect (event); });

    // Handle OTA update from asynchronous callbacks
    Update.runAsync (true);
#else
    // not needed for ESP32
#endif
   //  DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_WiFiMgr::GetStatus (JsonObject & jsonStatus)
{
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

} // GetStatus

//-----------------------------------------------------------------------------
void c_WiFiMgr::initWifi ()
{
    // DEBUG_START;
    // Switch to station mode and disconnect just in case
    WiFi.mode (WIFI_STA);
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.disconnect ();
#else
    WiFi.persistent (false);
    WiFi.disconnect (true);
#endif

    connectWifi ();

    // wait for the connection to complete via the callback function
    uint32_t WaitForWiFiStartTime = millis ();
    while (WiFi.status () != WL_CONNECTED)
    {
        // DEBUG_V ("");
        LOG_PORT.print (".");
        delay (500);
        if (millis () - WaitForWiFiStartTime > (1000 * config->sta_timeout))
        {
            LOG_PORT.println (F ("\n*** Failed to connect ***"));
            break;
        }
    }
    // DEBUG_END;
} // initWifi

//-----------------------------------------------------------------------------
void c_WiFiMgr::connectWifi ()
{
#ifdef ARDUINO_ARCH_ESP8266
    delay (secureRandom (100, 500));
#else
    delay (random (100, 500));
#endif
    LOG_PORT.println ("");
    LOG_PORT.print (F ("Connecting to "));
    LOG_PORT.print (config->ssid);
    LOG_PORT.print (F (" as "));
    LOG_PORT.println (config->hostname);

    WiFi.begin (config->ssid.c_str (), config->passphrase.c_str ());
} // connectWifi

//-----------------------------------------------------------------------------
void c_WiFiMgr::reset ()
{
    initWifi ();
} // reset

//-----------------------------------------------------------------------------
void c_WiFiMgr::SetUpIp ()
{
    // DEBUG_START;

    do // once
    {
        if (true == config->UseDhcp)
        {
            LOG_PORT.println (F ("Connected with DHCP"));
            break;
        }

        IPAddress temp = (uint32_t)0;
        // DEBUG_V ("   temp: " + temp.toString ());
        // DEBUG_V ("     ip: " + config->ip.toString());
        // DEBUG_V ("netmask: " + config->netmask.toString ());
        // DEBUG_V ("gateway: " + config->gateway.toString ());

        if (temp == config->ip)
        {
            LOG_PORT.println (F ("** ERROR - STATIC SELECTED WITHOUT IP. Using DHCP assigned address **"));
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

    SetUpIp ();

    CurrentIpAddress  = WiFi.localIP ();
    CurrentSubnetMask = WiFi.subnetMask ();
    LOG_PORT.println (String(F("\nConnected with IP: ")) + CurrentIpAddress.toString ());

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
    LOG_PORT.println (F ("*** WiFi Disconnected ***"));
    wifiTicker.once (2, [] { WiFiMgr.connectWifi (); });
} // onWiFiDisconnect

//-----------------------------------------------------------------------------
int c_WiFiMgr::ValidateConfig (config_t* NewConfig)
{
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

    return response;

} // ValidateConfig

//-----------------------------------------------------------------------------
// create a global instance of the output channel factory
c_WiFiMgr WiFiMgr;
