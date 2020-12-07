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

const String VERSION = "4.0_unified-dev";
const String BUILD_DATE = String(__DATE__);

config_t            config;                 // Current configuration
bool                reboot = false;         // Reboot flag
uint32_t            lastUpdate;             // Update timeout tracker
uint32_t            ConfigSaveNeeded = 0;
bool                ResetWiFi = false;

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
    config.ip      = IPAddress ((uint32_t)0);
    config.netmask = IPAddress ((uint32_t)0);
    config.gateway = IPAddress ((uint32_t)0);
    config.UseDhcp = true;
    config.ap_fallbackIsEnabled = true;
    config.RebootOnWiFiFailureToConnect = true;
    config.ap_timeout = AP_TIMEOUT;
    config.sta_timeout = CLIENT_TIMEOUT;

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
    LOG_PORT.println (String ("ESP Version: ") + ESP.getFullVersion ());
#else
    LOG_PORT.println (String("ESP Version: ") + ESP.getSdkVersion ());
#endif
    // DEBUG_V ("");

    // DEBUG_V ("");
    FileIO::Begin ();

    // Load configuration from the File System and set Hostname
    loadConfig();
    // DEBUG_V ("");

    // Set up the output manager to start sending data to the serial ports
    OutputMgr.Begin ();
    // DEBUG_V ("");

    WiFiMgr.Begin (& config);
    // DEBUG_V ("");

    // connect the input processing to the output processing. 
    InputMgr.Begin (OutputMgr.GetBufferAddress (), OutputMgr.GetBufferUsedSize ());

    // DEBUG_V ("");

    // Configure and start the web server
    WebMgr.Begin(&config);

    FPPDiscovery.begin ();

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
        // DEBUG_V ();
        ConfigSaveNeeded++;
    }

    if (0 == config.hostname.length ())
    {
        config.hostname = "esps-" + String (chipId);
        // DEBUG_V ();
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
        retval = retval | FileIO::setFromJSON (config.id,        json[F ("device")][F ("id")]);
                          FileIO::setFromJSON (ConfigSaveNeeded, json[F ("device")][F ("ConfigSaveNeeded")]);
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
#ifdef ARDUINO_ARCH_ESP8266
        IPAddress Temp = config.ip;
        String ip = Temp.toString ();
        Temp = config.gateway;
        String gateway = Temp.toString ();
        Temp = config.netmask;
        String netmask = Temp.toString ();
#else
        String ip      = config.ip.toString ();
        String gateway = config.gateway.toString ();
        String netmask = config.netmask.toString ();
#endif // def ARDUINO_ARCH_ESP8266

        JsonObject network = json["network"];
        retval |= FileIO::setFromJSON(config.ssid,                          network["ssid"]);
        retval |= FileIO::setFromJSON(config.passphrase,                    network["passphrase"]);
        retval |= FileIO::setFromJSON(ip,                                   network["ip"]);
        retval |= FileIO::setFromJSON(netmask,                              network["netmask"]);
        retval |= FileIO::setFromJSON(gateway,                              network["gateway"]);
        retval |= FileIO::setFromJSON(config.hostname,                      network["hostname"]);
        retval |= FileIO::setFromJSON(config.UseDhcp,                       network["dhcp"]);
        retval |= FileIO::setFromJSON(config.sta_timeout,                   network["sta_timeout"]);
        retval |= FileIO::setFromJSON(config.ap_fallbackIsEnabled,          network["ap_fallback"]);
        retval |= FileIO::setFromJSON(config.ap_timeout,                    network["ap_timeout"]);
        retval |= FileIO::setFromJSON (config.RebootOnWiFiFailureToConnect, network["ap_reboot"]);

        // DEBUG_V ("     ip: " + ip);
        // DEBUG_V ("gateway: " + gateway);
        // DEBUG_V ("netmask: " + netmask);

        config.ip.fromString (ip);
        config.gateway.fromString (gateway);
        config.netmask.fromString (netmask);
    }
    else
    {
        LOG_PORT.println(F("No network settings found."));
    }

    // DEBUG_V (String("retval: ") + String(retval));
    // DEBUG_END;
    return retval;
}

void SetConfig (JsonObject& json)
{
    // DEBUG_START;
    ResetWiFi = deserializeCore (json);

    // DEBUG_V ();
    ConfigSaveNeeded = 1;
    // DEBUG_END;

} // SetConfig

bool deserializeCore (JsonObject & json)
{
    bool response = false;
    // DEBUG_START;
    dsDevice (json);
    response = dsNetwork (json);

    // DEBUG_END;
    return response;
}

void deserializeCoreHandler (DynamicJsonDocument & jsonDoc)
{
    JsonObject json = jsonDoc.as<JsonObject> ();
    // DEBUG_START;
    deserializeCore (json);
    // DEBUG_END;
}

/// Load configuration file
/** Loads and validates the JSON configuration file from the file system.
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
    // DEBUG_V ("Load failed, create a new config file and save it");
        ConfigSaveNeeded = false;
        SaveConfig();
    }

    // DEBUG_START;
} // loadConfig

void DeleteConfig ()
{
    // DEBUG_START;
    FileIO::DeleteFile (ConfigFileName);
    // DEBUG_END;

} // DeleteConfig

void GetConfig (JsonObject & json)
{
    // DEBUG_START;

    // Device
    JsonObject device = json.createNestedObject(F("device"));
    device["id"]           = config.id;

    // Network
    JsonObject network = json.createNestedObject(F("network"));
    network["ssid"]        = config.ssid;
    network["passphrase"]  = config.passphrase;
    network["hostname"]    = config.hostname;
#ifdef ARDUINO_ARCH_ESP8266
    IPAddress Temp = config.ip;
    network["ip"]      = Temp.toString ();
    Temp = config.netmask;
    network["netmask"] = Temp.toString ();
    Temp = config.gateway;
    network["gateway"] = Temp.toString ();
#else
    network["ip"]      = config.ip.toString ();
    network["netmask"] = config.netmask.toString ();
    network["gateway"] = config.gateway.toString ();
#endif // !def ARDUINO_ARCH_ESP8266

    network["dhcp"]        = config.UseDhcp;
    network["sta_timeout"] = config.sta_timeout;

    network["ap_fallback"] = config.ap_fallbackIsEnabled;
    network["ap_timeout"]  = config.ap_timeout;
    network["ap_reboot"]   = config.RebootOnWiFiFailureToConnect;

    // DEBUG_END;
} // GetConfig

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
    // DEBUG_V ("ConfigFileName: " + ConfigFileName);
    // DEBUG_V ("DataToSave: " + DataToSave);
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
    // do we need to save the current config?
    if (0 != ConfigSaveNeeded)
    {
        ConfigSaveNeeded = 0;
        SaveConfig ();
    } // done need to save the current config

#ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_reset ();
#else
    ESP.wdtFeed ();
#endif // def ARDUINO_ARCH_ESP32

    // Keep the WiFi Open
    WiFiMgr.Poll ();

    // Process input data
    InputMgr.Process ();

    // Render output
    OutputMgr.Render();

    WebMgr.Process ();

    // need to keep the rx pipeline empty
    size_t BytesToDiscard = min (1000, LOG_PORT.available ());
    while (0 < BytesToDiscard)
    {
        // DEBUG_V (String("BytesToDiscard: ") + String(BytesToDiscard));
        BytesToDiscard--;
        LOG_PORT.read();
#ifdef ARDUINO_ARCH_ESP32
        esp_task_wdt_reset ();
#else
        ESP.wdtFeed ();
#endif // def ARDUINO_ARCH_ESP32
    } // end discard loop

    // Reboot handler
    if (reboot)
    {
        LOG_PORT.println (F ("Internal Reboot Requested. Rebooting Now"));
        delay (REBOOT_DELAY);
        ESP.restart ();
    }

    if (true == ResetWiFi)
    {
        ResetWiFi = false;
        WiFiMgr.reset ();
    }

} // loop
