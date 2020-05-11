/*
* ESPixelStick.ino
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2016 Shelby Merrick
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

/*****************************************/
/*        BEGIN - Configuration          */
/*****************************************/
// Create secrets.h with a #define for SECRETS_SSID and SECRETS_PASS
// or delete the #include and enter the strings directly below.
// #include "secrets.h"

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = "MaRtInG";
const char passphrase[] = "martinshomenetwork";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <SPI.h>

// Core
#include "src/ESPixelStick.h"
#include "src/EFUpdate.h"
#include "src/FileIO.h"
#include "src/WebIO.h"

// Input modules
#include "src/input/E131Input.h"
//#include "src/input/ESPAsyncDDP.h"

// Output modules
#include "src/output/OutputMgr.hpp"

// Services
//#include "src/service/MQTT.h"
//#include "src/service/EffectEngine.h"
//#include "src/service/ESPAsyncZCPP.h"
//#include "src/service/FPPDiscovery.h"

#ifdef ARDUINO_ARCH_ESP8266
#include <Hash.h>
extern "C"
{
#   include <user_interface.h>
} // extern "C" 

#elif defined ARDUINO_ARCH_ESP32
    // ESP32 user_interface is now built in
#   include <WiFi.h>
#   include <esp_wifi.h>
#   include <SPIFFS.h>
#   include <Update.h>
#else
#	error "Unsupported CPU type."
#endif

#ifndef WL_MAC_ADDR_LENGTH
#   define WL_MAC_ADDR_LENGTH 6
#endif // WL_MAC_ADDR_LENGTH

#ifndef ICACHE_RAM_ATTR
#   define ICACHE_RAM_ATTR IRAM_ATTR 
#endif // ICACHE_RAM_ATTR

// Debugging support
#if defined(DEBUG)
extern "C" void system_set_os_print(uint8 onoff);
extern "C" void ets_install_putc1(void* routine);

static void _u0_putc(char c){
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
}
#endif

/////////////////////////////////////////////////////////
//
//  Module Maps
//
/////////////////////////////////////////////////////////

/// Map of input modules
std::map<const String, _Input*>::const_iterator itInput;
const std::map<const String, _Input*> INPUT_MODES = {
    { E131Input::KEY, new E131Input() }
};

/////////////////////////////////////////////////////////
//
//  Globals
//
/////////////////////////////////////////////////////////

// Configuration file
const char CONFIG_FILE[] = "/config.json";

// Input and Output modules
_Input  *input;     ///< Pointer to currently enabled input module

uint8_t *showBuffer;        ///< Main show buffer

//TODO: Add Auxiliary services
//MQTT
//effects
//zcpp
//FPPDiscovery

config_t            config;                 // Current configuration
bool                reboot = false;         // Reboot flag
AsyncWebServer      web(HTTP_PORT);         // Web Server
AsyncWebSocket      ws("/ws");              // Web Socket Plugin
uint32_t            lastUpdate;             // Update timeout tracker
#ifdef ARDUINO_ARCH_ESP8266
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
#endif
Ticker              wifiTicker;             // Ticker to handle WiFi
IPAddress           ourLocalIP;
IPAddress           ourSubnetMask;

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
void initWifi();
void initWeb();

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

/// Arduino Setup
/** Arduino based setup code that is executed at startup. */
void setup() 
{
    // Disable persistant credential storage and configure SDK params
    WiFi.persistent(false);
#ifdef ARDUINO_ARCH_ESP8266
    wifi_set_sleep_type(NONE_SLEEP_T);
#elif defined(ARDUINO_ARCH_ESP32)
    esp_wifi_set_ps (WIFI_PS_NONE);
#endif
    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);
    // DEBUG_START;
#if defined(DEBUG)
    ets_install_putc1((void *) &_u0_putc);
    system_set_os_print(1);
#endif

    // Dump version and build information
    LOG_PORT.println("");
    LOG_PORT.print(F("ESPixelStick v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.print(F(" ("));
    for (uint8_t i = 0; i < strlen_P(BUILD_DATE); i++)
        LOG_PORT.print((char)(pgm_read_byte(BUILD_DATE + i)));
    LOG_PORT.println(")");
#ifdef ARDUINO_ARCH_ESP8266
    LOG_PORT.println (ESP.getFullVersion ());
#else
    LOG_PORT.println (ESP.getSdkVersion ());
#endif
    // DEBUG_V ("");

    OutputMgr.Begin ();

    // Dump supported input modes
    LOG_PORT.println(F("Supported Input modes:"));
    itInput = INPUT_MODES.begin();
    while (itInput != INPUT_MODES.end()) 
    {
        LOG_PORT.printf("- %s : %s\n", itInput->first.c_str(), itInput->second->getBrief());
        itInput++;
    }
    // DEBUG_V ("");

    // Enable SPIFFS
#ifdef ARDUINO_ARCH_ESP8266
    if (!SPIFFS.begin ())
#else
    if (!SPIFFS.begin (false))
#endif
    {
        LOG_PORT.println(F("*** File system did not initialize correctly ***"));
    } 
    else 
    {
        LOG_PORT.println(F("File system initialized."));
    }

#ifdef ARDUINO_ARCH_ESP8266
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
        LOG_PORT.printf("Total bytes used in file system: %u.\n", fs_info.usedBytes);
/*
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            File file = dir.openFile("r");
            LOG_PORT.printf("%s : %u\n", file.name(), file.size());
            file.close();
        }
*/
#elif defined(ARDUINO_ARCH_ESP32)
    if (0 != SPIFFS.totalBytes ())
    {
        LOG_PORT.println (String ("Total bytes in file system: ") + String (SPIFFS.usedBytes ()));

        fs::File root = SPIFFS.open ("/");
        fs::File MyFile = root.openNextFile();

        while (MyFile)
        {
            LOG_PORT.println ("'" + String (MyFile.name ()) + "': \t'" + String (MyFile.size ()) + "'");
            MyFile = MyFile.openNextFile ();
        }
#endif // ARDUINO_ARCH_ESP32

    }
    else 
    {
        LOG_PORT.println(F("*** Failed to read file system details ***"));
    }
    // DEBUG_V ("");

    // Load configuration from SPIFFS and set Hostname
    loadConfig();
    // DEBUG_V ("");

    if (config.hostname)
    {
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.hostname(config.hostname);
#else
    WiFi.setHostname (config.hostname.c_str());
#endif
    }
    // DEBUG_V ("");

    // Setup WiFi Handlers
#ifdef ARDUINO_ARCH_ESP8266
    wifiConnectHandler = WiFi.onStationModeGotIP(onWiFiConnect);
#else
    WiFi.onEvent (onWiFiConnect,    WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent (onWiFiDisconnect, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
#endif

    //TODO: Setup MQTT / Auxiliary service Handlers?

    // Fallback to default SSID and passphrase if we fail to connect
    initWifi();
    if (WiFi.status() != WL_CONNECTED) 
    {
        LOG_PORT.println(F("*** Timeout - Reverting to default SSID ***"));
        config.ssid = ssid;
        config.passphrase = passphrase;
        initWifi();
    }
    // DEBUG_V ("");

    // If we fail again, go SoftAP or reboot
    if (WiFi.status() != WL_CONNECTED) 
    {
        if (config.ap_fallback) 
        {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode(WIFI_AP);
            String ssid = "ESPixelStick " + String(config.hostname);
            WiFi.softAP(ssid.c_str());
            ourLocalIP = WiFi.softAPIP();
            ourSubnetMask = IPAddress(255,255,255,0);
        } else 
        {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart();
        }
    }
    // DEBUG_V ("");

#ifdef ARDUINO_ARCH_ESP8266
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

    // Handle OTA update from asynchronous callbacks
    Update.runAsync(true);
#else
	// not needed for ESP32
#endif

    // Configure and start the web server
    initWeb();

    // DEBUG_END;
}

/////////////////////////////////////////////////////////
//
//  WiFi Section
//
/////////////////////////////////////////////////////////

void initWifi() 
{
    // Switch to station mode and disconnect just in case
    WiFi.mode(WIFI_STA);
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.disconnect();
#else
    WiFi.persistent (false);
    WiFi.disconnect (true);
#endif

    connectWifi();
    uint32_t timeout = millis();
    while (WiFi.status() != WL_CONNECTED) 
    {
        LOG_PORT.print(".");
        delay(500);
        if (millis() - timeout > (1000 * config.sta_timeout) )
        {
            LOG_PORT.println("");
            LOG_PORT.println(F("*** Failed to connect ***"));
            break;
        }
    }
}

void connectWifi() 
{
#ifdef ARDUINO_ARCH_ESP8266
    delay(secureRandom(100, 500));
#else
    delay (random (100, 500));
#endif
    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.print(config.ssid);
    LOG_PORT.print(F(" as "));
    LOG_PORT.println(config.hostname);

    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
    if (config.dhcp) 
    {
        LOG_PORT.print(F("Connecting with DHCP"));
    } 
    else 
    {
        // We don't use DNS, so just set it to our gateway
        if (!config.ip.isEmpty()) 
        {
            IPAddress ip = ip.fromString(config.ip);
            IPAddress gateway = gateway.fromString(config.gateway);
            IPAddress netmask = netmask.fromString(config.netmask);
            WiFi.config(ip, gateway, netmask, gateway);
            LOG_PORT.print(F("Connecting with Static IP"));
        }
        else
        {
            LOG_PORT.println(F("** ERROR - STATIC SELECTED WITHOUT IP **"));
        }
    }
}

#ifdef ARDUINO_ARCH_ESP8266
void onWiFiConnect(const WiFiEventStationModeGotIP &event) 
{
#else
void onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info) 
{
#endif
    ourLocalIP = WiFi.localIP();
    ourSubnetMask = WiFi.subnetMask();
    LOG_PORT.printf("\nConnected with IP: %s\n", ourLocalIP.toString().c_str());

    // Setup MQTT connection if enabled

    // Setup mDNS / DNS-SD
    //TODO: Reboot or restart mdns when config.id is changed?
/*
#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = String ((unsigned long)ESP.getEfuseMac (), HEX);
#endif
    MDNS.setInstanceName(String(config.id + " (" + chipId + ")").c_str());
    if (MDNS.begin(config.hostname.c_str())) {
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

/// WiFi Disconnect Handler
#ifdef ARDUINO_ARCH_ESP8266
/** Attempt to re-connect every 2 seconds */
void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event) 
{
#else
static void onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info) 
{
#endif
    LOG_PORT.println(F("*** WiFi Disconnected ***"));
    wifiTicker.once(2, connectWifi);
}

/////////////////////////////////////////////////////////
//
//  Web Section
//
/////////////////////////////////////////////////////////

// Configure and start the web server
void initWeb()
{
    // DEBUG_START;
    // Add header for SVG plot support?
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");

    // Setup WebSockets
    ws.onEvent(WebIO::onEvent);
    web.addHandler(&ws);

    // Heap status handler
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // JSON Config Handler
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) 
    {
        request->send(200, "text/json", serializeCore(true));
    });

    // Firmware upload handler - only in station mode
    web.on("/updatefw", HTTP_POST, [](AsyncWebServerRequest *request) 
    {
        ws.textAll("X6");
    }, WebIO::onFirmwareUpload).setFilter(ON_STA_FILTER);

    // Root access for testing
    web.serveStatic("/root", SPIFFS, "/");

    // Static Handler
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");

    // Raw config file Handler - but only on station
//  web.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER);

    web.onNotFound([](AsyncWebServerRequest *request) 
    {
        request->send(404, "text/plain", "Page not found");
    });

/*
    // Config file upload handler - only in station mode
    web.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, handle_config_upload).setFilter(ON_STA_FILTER);
*/
    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
    // DEBUG_END;
}

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

/// Set input / output modes
/** Cleans up i/o modules as needed and re-initializes showBuffer.
 */
void setMode(_Input *newinput) 
{
    // DEBUG_START;

    if (newinput != nullptr) 
    {
        input->destroy();
        input = newinput;
    }
    // DEBUG_V ("");

//        input->setBuffer(showBuffer, szBuffer);

    // Can't init input until showBuffer is setup
    if (newinput != nullptr)
    {
        input->init ();
    }

    // DEBUG_END;
}

/// Configuration Validations
/** Validates the config_t (core) configuration structure and forces defaults for invalid entries */
void validateConfig() 
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = String ((unsigned long)ESP.getEfuseMac (), HEX);
#endif

    // Device defaults
    if (!config.id.length())
        config.id = "No ID Found";
    if (!config.ssid.length())
        config.ssid = ssid;
    if (!config.passphrase.length())
        config.passphrase = passphrase;
    if (!config.hostname.length())
        config.hostname = "esps-" + String(chipId);

    if (config.sta_timeout < 5)
        config.sta_timeout = CLIENT_TIMEOUT;

    if (config.ap_timeout < 15)
        config.ap_timeout = AP_TIMEOUT;
    // DEBUG_V ("");

//TODO: Update this to set to ws2811 and e131 if no config found
    itInput = INPUT_MODES.find(config.input);
    if (itInput != INPUT_MODES.end()) {
        input = itInput->second;
        LOG_PORT.printf("- Input mode set to %s\n", input->getKey());
    } else {
        itInput = INPUT_MODES.begin();
        LOG_PORT.printf("* Input mode from core config invalid, setting to %s.\n",
                itInput->first.c_str());
        config.input = itInput->first;
        input = itInput->second;
    }
    // DEBUG_V ("");

    // Set I/O modes
    setMode(input);
    // DEBUG_END;
}

/// Deserialize device confiugration JSON to config structure - returns true if config change detected
boolean dsDevice(DynamicJsonDocument &json) 
{
    boolean retval = false;
    if (json.containsKey("device")) {
        retval = retval | FileIO::setFromJSON(config.id,     json["device"]["id"]);
        retval = retval | FileIO::setFromJSON(config.input,  json["device"]["input"]);
        // todo retval = retval | FileIO::setFromJSON(config.output, json["device"]["output"]);
    } else {
        LOG_PORT.println("No device settings found.");
    }

    return retval;
}

/// Deserialize network confiugration JSON to config structure - returns true if config change detected
boolean dsNetwork(DynamicJsonDocument &json) {
    boolean retval = false;
    if (json.containsKey("network")) {
        JsonObject network = json["network"];
        retval = retval | FileIO::setFromJSON(config.ssid, network["ssid"]);
        retval = retval | FileIO::setFromJSON(config.passphrase, network["passphrase"]);
        retval = retval | FileIO::setFromJSON(config.ip, network["ip"]);
        retval = retval | FileIO::setFromJSON(config.netmask, network["netmask"]);
        retval = retval | FileIO::setFromJSON(config.gateway, network["gateway"]);
        retval = retval | FileIO::setFromJSON(config.hostname, network["hostname"]);
        retval = retval | FileIO::setFromJSON(config.dhcp, network["dhcp"]);
        retval = retval | FileIO::setFromJSON(config.sta_timeout, network["sta_timeout"]);
        retval = retval | FileIO::setFromJSON(config.ap_fallback, network["ap_fallback"]);
        retval = retval | FileIO::setFromJSON(config.ap_timeout, network["ap_timeout"]);
    } else {
        LOG_PORT.println("No network settings found.");
    }

    return retval;
}

void deserializeCore(DynamicJsonDocument &json) 
{
    dsDevice(json);
    dsNetwork(json);
}

/// Load configuration file
/** Loads and validates the JSON configuration file via SPIFFS.
 *  If no configuration file is found, a new one will be created.
 */
void loadConfig() 
{
    // DEBUG_START;

    // Zeroize Config struct
    memset(&config, 0, sizeof(config));

    if (FileIO::loadConfig(CONFIG_FILE, &deserializeCore)) 
    {
        // DEBUG_V ("");

        validateConfig();
    } 
    else
    {
        // Load failed, create a new config file and save it
        // DEBUG_V ("");

        saveConfig();
    }

    // cause the config for the output channels to get reloaded
    OutputMgr.LoadConfig ();

    // DEBUG_END;

    //TODO: Add auxiliary service load routine
}

// Serialize the current config into a JSON string
String serializeCore(boolean pretty, boolean creds) {
    // Create buffer and root object
    DynamicJsonDocument json(1024);
    String jsonString;

    // Device
    JsonObject device = json.createNestedObject("device");
    device["id"] = config.id.c_str();
    device["input"] = config.input.c_str();
    // todo device["output"] = config.output.c_str();

    // Network
    JsonObject network = json.createNestedObject("network");
    network["ssid"] = config.ssid.c_str();
    if (creds)
        network["passphrase"] = config.passphrase.c_str();
    network["hostname"] = config.hostname.c_str();
    network["ip"] = config.ip.c_str();
    network["netmask"] = config.netmask.c_str();
    network["gateway"] = config.gateway.c_str();

    network["dhcp"] = config.dhcp;
    network["sta_timeout"] = config.sta_timeout;

    network["ap_fallback"] = config.ap_fallback;
    network["ap_timeout"] = config.ap_timeout;

    if (pretty)
        serializeJsonPretty(json, jsonString);
    else
        serializeJson(json, jsonString);

    return jsonString;
}

// Save configuration JSON file
void saveConfig() 
{
    // Validate Config
    validateConfig();

    // Save Config
    FileIO::saveConfig(CONFIG_FILE, serializeCore(false, true));

    // save the config for the output channels
    OutputMgr.SaveConfig ();
}

/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
/// Main Loop
/** Arduino based main loop */
void loop() {
    // Reboot handler
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    // Process input data
    input->process();

    // Render output
    OutputMgr.Render();

//TODO: Research this further
// workaround crash - consume incoming bytes on serial port
    while (0 != LOG_PORT.available()) 
    {
        LOG_PORT.read();
    }
}
