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

/*****************************************/
/*         END - Configuration           */
/*****************************************/

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

// WiFi interface
#include "src/WiFiMgr.hpp"


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
_Input* input = nullptr;;     ///< Pointer to currently enabled input module

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

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
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
    input = INPUT_MODES.begin ()->second;

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

    WiFiMgr.Begin (&config);
    // DEBUG_V ("");

    // Set up the output manager to start sending data to the serial ports
    OutputMgr.Begin ();
    // DEBUG_V ("");

    // connect the input processing to the output processing. 
    // Only supports a single channel at the moment
    input->setBuffer (OutputMgr.GetBufferAddress (c_OutputMgr::e_OutputChannelIds::OutputChannelId_1), 
                      OutputMgr.GetBufferSize    (c_OutputMgr::e_OutputChannelIds::OutputChannelId_1));
    // DEBUG_V ("");

    // Configure and start the web server
    initWeb();

    // DEBUG_END;

} // setup


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
    do // once
    {
        if (newinput == nullptr)
        {
            LOG_PORT.println ("- Invalid Input mode requested");
            break;
        }

        // does an input exist?
        if (input != nullptr)
        {
            LOG_PORT.printf ("- Releasing Input mode '%s'\n", input->getKey ());
            input->destroy ();
            input = nullptr;
        }

        // set up the new input
        input = newinput;
        LOG_PORT.printf ("- Input mode set to %s\n", input->getKey ());

        // Can't init input until showBuffer is setup
        input->setBuffer(OutputMgr.GetBufferAddress(c_OutputMgr::e_OutputChannelIds::OutputChannelId_1),
                         OutputMgr.GetBufferSize(c_OutputMgr::e_OutputChannelIds::OutputChannelId_1));
        input->init ();

    } while (false);

} // setMode

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

    if (!config.hostname.length())
        config.hostname = "esps-" + String(chipId);

    WiFiMgr.ValidateConfig (&config);

    // DEBUG_V ("");

//TODO: Update this to set to ws2811 and e131 if no config found

    // set the default value
    _Input* NewInput = INPUT_MODES.begin ()->second;

    // try to find the desired input type
    itInput = INPUT_MODES.find (config.input);
    if (itInput != INPUT_MODES.end())
    {
        NewInput = itInput->second;
    } 
    else
    {
        LOG_PORT.printf("* Input mode from core config invalid, setting to %s.\n", NewInput->getBrief());
    }
    // DEBUG_V ("");

    // Set I/O modes
    setMode(NewInput);

    // DEBUG_END;
} // validateConfig

/// Deserialize device confiugration JSON to config structure - returns true if config change detected
boolean dsDevice(DynamicJsonDocument &json) 
{
    boolean retval = false;
    if (json.containsKey("device"))
    {
        retval = retval | FileIO::setFromJSON(config.id,     json["device"]["id"]);
        retval = retval | FileIO::setFromJSON(config.input,  json["device"]["input"]);
    }
    else 
    {
        LOG_PORT.println("No device settings found.");
    }

    return retval;
} // dsDevice

/// Deserialize network confiugration JSON to config structure - returns true if config change detected
boolean dsNetwork(DynamicJsonDocument &json) 
{
    // DEBUG_START;

    boolean retval = false;
    if (json.containsKey("network")) 
    {
        JsonObject network = json["network"];
        retval = retval | FileIO::setFromJSON(config.ssid,                 network["ssid"]);
        retval = retval | FileIO::setFromJSON(config.passphrase,           network["passphrase"]);
        retval = retval | FileIO::setFromJSON(config.ip,                   network["ip"]);
        retval = retval | FileIO::setFromJSON(config.netmask,              network["netmask"]);
        retval = retval | FileIO::setFromJSON(config.gateway,              network["gateway"]);
        retval = retval | FileIO::setFromJSON(config.hostname,             network["hostname"]);
        retval = retval | FileIO::setFromJSON(config.UseDhcp,              network["dhcp"]);
        retval = retval | FileIO::setFromJSON(config.sta_timeout,          network["sta_timeout"]);
        retval = retval | FileIO::setFromJSON(config.ap_fallbackIsEnabled, network["ap_fallback"]);
        retval = retval | FileIO::setFromJSON(config.ap_timeout,           network["ap_timeout"]);
    }
    else
    {
        LOG_PORT.println("No network settings found.");
    }

    // DEBUG_END;
    return retval;
}

void deserializeCore(DynamicJsonDocument &json) 
{
//    // DEBUG_START;
    dsDevice(json);
    dsNetwork(json);
//    // DEBUG_END;
}

/// Load configuration file
/** Loads and validates the JSON configuration file via SPIFFS.
 *  If no configuration file is found, a new one will be created.
 */
void loadConfig() 
{
    // Zeroize Config struct
    memset (&config, 0, sizeof (config));

    if (FileIO::loadConfig(CONFIG_FILE, &deserializeCore)) 
    {
        validateConfig();
    } 
    else
    {
        // Load failed, create a new config file and save it
        saveConfig();
    }

    //TODO: Add auxiliary service load routine

} // loadConfig

// Serialize the current config into a JSON string
String serializeCore(boolean pretty, boolean creds) {
    // Create buffer and root object
    DynamicJsonDocument json(1024);
    String jsonString;

    // Device
    JsonObject device = json.createNestedObject("device");
    device["id"]           = config.id;
    device["input"]        = config.input;

    // Network
    JsonObject network = json.createNestedObject("network");
    network["ssid"]        = config.ssid;
    if (creds) { network["passphrase"] = config.passphrase; };
    network["hostname"]    = config.hostname;
    network["ip"]          = config.ip;
    network["netmask"]     = config.netmask;
    network["gateway"]     = config.gateway;

    network["dhcp"]        = config.UseDhcp;
    network["sta_timeout"] = config.sta_timeout;

    network["ap_fallback"] = config.ap_fallbackIsEnabled;
    network["ap_timeout"]  = config.ap_timeout;

    if (pretty)
    {
        serializeJsonPretty (json, jsonString);
    }
    else
    {
        serializeJson (json, jsonString);
    }

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
} // saveConfig

/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
/// Main Loop
/** Arduino based main loop */
void loop() 
{
    // Reboot handler
    if (reboot) 
    {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    // Process input data
    input->process();

    // Render output
    OutputMgr.Render();

//TODO: Research this further
// workaround crash - consume incoming bytes on LOG serial port
    while (0 != LOG_PORT.available()) 
    {
        LOG_PORT.read();
    }
}
