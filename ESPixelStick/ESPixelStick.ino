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

// Core
#include "src/ESPixelStick.h"
#include "src/EFUpdate.h"
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

const String VERSION = "4.0-dev (NOT STABLE)";
const String BUILD_DATE = String(__DATE__) + " - " + String(__TIME__);
const String CurrentConfigVersion = "1";

config_t config;                // Current configuration
bool     reboot = false;        // Reboot flag
uint32_t lastUpdate;            // Update timeout tracker
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
    config.ssid.clear ();
    config.passphrase.clear ();
    config.ip         = IPAddress ((uint32_t)0);
    config.netmask    = IPAddress ((uint32_t)0);
    config.gateway    = IPAddress ((uint32_t)0);
    config.UseDhcp    = true;
    config.ap_fallbackIsEnabled = true;
    config.RebootOnWiFiFailureToConnect = true;
    config.ap_timeout  = AP_TIMEOUT;
    config.sta_timeout = CLIENT_TIMEOUT;

    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

    // DEBUG_START;
    // DEBUG_HW_START;
#if defined(DEBUG)
    ets_install_putc1((void *) &_u0_putc);
    system_set_os_print(1);
#endif

    // Dump version and build information
    LOG_PORT.println("");
    LOG_PORT.println( String(CN_ESPixelStick) + " v" + VERSION + "(" + BUILD_DATE + ")");
#ifdef ARDUINO_ARCH_ESP8266
    LOG_PORT.println (String (F ("ESP Version: ")) + ESP.getFullVersion ());
#else
    LOG_PORT.println (String(F ("ESP Version: ")) + ESP.getSdkVersion ());
#endif

    // DEBUG_V ("");
    FileMgr.Begin ();

    // Load configuration from the File System and set Hostname
    loadConfig();
    // DEBUG_V ("");

    // Set up the output manager to start sending data to the serial ports
    OutputMgr.Begin ();
    // DEBUG_V ("");

    // connect the input processing to the output processing.
    InputMgr.Begin (OutputMgr.GetBufferAddress (), OutputMgr.GetBufferUsedSize ());

    // Wifi will be reset in the main loop since we just booted and de-serialized the config
    WiFiMgr.Begin (& config);
    // DEBUG_V ("");

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
    }

    if (0 == config.hostname.length ())
    {
        config.hostname = "esps-" + String (chipId);
        // DEBUG_V ();
    }

    WiFiMgr.ValidateConfig (&config);

    // DEBUG_END;
} // validateConfig

/// Deserialize device confiugration JSON to config structure - returns true if config change detected
bool dsDevice(JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;
    if (json.containsKey(CN_device))
    {
        JsonObject JsonDeviceConfig = json[CN_device];

        String TempVersion;
        setFromJSON (TempVersion, JsonDeviceConfig, CN_cfgver);

        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (CurrentConfigVersion));
        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
        // PrettyPrint (JsonDeviceConfig, "Main Config");

        if (TempVersion != CurrentConfigVersion)
        {
            // need to do something in the future
            LOG_PORT.println ("Incorrect Device Config Version ID found in config");
            // break;
        }

        ConfigChanged |= setFromJSON (config.id,        JsonDeviceConfig, CN_id);
    }
    else
    {
        LOG_PORT.println(F ("No device settings found."));
    }

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    // DEBUG_END;

    return ConfigChanged;
} // dsDevice

/// Deserialize network confiugration JSON to config structure - returns true if config change detected
bool dsNetwork(JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;
    if (json.containsKey(CN_network))
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

        JsonObject network = json[CN_network];

        String TempVersion;
        setFromJSON (TempVersion, network, CN_cfgver);

        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (CurrentConfigVersion));
        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
        // PrettyPrint (network, "Main Config");

        if (TempVersion != CurrentConfigVersion)
        {
            // need to do something in the future
            LOG_PORT.println ("Incorrect Version ID found in config");
            // break;
        }

        ConfigChanged |= setFromJSON (config.ssid,                         network, CN_ssid);
        ConfigChanged |= setFromJSON (config.passphrase,                   network, CN_passphrase);
        ConfigChanged |= setFromJSON (ip,                                  network, CN_ip);
        ConfigChanged |= setFromJSON (netmask,                             network, CN_netmask);
        ConfigChanged |= setFromJSON (gateway,                             network, CN_gateway);
        ConfigChanged |= setFromJSON (config.hostname,                     network, CN_hostname);
        ConfigChanged |= setFromJSON (config.UseDhcp,                      network, CN_dhcp);
        ConfigChanged |= setFromJSON (config.sta_timeout,                  network, CN_sta_timeout);
        ConfigChanged |= setFromJSON (config.ap_fallbackIsEnabled,         network, CN_ap_fallback);
        ConfigChanged |= setFromJSON (config.ap_timeout,                   network, CN_ap_timeout);
        ConfigChanged |= setFromJSON (config.RebootOnWiFiFailureToConnect, network, CN_ap_reboot);

        // DEBUG_V ("     ip: " + ip);
        // DEBUG_V ("gateway: " + gateway);
        // DEBUG_V ("netmask: " + netmask);

        config.ip.fromString (ip);
        config.gateway.fromString (gateway);
        config.netmask.fromString (netmask);
    }
    else
    {
        LOG_PORT.println(F ("No network settings found."));
    }

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    // DEBUG_END;
    return ConfigChanged;
} // dsNetwork

void SetConfig (JsonObject & json, const char * DataString)
{
    // DEBUG_START;

    FileMgr.SaveConfigFile (ConfigFileName, DataString);
    deserializeCore (json);

    // DEBUG_END;

} // SetConfig

bool deserializeCore (JsonObject & json)
{
    // DEBUG_START;

    bool DataHasBeenAccepted = false;

    // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
    // PrettyPrint (json, "Main Config");

    do // once
    {
        dsDevice  (json);
        FileMgr.SetConfig (json);
        ResetWiFi = dsNetwork (json);
        DataHasBeenAccepted = true;
    } while (false);

    // DEBUG_END;

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

    String temp;
    // DEBUG_V ("");
    if (FileMgr.LoadConfigFile (ConfigFileName, &deserializeCoreHandler))
    {
        // DEBUG_V ("Validate");
        validateConfig();
    }
    else
    {
        // DEBUG_V ("Load failed, create a new config file and save it");
        CreateNewConfig();
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
    JsonObject device = json.createNestedObject(CN_device);
    device[CN_id]     = config.id;
    device[CN_cfgver] = CurrentConfigVersion;

    FileMgr.GetConfig (device);

    // Network
    JsonObject network = json.createNestedObject(CN_network);
    network[CN_cfgver]     = CurrentConfigVersion;
    network[CN_ssid]       = config.ssid;
    network[CN_passphrase] = config.passphrase;
    network[CN_hostname] = config.hostname;
#ifdef ARDUINO_ARCH_ESP8266
    IPAddress Temp = config.ip;
    network[CN_ip]      = Temp.toString ();
    Temp = config.netmask;
    network[CN_netmask] = Temp.toString ();
    Temp = config.gateway;
    network[CN_gateway] = Temp.toString ();
#else
    network[CN_ip]      = config.ip.toString ();
    network[CN_netmask] = config.netmask.toString ();
    network[CN_gateway] = config.gateway.toString ();
#endif // !def ARDUINO_ARCH_ESP8266

    network[CN_dhcp]        = config.UseDhcp;
    network[CN_sta_timeout] = config.sta_timeout;

    network[CN_ap_fallback] = config.ap_fallbackIsEnabled;
    network[CN_ap_timeout]  = config.ap_timeout;
    network[CN_ap_reboot]   = config.RebootOnWiFiFailureToConnect;

    // DEBUG_END;
} // GetConfig

// Serialize the current config into a JSON string
String serializeCore(bool pretty)
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
void CreateNewConfig()
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
} // CreateNewConfig

/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
/// Main Loop
/** Arduino based main loop */
void loop()
{
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
        LOG_PORT.println (String(CN_stars) + CN_minussigns + F ("Internal Reboot Requested. Rebooting Now"));
        delay (REBOOT_DELAY);
        ESP.restart ();
    }

    if (true == ResetWiFi)
    {
        ResetWiFi = false;
        WiFiMgr.reset ();
    }

} // loop
