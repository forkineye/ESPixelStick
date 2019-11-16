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
#include "secrets.h"

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = SECRETS_SSID;
const char passphrase[] = SECRETS_PASS;

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <Hash.h>
#include <SPI.h>

// Core
#include "src/ESPixelStick.h"
#include "src/EFUpdate.h"
#include "src/FileIO.h"
#include "src/WebIO.h"
//#include "src/wshandler.h"

// Inputs
#include "src/input/_Input.h"
#include "src/input/E131Input.h"
//#include "src/input/ESPAsyncDDP.h"

// Outputs
#include "src/output/_Output.h"
#include "src/output/WS2811.h"
//#include "src/output/SerialDriver.h"

// Services
//#include "src/service/MQTT.h"
//#include "src/service/EffectEngine.h"
//#include "src/service/ESPAsyncZCPP.h"
//#include "src/service/FPPDiscovery.h"

extern "C" {
#include <user_interface.h>
}

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
const String strddp="ddp";
const std::map<const String, _Input*> INPUT_MODES = {
    { E131Input::KEY, new E131Input() }
};

/// Map of output modules
std::map<const String, _Output*>::const_iterator itOutput;
const String strdmx="dmx";
const std::map<const String, _Output*> OUTPUT_MODES = {
    { WS2811::KEY, new WS2811() }
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
_Output *output;    ///< Pointer to currently enabled output module

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
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
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
void setInput();
void setOutput();

/// Radio configuration
/** ESP8266 radio configuration routines that are executed at startup. */
/* Disabled for now, possible flash wear issue. Need to research further
RF_PRE_INIT() {
    system_phy_set_powerup_option(3);   // Do full RF calibration on power-up
    system_phy_set_max_tpw(82);         // Set max TX power
}
*/

/// Arduino Setup
/** Arduino based setup code that is executed at startup. */
void setup() {
    // Disable persistant credential storage and configure SDK params
    WiFi.persistent(false);
    wifi_set_sleep_type(NONE_SLEEP_T);

    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

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
    LOG_PORT.println(ESP.getFullVersion());

    // Dump supported input modes
    LOG_PORT.println(F("Supported Input modes:"));
    itInput = INPUT_MODES.begin();
    while (itInput != INPUT_MODES.end()) {
        LOG_PORT.printf("- %s : %s\n", itInput->first.c_str(), itInput->second->getBrief().c_str());
        itInput++;
    }

    // Dump supported output modes
    LOG_PORT.println(F("Supported Output modes:"));
    itOutput = OUTPUT_MODES.begin();
    while (itOutput != OUTPUT_MODES.end()) {
        LOG_PORT.printf("- %s : %s\n", itOutput->first.c_str(), itOutput->second->getBrief().c_str());
        itOutput++;
    }

    // Enable SPIFFS
    if (!SPIFFS.begin()) {
        LOG_PORT.println(F("*** File system did not initialize correctly ***"));
    } else {
        LOG_PORT.println(F("File system initialized."));
    }

    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
        LOG_PORT.printf("Total bytes used in file system: %u.\n", fs_info.usedBytes);

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            File file = dir.openFile("r");
            LOG_PORT.printf("%s : %u\n", file.name(), file.size());
            file.close();
        }
    } else {
        LOG_PORT.println(F("*** Failed to read file system details ***"));
    }

    // Load configuration from SPIFFS and set Hostname
    loadConfig();
    WiFi.hostname(config.hostname);

    // Configure inputs and outputs
    setMode(input, output);

    // Setup WiFi Handlers
    wifiConnectHandler = WiFi.onStationModeGotIP(onWiFiConnect);

    //TODO: Setup MQTT / Auxiliary service Handlers?

    // Fallback to default SSID and passphrase if we fail to connect
    initWifi();
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PORT.println(F("*** Timeout - Reverting to default SSID ***"));
        config.ssid = ssid;
        config.passphrase = passphrase;
        initWifi();
    }

    // If we fail again, go SoftAP or reboot
    if (WiFi.status() != WL_CONNECTED) {
        if (config.ap_fallback) {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode(WIFI_AP);
            String ssid = "ESPixelStick " + String(config.hostname);
            WiFi.softAP(ssid.c_str());
            ourLocalIP = WiFi.softAPIP();
            ourSubnetMask = IPAddress(255,255,255,0);
        } else {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart();
        }
    }

    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

    // Handle OTA update from asynchronous callbacks
    Update.runAsync(true);

    // Configure and start the web server
    initWeb();
}

/////////////////////////////////////////////////////////
//
//  WiFi Section
//
/////////////////////////////////////////////////////////

void initWifi() {
    // Switch to station mode and disconnect just in case
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    connectWifi();
    uint32_t timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
        LOG_PORT.print(".");
        delay(500);
        if (millis() - timeout > (1000 * config.sta_timeout) ){
            LOG_PORT.println("");
            LOG_PORT.println(F("*** Failed to connect ***"));
            break;
        }
    }
}

void connectWifi() {
    delay(secureRandom(100, 500));

    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.print(config.ssid);
    LOG_PORT.print(F(" as "));
    LOG_PORT.println(config.hostname);

    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
    if (config.dhcp) {
        LOG_PORT.print(F("Connecting with DHCP"));
    } else {
        // We don't use DNS, so just set it to our gateway
        IPAddress ip = ip.fromString(config.ip);
        IPAddress gateway = gateway.fromString(config.gateway);
        IPAddress netmask = netmask.fromString(config.netmask);
        WiFi.config(ip, gateway, netmask, gateway);
        LOG_PORT.print(F("Connecting with Static IP"));
    }
}

void onWiFiConnect(const WiFiEventStationModeGotIP &event) {
    ourLocalIP = WiFi.localIP();
    ourSubnetMask = WiFi.subnetMask();
    LOG_PORT.printf("\nConnected with IP: %s\n", ourLocalIP.toString().c_str());

    // Setup MQTT connection if enabled

    // Setup mDNS / DNS-SD
    //TODO: Reboot or restart mdns when config.id is changed?
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    MDNS.setInstanceName(String(config.id + " (" + String(chipId) + ")").c_str());
    if (MDNS.begin(config.hostname.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
//        MDNS.addService("zcpp", "udp", ZCPP_PORT);
//        MDNS.addService("ddp", "udp", DDP_PORT);
        MDNS.addService("e131", "udp", E131_DEFAULT_PORT);
        MDNS.addServiceTxt("e131", "udp", "TxtVers", String(RDMNET_DNSSD_TXTVERS));
        MDNS.addServiceTxt("e131", "udp", "ConfScope", RDMNET_DEFAULT_SCOPE);
        MDNS.addServiceTxt("e131", "udp", "E133Vers", String(RDMNET_DNSSD_E133VERS));
        MDNS.addServiceTxt("e131", "udp", "CID", String(chipId));
        MDNS.addServiceTxt("e131", "udp", "Model", "ESPixelStick");
        MDNS.addServiceTxt("e131", "udp", "Manuf", "Forkineye");
    } else {
        LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
    }
}

/// WiFi Disconnect Handler
/** Attempt to re-connect every 2 seconds */
void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event) {
    LOG_PORT.println(F("*** WiFi Disconnected ***"));
    wifiTicker.once(2, connectWifi);
}

/////////////////////////////////////////////////////////
//
//  Web Section
//
/////////////////////////////////////////////////////////

// Configure and start the web server
void initWeb() {
    // Add header for SVG plot support?
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");

    // Setup WebSockets
    ws.onEvent(WebIO::onEvent);
    web.addHandler(&ws);

    // Heap status handler
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // JSON Config Handler
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) {
        String jsonString;
        serializeCore(jsonString, true);
        request->send(200, "text/json", jsonString);
    });

    // Firmware upload handler - only in station mode
    web.on("/updatefw", HTTP_POST, [](AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, WebIO::onFirmwareUpload).setFilter(ON_STA_FILTER);

    // Root access for testing
    web.serveStatic("/root", SPIFFS, "/");

    // Static Handler
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");

    // Raw config file Handler - but only on station
//  web.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER);

    web.onNotFound([](AsyncWebServerRequest *request) {
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
}

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

/// Set input / output modes
/** Cleans up i/o modules as needed and re-initializes showBuffer.
 */
void setMode(_Input *newinput, _Output *newoutput) {
    if (newoutput != nullptr) {
        output->destroy();
        output = newoutput;
        output->init();
    }

    if (newinput != nullptr) {
        input->destroy();
        input = newinput;
    }

    // showBuffer only needs to change if there's an output mode change
    if (newoutput != nullptr) {
        uint16_t szBuffer = output->getTupleCount() * output->getTupleSize();
        if (showBuffer) free (showBuffer);
        if (showBuffer = static_cast<uint8_t *>(malloc(szBuffer)))
            memset(showBuffer, 0, szBuffer);

        output->setBuffer(showBuffer);
        input->setBuffer(showBuffer, szBuffer);
    }

    // Can't init input until showBuffer is setup
    if (newinput != nullptr)
        input->init();

    // Render an output frame
    output->render();
}

/// Configuration Validations
/** Validates the config_t (core) configuration structure and forces defaults for invalid entries */
void validateConfig() {
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());

    // Device defaults
    if (!config.id.length())
        config.id = "No Config Found";
    if (!config.ssid.length())
        config.ssid = ssid;
    if (!config.passphrase.length())
        config.passphrase = passphrase;
    if (!config.hostname.length())
        config.hostname = "esps-" + String(chipId);

//TODO: Update this to set to ws2811 and e131 if no config found
    itInput = INPUT_MODES.find(config.input);
    if (itInput != INPUT_MODES.end()) {
        input = itInput->second;
        LOG_PORT.printf("- Input mode set to %s\n", input->getKey().c_str());
    } else {
        itInput = INPUT_MODES.begin();
        LOG_PORT.printf("* Input mode from core config invalid, setting to %s.\n",
                itInput->first.c_str());
        config.input = itInput->first;
        input = itInput->second;
    }

    itOutput = OUTPUT_MODES.find(config.output);
    if (itOutput != OUTPUT_MODES.end()) {
        output = itOutput->second;
        LOG_PORT.printf("- Output mode set to %s\n", output->getKey().c_str());
    } else {
        itOutput = OUTPUT_MODES.begin();
        LOG_PORT.printf("* Input mode from core config invalid, setting to %s.\n",
                itOutput->first.c_str());
        config.output = itOutput->first;
        output = itOutput->second;
    }
}

void deserializeCore(DynamicJsonDocument &json) {
    // Device
    if (json.containsKey("device")) {
        config.id = json["device"]["id"].as<String>();
        config.input = json["device"]["input"].as<String>();
        config.output = json["device"]["output"].as<String>();
    } else {
        LOG_PORT.println("No device settings found.");
    }

    // Network
    if (json.containsKey("network")) {
        JsonObject networkJson = json["network"];

        // Fallback to embedded ssid and passphrase if null in config
        config.ssid = networkJson["ssid"].as<String>();
        if (!config.ssid.length())
            config.ssid = ssid;

        config.passphrase = networkJson["passphrase"].as<String>();
        if (!config.passphrase.length())
            config.passphrase = passphrase;

        // Network
        config.ip = networkJson["ip"].as<String>();
        config.netmask = networkJson["netmask"].as<String>();
        config.gateway = networkJson["gateway"].as<String>();

        config.dhcp = networkJson["dhcp"];
        config.sta_timeout = networkJson["sta_timeout"] | CLIENT_TIMEOUT;
        if (config.sta_timeout < 5) {
            config.sta_timeout = 5;
        }

        config.ap_fallback = networkJson["ap_fallback"];
        config.ap_timeout = networkJson["ap_timeout"] | AP_TIMEOUT;
        if (config.ap_timeout < 15) {
            config.ap_timeout = 15;
        }

        if (networkJson.containsKey("hostname"))
            config.hostname = networkJson["hostname"].as<String>();
    } else {
        LOG_PORT.println("No network settings found.");
    }

    if (!config.hostname.length()) {
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        config.hostname = "esps-" + String(chipId);
    }
}

/// Load configuration file
/** Loads and validates the JSON configuration file via SPIFFS.
 *  If no configuration file is found, a new one will be created.
 */
void loadConfig() {
    // Zeroize Config struct
    memset(&config, 0, sizeof(config));

    if (FileIO::loadConfig(CONFIG_FILE, &deserializeCore)) {
        validateConfig();
    } else {
        // Load failed, create a new config file and save it
        saveConfig();
    }

    //TODO: Add auxiliary service load routine
}

// Serialize the current config into a JSON string
void serializeCore(String &jsonString, boolean pretty, boolean creds) {
    // Create buffer and root object
    DynamicJsonDocument json(1024);

    // Device
    JsonObject device = json.createNestedObject("device");
    device["id"] = config.id.c_str();
    device["input"] = config.input.c_str();
    device["output"] = config.output.c_str();

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
}

// Save configuration JSON file
void saveConfig() {
    // Validate Config
    validateConfig();

    // Serialize Config
    String jsonString;
    serializeCore(jsonString, false, true);

    // Save Config
    FileIO::saveConfig(CONFIG_FILE, jsonString);
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
    output->render();

//TODO: Research this further
// workaround crash - consume incoming bytes on serial port
    if (LOG_PORT.available()) {
        while (LOG_PORT.read() >= 0);
    }
}
