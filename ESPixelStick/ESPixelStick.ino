#include <Arduino.h>

// Core
#include "src/ESPixelStick.h"
#include "src/EFUpdate.h"
#include <Int64String.h>

// Input modules
#include "src/input/InputMgr.hpp"

// Output modules
#include "src/output/OutputMgr.hpp"

// Network interface
#include "src/network/NetworkMgr.hpp"

// WEB interface
#include "src/WebMgr.hpp"

// File System Interface
#include "src/FileMgr.hpp"

// Services
#include "src/service/FPPDiscovery.h"
#include <TimeLib.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <Hash.h>
extern "C"
{
#include <user_interface.h>
} // extern "C"

#elif defined ARDUINO_ARCH_ESP32
// ESP32 user_interface is now built in
#include <Update.h>
#include <esp_task_wdt.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#else
#error "Unsupported CPU type."
#endif

// Debugging support
#if defined(DEBUG)
extern "C" void system_set_os_print(uint8 onoff);
extern "C" void ets_install_putc1(void *routine);

static void _u0_putc(char c)
{
    while (((U0S >> USTXC) & 0x7F) == 0x7F)
        ;
    U0F = c;
}
#endif

/////////////////////////////////////////////////////////
//
//  Globals
//
/////////////////////////////////////////////////////////
// OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#ifdef ESPS_VERSION
const String VERSION = STRING(ESPS_VERSION);
#else
const String VERSION = "4.x-jmt";
#endif
int period = 5000;
unsigned long time_now = 0;
const String ConfigFileName = "/config.json";
const String BUILD_DATE = String(__DATE__) + " - " + String(__TIME__);
const uint8_t CurrentConfigVersion = 1;

config_t config; // Current configuration
static const uint32_t NotRebootingValue = uint32_t(-1);
uint32_t RebootCount = NotRebootingValue;
uint32_t lastUpdate; // Update timeout tracker
bool ResetWiFi = false;
bool IsBooting = true; // Configuration initialization flag
time_t ConfigLoadNeeded = NO_CONFIG_NEEDED;
bool ConfigSaveNeeded = false;
uint32_t DiscardedRxData = 0;

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

#define NO_CONFIG_NEEDED time_t(-1)

void ScheduleLoadConfig() { ConfigLoadNeeded = now(); }
void LoadConfig();
void GetConfig(JsonObject &json);
void GetDriverName(String &Name) { Name = F("ESP"); }

/// Radio configuration
/** ESP8266 radio configuration routines that are executed at startup. */
#ifdef ARDUINO_ARCH_ESP8266
RF_PRE_INIT()
{
    system_phy_set_powerup_option(3); // Do full RF calibration on power-up
}
#endif

void TestHeap(uint32_t Id)
{
    DEBUG_V(String("Test ID: ") + String(Id));
    DEBUG_V(String("Allocate JSON document. Size = ") + String(20 * 1024));
    DEBUG_V(String("Heap Before: ") + ESP.getFreeHeap());
    {
        DynamicJsonDocument jsonDoc(20 * 1024);
    }
    DEBUG_V(String(" Heap After: ") + ESP.getFreeHeap());
}

/// Arduino Setup
/** Arduino based setup code that is executed at startup. */
void setup()
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        for (;;)
            ; // Don't proceed, loop forever
    }
#ifdef DEBUG_GPIO
    pinMode(DEBUG_GPIO, OUTPUT);
    digitalWrite(DEBUG_GPIO, HIGH);
#endif // def DEBUG_GPIO
    display.clearDisplay();

    display.setTextSize(2);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.println(F("Loading.."));
    display.display();
    config.BlankDelay = 5;
#ifdef ARDUINO_ARCH_ESP32
    // disable brownout detector
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif // def ARDUINO_ARCH_ESP32

    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

    // DEBUG_START;
    // DEBUG_HW_START;
#if defined(DEBUG)
    ets_install_putc1((void *)&_u0_putc);
    system_set_os_print(1);
#endif

    // Dump version and build information
    LOG_PORT.println();
    logcon(String(CN_ESPixelStick) + " v" + VERSION + " (" + BUILD_DATE + ")");
#ifdef ARDUINO_ARCH_ESP8266
    logcon(ESP.getFullVersion());
#else
    logcon(ESP.getSdkVersion());
#endif

    // TestHeap(uint32_t(10));
    // DEBUG_V("");
    FileMgr.Begin();

    // Load configuration from the File System and set Hostname
    // TestHeap(uint32_t(15));
    // DEBUG_V(String("LoadConfig Heap: ") + String(ESP.getFreeHeap()));
    LoadConfig();

    // TestHeap(uint32_t(20));
    // DEBUG_V(String("InputMgr Heap: ") + String(ESP.getFreeHeap()));
    // connect the input processing to the output processing.
    InputMgr.Begin(0);

    // TestHeap(uint32_t(30));
    // DEBUG_V(String("OutputMgr Heap: ") + String(ESP.getFreeHeap()));
    // Set up the output manager to start sending data to the serial ports
    OutputMgr.Begin();

    // TestHeap(uint32_t(40));
    // DEBUG_V(String("NetworkMgr Heap: ") + String(ESP.getFreeHeap()));
    NetworkMgr.Begin();

    // TestHeap(uint32_t(50));
    // DEBUG_V(String("WebMgr Heap: ") + String(ESP.getFreeHeap()));
    // Configure and start the web server
    WebMgr.Begin(&config);

    // DEBUG_V(String("FPPDiscovery Heap: ") + String(ESP.getFreeHeap()));
    FPPDiscovery.begin();

    // DEBUG_V(String("Final Heap: ") + String(ESP.getFreeHeap()));

#ifdef ARDUINO_ARCH_ESP8266
    // * ((volatile uint32_t*)0x60000900) &= ~(1); // Hardware WDT OFF
    ESP.wdtEnable(2000); // 2 seconds
#else
    esp_task_wdt_init(5, true);
#endif

    WebMgr.CreateAdminInfoFile();

    // Done with initialization
    IsBooting = false;

    // DEBUG_END;

} // setup

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

/// Configuration Validations
/** Validates the config_t (core) configuration structure and forces defaults for invalid entries */
bool validateConfig()
{
    // DEBUG_START;

    bool configValid = true;

    // Device defaults
    if (!config.id.length())
    {
        config.id = F("ESPixelStick");
        configValid = false;
        // DEBUG_V ();
    }

    return configValid;

    // DEBUG_END;
} // validateConfig

/// Deserialize device configuration JSON to config structure - returns true if config change detected
bool dsDevice(JsonObject &json)
{
    // DEBUG_START;
    // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
    // PrettyPrint (json, "dsDevice");

    bool ConfigChanged = false;
    if (json.containsKey(CN_device))
    {
        JsonObject JsonDeviceConfig = json[CN_device];

        // TODO: Add configuration upgrade handling - cfgver moved to root level
        ConfigChanged |= setFromJSON(config.id, JsonDeviceConfig, CN_id);
        ConfigChanged |= setFromJSON(config.BlankDelay, JsonDeviceConfig, CN_blanktime);
    }
    else
    {
        logcon(String(F("No device settings found.")));
    }

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    // DEBUG_END;

    return ConfigChanged;
} // dsDevice

// Save the config and schedule a load operation
void SetConfig(const char *DataString)
{
    // DEBUG_START;

    // TODO: This is being called from c_WebMgr::processCmdSet() with no validation
    //       of the data. Chance for 3rd party software to muck up the configuraton
    //       if they send bad json data.

    FileMgr.SaveConfigFile(ConfigFileName, DataString);
    ScheduleLoadConfig();

    // DEBUG_END;

} // SetConfig

bool deserializeCore(JsonObject &json)
{
    // DEBUG_START;

    bool DataHasBeenAccepted = false;

    // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
    // PrettyPrint (json, "Main Config");
    JsonObject DeviceConfig;

    do // once
    {
        // was this saved by the ESP itself
        if (json.containsKey(CN_system))
        {
            // DEBUG_V("");
            DeviceConfig = json[CN_system];
        }
        // is this an initial config from the flash tool?
        else if (json.containsKey(CN_init) || json.containsKey(CN_network))
        {
            // trigger a save operation
            ConfigSaveNeeded = true;
            logcon(String(F("Processing Flash Tool config")));
            DeviceConfig = json;
        }
        else
        {
            logcon(String(F("Could not find system config")));
            ConfigSaveNeeded = true;
            break;
        }
        // DEBUG_V("");

        if (DeviceConfig.containsKey(CN_cfgver))
        {
            // DEBUG_V("");
            uint8_t TempVersion = uint8_t(-1);
            setFromJSON(TempVersion, DeviceConfig, CN_cfgver);
            if (TempVersion != CurrentConfigVersion)
            {
                // TODO: Add configuration update handler
                logcon(String(F("Incorrect Config Version ID")));
            }
        }
        else
        {
            logcon(String(F("Missing Config Version ID")));
            // break; // ignoring this error for now.
        }

        dsDevice(DeviceConfig);
        // DEBUG_V("");
        FileMgr.SetConfig(DeviceConfig);
        // DEBUG_V("");
        ConfigSaveNeeded |= NetworkMgr.SetConfig(DeviceConfig);
        // DEBUG_V("");
        DataHasBeenAccepted = true;

    } while (false);

    // DEBUG_END;

    return DataHasBeenAccepted;
}

void deserializeCoreHandler(DynamicJsonDocument &jsonDoc)
{
    // DEBUG_START;

    // extern void PrettyPrint(DynamicJsonDocument & jsonStuff, String Name);
    // PrettyPrint(jsonDoc, "deserializeCoreHandler");

    JsonObject json = jsonDoc.as<JsonObject>();
    deserializeCore(json);

    // DEBUG_END;
}

// Save configuration JSON file
void SaveConfig()
{
    // DEBUG_START;

    ConfigSaveNeeded = false;

    // Create buffer and root object
    DynamicJsonDocument jsonConfigDoc(2048);
    JsonObject JsonConfig = jsonConfigDoc.createNestedObject(CN_system);

    GetConfig(JsonConfig);

    FileMgr.SaveConfigFile(ConfigFileName, jsonConfigDoc);

    // DEBUG_END;
} // SaveConfig

/// Load configuration file
/** Loads and validates the JSON configuration file from the file system.
 *  If no configuration file is found, a new one will be created.
 */
void LoadConfig()
{
    // DEBUG_START;

    ConfigLoadNeeded = NO_CONFIG_NEEDED;

    String temp;
    // DEBUG_V ("");
    FileMgr.LoadConfigFile(ConfigFileName, &deserializeCoreHandler);

    ConfigSaveNeeded |= !validateConfig();

    // DEBUG_END;
} // loadConfig

void DeleteConfig()
{
    // DEBUG_START;
    FileMgr.DeleteConfigFile(ConfigFileName);

    // DEBUG_END;

} // DeleteConfig

void GetConfig(JsonObject &json)
{
    // DEBUG_START;

    // Config Version
    json[CN_cfgver] = CurrentConfigVersion;

    // Device
    JsonObject device = json.createNestedObject(CN_device);
    device[CN_id] = config.id;
    device[CN_blanktime] = config.BlankDelay;

    FileMgr.GetConfig(device);

    NetworkMgr.GetConfig(json);

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

    GetConfig(JsonConfig);

    if (pretty)
    {
        serializeJsonPretty(JsonConfig, jsonConfigString);
    }
    else
    {
        serializeJson(JsonConfig, jsonConfigString);
    }

    // DEBUG_V (String ("jsonConfigString: ") + jsonConfigString);

    // DEBUG_END;

    return jsonConfigString;
} // serializeCore

/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
/// Main Loop
/** Arduino based main loop */
// uint32_t HeapTime = 100;

void loop()
{






    FeedWDT();

    // Keep the Network Open
    NetworkMgr.Poll();

    // Process input data
    InputMgr.Process();

    // Poll output
    OutputMgr.Poll();

    WebMgr.Process();

    // need to keep the rx pipeline empty
    size_t BytesToDiscard = min(100, LOG_PORT.available());
    DiscardedRxData += BytesToDiscard;
    while (0 < BytesToDiscard)
    {
        FeedWDT();

        // DEBUG_V (String("BytesToDiscard: ") + String(BytesToDiscard));
        BytesToDiscard--;
        LOG_PORT.read();
    } // end discard loop

    // Reboot handler
    if (NotRebootingValue != RebootCount)
    {
        if (0 == --RebootCount)
        {
            logcon(String(CN_stars) + CN_minussigns + F("Internal Reboot Requested. Rebooting Now"));
            delay(REBOOT_DELAY);
            ESP.restart();
        }
    }

    if (NO_CONFIG_NEEDED != ConfigLoadNeeded)
    {
        if (abs(now() - ConfigLoadNeeded) > LOAD_CONFIG_DELAY)
        {
            FeedWDT();
            LoadConfig();
        }
    }

    if (ConfigSaveNeeded)
    {
        FeedWDT();
        SaveConfig();
    }
    if(millis() >= time_now + period){
        time_now += period;
    display.clearDisplay();
    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 10);             // Start at top-left corner
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.print("HOST: ");
    display.println(WiFi.getHostname());
    display.println("");
    display.display();
    }
} // loop

bool RebootInProgress()
{
    return RebootCount != NotRebootingValue;
}

void RequestReboot(uint32_t LoopDelay)
{
    RebootCount = LoopDelay;

    InputMgr.SetOperationalState(false);
    OutputMgr.PauseOutputs(true);

} // RequestReboot

void _logcon(String &DriverName, String Message)
{
    char Spaces[7];
    memset(Spaces, ' ', sizeof(Spaces));
    if (DriverName.length() < (sizeof(Spaces) - 1))
    {
        Spaces[(sizeof(Spaces) - 1) - DriverName.length()] = '\0';
    }
    else
    {
        Spaces[0] = '\0';
    }

    LOG_PORT.println(String(F("[")) + String(Spaces) + DriverName + F("] ") + Message);
    LOG_PORT.flush();
} // logcon

void FeedWDT()
{
#ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_reset();
#else
    ESP.wdtFeed();
#endif // def ARDUINO_ARCH_ESP32
}
