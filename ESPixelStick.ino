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
#include "src/FileMgr.hpp"
#include <Int64String.h>

// Input modules
#include "src/input/InputMgr.hpp"

// Output modules
#include "src/output/OutputMgr.hpp"

// WiFi interface
#include "src/WiFiMgr.hpp"

// WEB interface
#include "src/WebMgr.hpp"

// File System Interface
#include "src/FileMgr.hpp"

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

const String VERSION = "4.0-dev (NOT STABLE)";
const String BUILD_DATE = String(__DATE__) + " - " + String(__TIME__);
const String CurrentConfigVersion = "1";

// json object names.
const String DEVICE_NAME =      ("device");
const String VERSION_NAME =     ("cfgver");
const String ID_NAME =          ("id");
const String NETWORK_NAME =     ("network");
const String SSID_NAME =        ("ssid");
const String PASSPHRASE_NAME =  ("passphrase");
const String IP_NAME =          ("ip");
const String NETMASK_NAME =     ("netmask");
const String GATEWAY_NAME =     ("gateway");
const String HOSTNAME_NAME =    ("hostname");
const String DHCP_NAME =        ("dhcp");
const String STA_TIMEOUT_NAME = ("sta_timeout");
const String AP_FALLBACK_NAME = ("ap_fallback");
const String AP_REBOOT_NAME   = ("ap_reboot");
const String AP_TIMEOUT_NAME  = ("ap_timeout");

config_t config;                 // Current configuration
bool     reboot = false;         // Reboot flag
uint32_t lastUpdate;             // Update timeout tracker
bool     ConfigSaveNeeded = false;
bool     ResetWiFi = false;

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
    FileMgr.Begin ();

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
    DEBUG_START;

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
        ConfigSaveNeeded = true;
    }

    if (0 == config.hostname.length ())
    {
        config.hostname = "esps-" + String (chipId);
        // DEBUG_V ();
        ConfigSaveNeeded = true;
    }

    ConfigSaveNeeded |= WiFiMgr.ValidateConfig (&config);

    DEBUG_END;
} // validateConfig

/// Deserialize device confiugration JSON to config structure - returns true if config change detected
boolean dsDevice(JsonObject & json)
{
    // DEBUG_START;

    boolean ConfigChanged = false;
    if (json.containsKey(DEVICE_NAME))
    {
        JsonObject JsonDeviceConfig = json[DEVICE_NAME];

        ConfigChanged |= setFromJSON (config.id,        JsonDeviceConfig, ID_NAME);
                         // setFromJSON (ConfigSaveNeeded, JsonDeviceConfig, "ConfigSaveNeeded");
    }
    else
    {
        LOG_PORT.println(F("No device settings found."));
    }

    // DEBUG_END;

    return ConfigChanged;
} // dsDevice

/// Deserialize network confiugration JSON to config structure - returns true if config change detected
boolean dsNetwork(JsonObject & json)
{
    // DEBUG_START;

    boolean ConfigChanged = false;
    if (json.containsKey(NETWORK_NAME))
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

        JsonObject network = json[NETWORK_NAME];
        ConfigChanged |= setFromJSON (config.ssid,                         network, SSID_NAME);
        ConfigChanged |= setFromJSON (config.passphrase,                   network, PASSPHRASE_NAME);
        ConfigChanged |= setFromJSON(ip,                                   network, IP_NAME);
        ConfigChanged |= setFromJSON(netmask,                              network, NETMASK_NAME);
        ConfigChanged |= setFromJSON(gateway,                              network, GATEWAY_NAME);
        ConfigChanged |= setFromJSON(config.hostname,                      network, HOSTNAME_NAME);
        ConfigChanged |= setFromJSON(config.UseDhcp,                       network, DHCP_NAME);
        ConfigChanged |= setFromJSON(config.sta_timeout,                   network, STA_TIMEOUT_NAME);
        ConfigChanged |= setFromJSON(config.ap_fallbackIsEnabled,          network, AP_FALLBACK_NAME);
        ConfigChanged |= setFromJSON(config.ap_timeout,                    network, AP_TIMEOUT_NAME);
        ConfigChanged |= setFromJSON (config.RebootOnWiFiFailureToConnect, network, AP_REBOOT_NAME);

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

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    // DEBUG_END;
    return ConfigChanged;
} // dsNetwork

void SetConfig (JsonObject& json)
{
    DEBUG_START;

    ConfigSaveNeeded = deserializeCore (json);

    DEBUG_END;

} // SetConfig

bool deserializeCore (JsonObject & json)
{
    DEBUG_START;

    bool DataHasBeenAccepted = false;

    do // once
    {
        String TempVersion;
        setFromJSON (TempVersion, json, VERSION_NAME);

        if (TempVersion != CurrentConfigVersion)
        {
            // need to do something in the future
            LOG_PORT.println ("Incorrect Version ID found in config");
            ConfigSaveNeeded = true;
            // break;
        }

        ConfigSaveNeeded |= dsDevice  (json);
        ResetWiFi = dsNetwork (json);
        ConfigSaveNeeded |= ResetWiFi;

        DataHasBeenAccepted = true;

    } while (false);

    DEBUG_END;

    return DataHasBeenAccepted;
}

void deserializeCoreHandler (DynamicJsonDocument & jsonDoc)
{
    // DEBUG_START;

    JsonObject json = jsonDoc.as<JsonObject> ();
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

    String temp;
    // DEBUG_V ("");
    if (FileMgr.LoadConfigFile (ConfigFileName, &deserializeCoreHandler))
    {
        ConfigSaveNeeded = false;
        // DEBUG_V ("Validate");
        validateConfig();
    }
    else
    {
        // DEBUG_V ("Load failed, create a new config file and save it");
        SaveConfig();
    }

    // DEBUG_START;
} // loadConfig

void DeleteConfig ()
{
    // DEBUG_START;
    FileMgr.DeleteConfigFile (ConfigFileName);

    // DEBUG_END;

} // DeleteConfig

void GetConfig (JsonObject & json)
{
    // DEBUG_START;

    // Device
    JsonObject device = json.createNestedObject(DEVICE_NAME);
    device[ID_NAME]      = config.id;
    device[VERSION_NAME] = CurrentConfigVersion;

    // Network
    JsonObject network = json.createNestedObject(NETWORK_NAME);
    network[SSID_NAME]        = config.ssid;
    network[PASSPHRASE_NAME]  = config.passphrase;
    network[HOSTNAME_NAME]    = config.hostname;
#ifdef ARDUINO_ARCH_ESP8266
    IPAddress Temp = config.ip;
    network[IP_NAME]      = Temp.toString ();
    Temp = config.netmask;
    network[NETMASK_NAME] = Temp.toString ();
    Temp = config.gateway;
    network[GATEWAY_NAME] = Temp.toString ();
#else
    network[IP_NAME]      = config.ip.toString ();
    network[NETMASK_NAME] = config.netmask.toString ();
    network[GATEWAY_NAME] = config.gateway.toString ();
#endif // !def ARDUINO_ARCH_ESP8266

    network[DHCP_NAME]        = config.UseDhcp;
    network[STA_TIMEOUT_NAME] = config.sta_timeout;

    network[AP_FALLBACK_NAME] = config.ap_fallbackIsEnabled;
    network[AP_TIMEOUT_NAME]  = config.ap_timeout;
    network[AP_REBOOT_NAME]   = config.RebootOnWiFiFailureToConnect;

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
    FileMgr.SaveConfigFile(ConfigFileName, DataToSave);

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
    if (true == ConfigSaveNeeded)
    {
        ConfigSaveNeeded = false;
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
