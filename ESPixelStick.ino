/*
* ESPixelStick.ino
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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
#include <Int64String.h>

// Input modules
#include "src/input/InputMgr.hpp"

// Output modules
#include "src/output/OutputMgr.hpp"

// WiFi interface
#include "src/WiFiMgr.hpp"

// WEB interface
#include "src/WebMgr.hpp"

// Services
#include "src/service/ESPAsyncZCPP.h"
#include "src/service/FPPDiscovery.h"

#ifdef ARDUINO_ARCH_ESP8266
#include <Hash.h>
extern "C"
{
#   include <user_interface.h>
} // extern "C" 

#elif defined ARDUINO_ARCH_ESP32
    // ESP32 user_interface is now built in
#   include <Update.h>
#   include <esp_task_wdt.h>
#else
#	error "Unsupported CPU type."
#endif

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
//  Globals
//
/////////////////////////////////////////////////////////

// Configuration file
String ConfigFileName = "/config.json";

//TODO: Add Auxiliary services
//zcpp
//FPPDiscovery

const String VERSION = "4.0_unified-dev";
const String BUILD_DATE = String(__DATE__);

config_t            config;                 // Current configuration
bool                reboot = false;         // Reboot flag
uint32_t            lastUpdate;             // Update timeout tracker
int                 ConfigSaveNeeded = 0;

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
void GetConfig (JsonObject & json);

/// Arduino Setup
/** Arduino based setup code that is executed at startup. */
void setup() 
{
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
    LOG_PORT.println(String(F("ESPixelStick v")) + VERSION + "(" + BUILD_DATE + ")");
#ifdef ARDUINO_ARCH_ESP8266
    LOG_PORT.println (String ("ESP Verion: ") + ESP.getFullVersion ());
#else
    LOG_PORT.println (String("ESP Verion: ") + ESP.getSdkVersion ());
#endif
    // DEBUG_V ("");

    // DEBUG_V ("");
    FileIO::Begin ();

    // Load configuration from SPIFFS and set Hostname
    loadConfig();
    // DEBUG_V ("");

    // Set up the output manager to start sending data to the serial ports
    OutputMgr.Begin ();
    // DEBUG_V ("");

    WiFiMgr.Begin (& config);
    // DEBUG_V ("");

    // connect the input processing to the output processing. 
    // Only supports a single channel at the moment
    InputMgr.Begin (OutputMgr.GetBufferAddress (c_OutputMgr::e_OutputChannelIds::OutputChannelId_1),
                    OutputMgr.GetBufferSize    (c_OutputMgr::e_OutputChannelIds::OutputChannelId_1));

    // DEBUG_V ("");

    // Configure and start the web server
    WebMgr.Begin(&config);

    FPPDiscovery.begin ();

    ESPAsyncZCPP.begin ();

#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtEnable (2000);
#else
    esp_task_wdt_init (5, true);
#endif
    // DEBUG_END;

} // setup

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

/// Configuration Validations
/** Validates the config_t (core) configuration structure and forces defaults for invalid entries */
void validateConfig() 
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = int64String (ESP.getEfuseMac (), HEX);
#endif

    // Device defaults
    if (!config.id.length ())
    {
        config.id = "No ID Found";
        ConfigSaveNeeded++;
    }

    if (!config.hostname.length ())
    {
        config.hostname = "esps-" + String (chipId);
        ConfigSaveNeeded++;
    }

    ConfigSaveNeeded += WiFiMgr.ValidateConfig (&config);

    // DEBUG_END;
} // validateConfig

/// Deserialize device confiugration JSON to config structure - returns true if config change detected
boolean dsDevice(JsonObject & json)
{
    // DEBUG_START;

    boolean retval = false;
    if (json.containsKey("device"))
    {
        retval = retval | FileIO::setFromJSON(config.id,     json[F("device")][F("id")]);
        retval = retval | FileIO::setFromJSON(config.input,  json[F("device")][F("input")]);
    }
    else 
    {
        LOG_PORT.println(F("No device settings found."));
    }

    // DEBUG_END;

    return retval;
} // dsDevice

/// Deserialize network confiugration JSON to config structure - returns true if config change detected
boolean dsNetwork(JsonObject & json)
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
        LOG_PORT.println(F("No network settings found."));
    }

    // DEBUG_END;
    return retval;
}

void SetConfig (JsonObject& json)
{
    // DEBUG_START;
    deserializeCore (json);
    ConfigSaveNeeded++;
    // DEBUG_END;

} // SetConfig

void deserializeCore (JsonObject & json)
{
    // DEBUG_START;
    dsDevice (json);
    dsNetwork (json);
    // DEBUG_END;
}

void deserializeCoreHandler (DynamicJsonDocument & jsonDoc)
{
    JsonObject json = jsonDoc.as<JsonObject> ();
    // DEBUG_START;
    deserializeCore (json);
    // DEBUG_END;
}

/// Load configuration file
/** Loads and validates the JSON configuration file via SPIFFS.
 *  If no configuration file is found, a new one will be created.
 */
void loadConfig() 
{
    // DEBUG_START;

    // Zeroize Config struct
    memset (&config, 0, sizeof (config));

    // DEBUG_V ("");
    if (FileIO::loadConfig(ConfigFileName, &deserializeCoreHandler))
    {
        validateConfig();
    } 
    else
    {
        // Load failed, create a new config file and save it
        ConfigSaveNeeded = false;
        SaveConfig();
    }

    //TODO: Add auxiliary service load routine

    // DEBUG_START;
} // loadConfig

void GetConfig (JsonObject & json)
{
    // DEBUG_START;

    // Device
    JsonObject device = json.createNestedObject(F("device"));
    device["id"]           = config.id;
    device["input"]        = config.input;

    // Network
    JsonObject network = json.createNestedObject(F("network"));
    network["ssid"]        = config.ssid;
    network["passphrase"]  = config.passphrase;
    network["hostname"]    = config.hostname;
    network["ip"]          = config.ip;
    network["netmask"]     = config.netmask;
    network["gateway"]     = config.gateway;

    network["dhcp"]        = config.UseDhcp;
    network["sta_timeout"] = config.sta_timeout;

    network["ap_fallback"] = config.ap_fallbackIsEnabled;
    network["ap_timeout"]  = config.ap_timeout;

    // DEBUG_END;
}
// Serialize the current config into a JSON string
String serializeCore(boolean pretty) 
{
    // DEBUG_START;

    // Create buffer and root object
    DynamicJsonDocument jsonConfigDoc(2048);
    JsonObject JsonConfig = jsonConfigDoc.createNestedObject();

    String jsonConfigString;

    GetConfig (JsonConfig);

    if (pretty)
    {
        serializeJsonPretty (JsonConfig, jsonConfigString);
    }
    else
    {
        serializeJson (JsonConfig, jsonConfigString);
    }

    // DEBUG_END;

    return jsonConfigString;
} // serializeCore

// Save configuration JSON file
void SaveConfig() 
{
    // DEBUG_START;

    // Validate Config
    validateConfig();

    // Save Config
    String DataToSave = serializeCore (false);
    FileIO::SaveConfig(ConfigFileName, DataToSave);

    // DEBUG_END;
} // SaveConfig

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
        LOG_PORT.println ("Rebooting");
        delay(REBOOT_DELAY);
        ESP.restart();
    }
#ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_reset ();
#else
    ESP.wdtFeed ();
#endif // def ARDUINO_ARCH_ESP32

    // do we need to save the current config?
    if (0 != ConfigSaveNeeded)
    {
        ConfigSaveNeeded = 0;
        SaveConfig ();
    } // done need to save the current config

    // Process input data
    InputMgr.Process ();

    // Render output
    OutputMgr.Render();

    WebMgr.Process ();

// need to keep the rx pipeline empty
    size_t BytesToDiscard = max (500, LOG_PORT.available ());
    while (0 < BytesToDiscard)
    {
        BytesToDiscard--;
        LOG_PORT.read();
#ifdef ARDUINO_ARCH_ESP32
        esp_task_wdt_reset ();
#else
        ESP.wdtFeed ();
#endif // def ARDUINO_ARCH_ESP32
    } // end discard loop
}
