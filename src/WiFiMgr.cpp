/*
* WiFiMgr.cpp - Output Management class
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
#include <WiFi.h>
#include <esp_wifi.h>
#include "ESPixelStick.h"
#include "WiFiMgr.hpp"
#include "memdebug.h"


//-----------------------------------------------------------------------------
// Create secrets.h with a #define for SECRETS_SSID and SECRETS_PASS
// or delete the #include and enter the strings directly below.
// #include "secrets.h"

/* Fallback configuration if config->json is empty or fails */
const char ssid[] = "MaRtInG";
const char passphrase[] = "martinshomenetwork";

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
    DEBUG_START;

    DEBUG_END;

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
    wifiConnectHandler = WiFi.onStationModeGotIP (onWiFiConnect);
#else
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiConnect (event, info);}, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent ([this](WiFiEvent_t event, system_event_info_t info) {this->onWiFiDisconnect (event, info); }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
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
        if (config->ap_fallback)
        {
            LOG_PORT.println (F ("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode (WIFI_AP);
            String ssid = "ESPixelStick " + String (config->hostname);
            WiFi.softAP (ssid.c_str ());
            CurrentIpAddress = WiFi.softAPIP ();
            CurrentSubnetMask = IPAddress (255, 255, 255, 0);
        }
        else
        {
            LOG_PORT.println (F ("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart ();
        }
    }
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP8266
    wifiDisconnectHandler = WiFi.onStationModeDisconnected (onWiFiDisconnect);

    // Handle OTA update from asynchronous callbacks
    Update.runAsync (true);
#else
    // not needed for ESP32
#endif
   //  DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_WiFiMgr::initWifi ()
{
    // Switch to station mode and disconnect just in case
    WiFi.mode (WIFI_STA);
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.disconnect ();
#else
    WiFi.persistent (false);
    WiFi.disconnect (true);
#endif

    connectWifi ();
    uint32_t timeout = millis ();
    while (WiFi.status () != WL_CONNECTED)
    {
        LOG_PORT.print (".");
        delay (500);
        if (millis () - timeout > (1000 * config->sta_timeout))
        {
            LOG_PORT.println ("");
            LOG_PORT.println (F ("*** Failed to connect ***"));
            break;
        }
    }
}

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
    if (config->dhcp)
    {
        LOG_PORT.print (F ("Connecting with DHCP"));
    }
    else
    {
        // We don't use DNS, so just set it to our gateway
        if (!config->ip.isEmpty ())
        {
            IPAddress ip = ip.fromString (config->ip);
            IPAddress gateway = gateway.fromString (config->gateway);
            IPAddress netmask = netmask.fromString (config->netmask);
            WiFi.config (ip, gateway, netmask, gateway);
            LOG_PORT.print (F ("Connecting with Static IP"));
        }
        else
        {
            LOG_PORT.println (F ("** ERROR - STATIC SELECTED WITHOUT IP **"));
        }
    }
}

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP8266
void c_WiFiMgr::onWiFiConnect (const WiFiEventStationModeGotIP& event)
{
#else
void c_WiFiMgr::onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
#endif
    CurrentIpAddress = WiFi.localIP ();
    CurrentSubnetMask = WiFi.subnetMask ();
    LOG_PORT.printf ("\nConnected with IP: %s\n", CurrentIpAddress.toString ().c_str ());

    // Setup MQTT connection if enabled

    // Setup mDNS / DNS-SD
    //TODO: Reboot or restart mdns when config->id is changed?
/*
#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = String ((unsigned long)ESP.getEfuseMac (), HEX);
#endif
    MDNS.setInstanceName(String(config->id + " (" + chipId + ")").c_str());
    if (MDNS.begin(config->hostname.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
//        MDNS.addService("zcpp", "udp", ZCPP_PORT);
//        MDNS.addService("ddp", "udp", DDP_PORT);
        MDNS.addService("e131", "udp", E131_DEFAULT_PORT);
        MDNS.addServiceTxt("e131", "udp", "TxtVers", String(RDMNET_DNSSD_TXTVERS));
        MDNS.addServiceTxt("e131", "udp", "ConfScope", RDMNET_DEFAULT_SCOPE);
        MDNS.addServiceTxt("e131", "udp", "E133Vers", String(RDMNET_DNSSD_E133VERS));
        MDNS.addServiceTxt("e131", "udp", "CID", chipId);
        MDNS.addServiceTxt("e131", "udp", "Model", "ESPixelStick");
        MDNS.addServiceTxt("e131", "udp", "Manuf", "Forkineye");
    } else {
        LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
    }
*/
}

//-----------------------------------------------------------------------------
static void connectWifiTicker ()
{
    WiFiMgr.connectWifi ();
} // connectWifiTick
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
    // wifiTicker.once (2, connectWifi);
    wifiTicker.once (2, connectWifiTicker);
}

//-----------------------------------------------------------------------------
void c_WiFiMgr::ValidateConfig (config_t* NewConfig)
{
    if (!NewConfig->ssid.length ())
        NewConfig->ssid = ssid;

    if (!NewConfig->passphrase.length ())
        NewConfig->passphrase = passphrase;

    if (NewConfig->sta_timeout < 5)
        NewConfig->sta_timeout = CLIENT_TIMEOUT;

    if (NewConfig->ap_timeout < 15)
        NewConfig->ap_timeout = AP_TIMEOUT;

} // ValidateConfig

//-----------------------------------------------------------------------------
// create a global instance of the output channel factory
c_WiFiMgr WiFiMgr;

