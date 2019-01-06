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

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = "ENTER_SSID_HERE";
const char passphrase[] = "ENTER_PASSPHRASE_HERE";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <ESPAsyncE131.h>
#include <Hash.h>
#include <SPI.h>
#include "ESPixelStick.h"
#include "EFUpdate.h"
#include "wshandler.h"
#include "gamma.h"

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
//  Globals
//
/////////////////////////////////////////////////////////

// MQTT State
const char MQTT_SET_COMMAND_TOPIC[] = "/set";

// MQTT Payloads by default (on/off)
const char LIGHT_ON[] = "ON";
const char LIGHT_OFF[] = "OFF";

// Configuration file
const char CONFIG_FILE[] = "/config.json";

ESPAsyncE131        e131(10);       // ESPAsyncE131 with X buffers
config_t            config;         // Current configuration
uint32_t            *seqError;      // Sequence error tracking for each universe
uint16_t            uniLast = 1;    // Last Universe to listen for
bool                reboot = false; // Reboot flag
AsyncWebServer      web(HTTP_PORT); // Web Server
AsyncWebSocket      ws("/ws");      // Web Socket Plugin
uint8_t             *seqTracker;    // Current sequence numbers for each Universe */
uint32_t            lastUpdate;     // Update timeout tracker
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
Ticker              wifiTicker; // Ticker to handle WiFi
Ticker              idleTicker; // Ticker for effect on idle
AsyncMqttClient     mqtt;       // MQTT object
Ticker              mqttTicker; // Ticker to handle MQTT
EffectEngine        effects;    // Effects Engine

// Output Drivers
#if defined(ESPS_MODE_PIXEL)
PixelDriver     pixels;         // Pixel object
#elif defined(ESPS_MODE_SERIAL)
SerialDriver    serial;         // Serial object
#else
#error "No valid output mode defined."
#endif

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
void initWifi();
void initWeb();
void updateConfig();

// Radio config
RF_PRE_INIT() {
    //wifi_set_phy_mode(PHY_MODE_11G);    // Force 802.11g mode
    system_phy_set_powerup_option(31);  // Do full RF calibration on power-up
    system_phy_set_max_tpw(82);         // Set max TX power
}

void setup() {
    // Configure SDK params
    wifi_set_sleep_type(NONE_SLEEP_T);

    // Initial pin states
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

#if defined(DEBUG)
    ets_install_putc1((void *) &_u0_putc);
    system_set_os_print(1);
#endif

    // Enable SPIFFS
    SPIFFS.begin();

    // Set default data source to E131
    config.ds = DataSource::E131;

    LOG_PORT.println("");
    LOG_PORT.print(F("ESPixelStick v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.print(F(" ("));
    for (uint8_t i = 0; i < strlen_P(BUILD_DATE); i++)
        LOG_PORT.print((char)(pgm_read_byte(BUILD_DATE + i)));
    LOG_PORT.println(")");
    LOG_PORT.println(ESP.getFullVersion());

    // Load configuration from SPIFFS and set Hostname
    loadConfig();
    if (config.hostname)
        WiFi.hostname(config.hostname);

#if defined (ESPS_MODE_PIXEL)
    pixels.setPin(DATA_PIN);
    updateConfig();

    // Do one effects cycle as early as possible
    if (config.ds == DataSource::WEB) {
        effects.run();
    }
    // set the effect idle timer
    idleTicker.attach(config.effect_idletimeout, idleTimeout);

    pixels.show();
#else
    updateConfig();
#endif

    // Setup WiFi Handlers
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);

    // Setup MQTT Handlers
    if (config.mqtt) {
        mqtt.onConnect(onMqttConnect);
        mqtt.onDisconnect(onMqttDisconnect);
        mqtt.onMessage(onMqttMessage);
        mqtt.setServer(config.mqtt_ip.c_str(), config.mqtt_port);
        // Unset clean session (defaults to true) so we get retained messages of QoS > 0
        mqtt.setCleanSession(config.mqtt_clean);
        if (config.mqtt_user.length() > 0)
            mqtt.setCredentials(config.mqtt_user.c_str(), config.mqtt_password.c_str());
    }

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
        } else {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart();
        }
    }

    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

    // Configure and start the web server
    initWeb();

    // Setup E1.31
    if (config.multicast) {
        if (e131.begin(E131_MULTICAST, config.universe,
                uniLast - config.universe + 1)) {
            LOG_PORT.println(F("- Multicast Enabled"));
        }  else {
            LOG_PORT.println(F("*** MULTICAST INIT FAILED ****"));
        }
    } else {
        if (e131.begin(E131_UNICAST)) {
            LOG_PORT.print(F("- Unicast port: "));
            LOG_PORT.println(E131_DEFAULT_PORT);
        } else {
            LOG_PORT.println(F("*** UNICAST INIT FAILED ****"));
        }
    }

    // Configure the outputs
#if defined (ESPS_MODE_PIXEL)
    pixels.setPin(DATA_PIN);
    updateConfig();
    pixels.show();
#else
    updateConfig();
#endif
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
        if (millis() - timeout > CONNECT_TIMEOUT) {
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
        WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
        );
        LOG_PORT.print(F("Connecting with Static IP"));
    }
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
    LOG_PORT.println("");
    LOG_PORT.print(F("Connected with IP: "));
    LOG_PORT.println(WiFi.localIP());

    // Setup MQTT connection if enabled
    if (config.mqtt)
        connectToMqtt();

    // Setup mDNS / DNS-SD
    //TODO: Reboot or restart mdns when config.id is changed?
     char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    MDNS.setInstanceName(String(config.id + " (" + String(chipId) + ")").c_str());
    if (MDNS.begin(config.hostname.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
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

void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event) {
    LOG_PORT.println(F("*** WiFi Disconnected ***"));

    // Pause MQTT reconnect while WiFi is reconnecting
    mqttTicker.detach();
    wifiTicker.once(2, connectWifi);
}

// Subscribe to "n" universes, starting at "universe"
void multiSub() {
    uint8_t count;
    ip_addr_t ifaddr;
    ip_addr_t multicast_addr;

    count = uniLast - config.universe + 1;
    ifaddr.addr = static_cast<uint32_t>(WiFi.localIP());
    for (uint8_t i = 0; i < count; i++) {
        multicast_addr.addr = static_cast<uint32_t>(IPAddress(239, 255,
                (((config.universe + i) >> 8) & 0xff),
                (((config.universe + i) >> 0) & 0xff)));
        igmp_joingroup(&ifaddr, &multicast_addr);
    }
}

/////////////////////////////////////////////////////////
//
//  MQTT Section
//
/////////////////////////////////////////////////////////

void connectToMqtt() {
    LOG_PORT.print(F("- Connecting to MQTT Broker "));
    LOG_PORT.println(config.mqtt_ip);
    mqtt.connect();
}

void onMqttConnect(bool sessionPresent) {
    LOG_PORT.println(F("- MQTT Connected"));

    // Get retained MQTT state
    mqtt.subscribe(config.mqtt_topic.c_str(), 0);
    mqtt.unsubscribe(config.mqtt_topic.c_str());

    // Setup subscriptions
    mqtt.subscribe(String(config.mqtt_topic + MQTT_SET_COMMAND_TOPIC).c_str(), 0);

    // Publish state
    publishState();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    LOG_PORT.println(F("- MQTT Disconnected"));
    if (WiFi.isConnected()) {
        mqttTicker.once(2, connectToMqtt);
    }
}

void onMqttMessage(char* topic, char* payload,
        AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    DynamicJsonBuffer jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(payload);
    bool stateOn = false;

    if (!root.success()) {
        LOG_PORT.println("MQTT: Parsing failed");
        return;
    }

// if its a retained message and we want clean session, ignore it
    if ( properties.retain && config.mqtt_clean ) {
        return;
    }

    if (root.containsKey("state")) {
        if (strcmp(root["state"], LIGHT_ON) == 0) {
            stateOn = true;
        } else if (strcmp(root["state"], LIGHT_OFF) == 0) {
            stateOn = false;
        }
    }

    if (root.containsKey("brightness")) {
        effects.setBrightness(root["brightness"]);
    }

    if (root.containsKey("color")) {
        effects.setColor({
            root["color"]["r"],
            root["color"]["g"],
            root["color"]["b"]
        });
    }

    if (root.containsKey("effect")) {
        // Set the explict effect provided by the MQTT client
        effects.setEffect(root["effect"]);
    }

    if (root.containsKey("reverse")) {
        effects.setReverse(root["reverse"]);
    }

    if (root.containsKey("mirror")) {
        effects.setMirror(root["mirror"]);
    }

    if (root.containsKey("allleds")) {
        effects.setAllLeds(root["allleds"]);
    }

    // Set data source based on state - Fall back to E131 when off
    if (stateOn) {
        config.ds = DataSource::MQTT;
    } else {
        config.ds = DataSource::E131;
        effects.clearAll();
    }

    publishState();
}

void publishState() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["state"] = (config.ds == DataSource::MQTT) ? LIGHT_ON : LIGHT_OFF;
    JsonObject& color = root.createNestedObject("color");
    color["r"] = effects.getColor().r;
    color["g"] = effects.getColor().g;
    color["b"] = effects.getColor().b;
    root["brightness"] = effects.getBrightness();
    if (effects.getEffect() != "") {
        root["effect"] = effects.getEffect();
    }
    root["reverse"] = effects.getReverse();
    root["mirror"] = effects.getMirror();
    root["allleds"] = effects.getAllLeds();

    char buffer[root.measureLength() + 1];
    root.printTo(buffer, sizeof(buffer));
    mqtt.publish(config.mqtt_topic.c_str(), 0, true, buffer);
}

/////////////////////////////////////////////////////////
//
//  Web Section
//
/////////////////////////////////////////////////////////

// Configure and start the web server
void initWeb() {
    // Handle OTA update from asynchronous callbacks
    Update.runAsync(true);

    // Add header for SVG plot support?
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // Setup WebSockets
    ws.onEvent(wsEvent);
    web.addHandler(&ws);

    // Heap status handler
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // JSON Config Handler
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) {
        String jsonString;
        serializeConfig(jsonString, true);
        request->send(200, "text/json", jsonString);
    });

    // gamma debugging Config Handler
    web.on("/gamma", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/plain");
        for (int i = 0; i < 256; i++) {
          response->printf ("%5d,", GAMMA_TABLE[i]);
          if (i % 16 == 15) {
            response->printf("\r\n");
          }
        }
        request->send(response);
    });

    // Firmware upload handler
    web.on("/updatefw", HTTP_POST, [](AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, handle_fw_upload);

    // Static Handler
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");
    //web.serveStatic("/config.json", SPIFFS, "/config.json");

    web.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Page not found");
    });

    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");

    // Config file upload handler
    web.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, handle_config_upload);

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

// Configuration Validations
void validateConfig() {
    // E1.31 Limits
    if (config.universe < 1)
        config.universe = 1;

    if (config.universe_limit > UNIVERSE_MAX || config.universe_limit < 1)
        config.universe_limit = UNIVERSE_MAX;

    if (config.channel_start < 1)
        config.channel_start = 1;
    else if (config.channel_start > config.universe_limit)
        config.channel_start = config.universe_limit;

    // Set default MQTT port if missing
    if (config.mqtt_port == 0)
        config.mqtt_port = MQTT_PORT;

    // Generate default MQTT topic if needed
    if (!config.mqtt_topic.length()) {
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        config.mqtt_topic = "diy/esps/" + String(chipId);
    }

#if defined(ESPS_MODE_PIXEL)
    // Set Mode
    config.devmode.MPIXEL = true;
    config.devmode.MSERIAL = false;

    // Generic channel limits for pixels
    if (config.channel_count % 3)
        config.channel_count = (config.channel_count / 3) * 3;

    if (config.channel_count > PIXEL_LIMIT * 3)
        config.channel_count = PIXEL_LIMIT * 3;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    if (config.groupSize > config.channel_count / 3)
        config.groupSize = config.channel_count / 3;
    else if (config.groupSize < 1)
        config.groupSize = 1;

    // GECE Limits
    if (config.pixel_type == PixelType::GECE) {
        config.pixel_color = PixelColor::RGB;
        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
    }

    // Default gamma value
    if (config.gammaVal <= 0) {
        config.gammaVal = 2.2;
    }

    // Default brightness value
    if (config.briteVal <= 0) {
        config.briteVal = 1.0;
    }
#elif defined(ESPS_MODE_SERIAL)
    // Set Mode
    config.devmode.MPIXEL = false;
    config.devmode.MSERIAL = true;

    // Generic serial channel limits
    if (config.channel_count > RENARD_LIMIT)
        config.channel_count = RENARD_LIMIT;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    if (config.serial_type == SerialType::DMX512 && config.channel_count > UNIVERSE_MAX)
        config.channel_count = UNIVERSE_MAX;

    // Baud rate check
    if (config.baudrate > BaudRate::BR_460800)
        config.baudrate = BaudRate::BR_460800;
    else if (config.baudrate < BaudRate::BR_38400)
        config.baudrate = BaudRate::BR_57600;
#endif

    if (config.effect_idletimeout == 0) {
        config.effect_idletimeout = 10;
        config.effect_idleenabled = false;
    }

    effects.setFromConfig();
    if (config.effect_startenabled) {
        if (effects.isValidEffect(config.effect_name)) {
            effects.setEffect(config.effect_name);

            if ( !config.effect_name.equalsIgnoreCase("disabled")
              && !config.effect_name.equalsIgnoreCase("view") ) {
                config.ds = DataSource::WEB;
            }

        }
    } else {
        effects.setEffect("Disabled");
    }

}

void updateConfig() {
    // Validate first
    validateConfig();

    // Find the last universe we should listen for
    uint16_t span = config.channel_start + config.channel_count - 1;
    if (span % config.universe_limit)
        uniLast = config.universe + span / config.universe_limit;
    else
        uniLast = config.universe + span / config.universe_limit - 1;

    // Setup the sequence error tracker
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = static_cast<uint8_t *>(malloc(uniTotal))))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = static_cast<uint32_t *>(malloc(uniTotal * 4))))
        memset(seqError, 0x00, uniTotal * 4);

    // Zero out packet stats
    e131.stats.num_packets = 0;

    // Initialize for our pixel type
#if defined(ESPS_MODE_PIXEL)
    pixels.begin(config.pixel_type, config.pixel_color, config.channel_count / 3);
//    pixels.setGroupMode(config.groupMode, config.groupSize);
    pixels.setGroup(config.groupSize, config.zigSize);
    pixels.setGamma(config.gamma);
    updateGammaTable(config.gammaVal, config.briteVal);
    effects.begin(&pixels, config.channel_count / 3 / config.groupSize);

#elif defined(ESPS_MODE_SERIAL)
    serial.begin(&SEROUT_PORT, config.serial_type, config.channel_count, config.baudrate);

#endif

    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.println(uniLast);

    // Setup IGMP subscriptions if multicast is enabled
    if (config.multicast)
        multiSub();
}

// De-Serialize Network config
void dsNetworkConfig(JsonObject &json) {
    if (json.containsKey("network")) {
        JsonObject& networkJson = json["network"];

        // Fallback to embedded ssid and passphrase if null in config
        config.ssid = networkJson["ssid"].as<String>();
        if (!config.ssid.length())
            config.ssid = ssid;

        config.passphrase = networkJson["passphrase"].as<String>();
        if (!config.passphrase.length())
            config.passphrase = passphrase;

        // Network
        for (int i = 0; i < 4; i++) {
            config.ip[i] = networkJson["ip"][i];
            config.netmask[i] = networkJson["netmask"][i];
            config.gateway[i] = networkJson["gateway"][i];
        }
        config.dhcp = networkJson["dhcp"];
        config.ap_fallback = networkJson["ap_fallback"];

        // Generate default hostname if needed
        config.hostname = networkJson["hostname"].as<String>();
        if (!config.hostname.length()) {
            char chipId[7] = { 0 };
            snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
            config.hostname = "esps-" + String(chipId);
        }
    }
}

// De-serialize Effect Config
void dsEffectConfig(JsonObject &json) {
    // Effects
    if (json.containsKey("effects")) {
        JsonObject& effectsJson = json["effects"];
        config.effect_name = effectsJson["name"].as<String>();
        config.effect_mirror = effectsJson["mirror"];
        config.effect_allleds = effectsJson["allleds"];
        config.effect_reverse = effectsJson["reverse"];
        config.effect_color = { effectsJson["r"], effectsJson["g"], effectsJson["b"] };
        if (effectsJson.containsKey("brightness"))
            config.effect_brightness = effectsJson["brightness"];
        config.effect_startenabled = effectsJson["startenabled"];
        config.effect_idleenabled = effectsJson["idleenabled"];
        config.effect_idletimeout = effectsJson["idletimeout"];

    }
}

// De-serialize Device Config
void dsDeviceConfig(JsonObject &json) {
    // Device
    config.id = json["device"]["id"].as<String>();

    // E131
    config.universe = json["e131"]["universe"];
    config.universe_limit = json["e131"]["universe_limit"];
    config.channel_start = json["e131"]["channel_start"];
    config.channel_count = json["e131"]["channel_count"];
    config.multicast = json["e131"]["multicast"];

    // MQTT
    if (json.containsKey("mqtt")) {
        JsonObject& mqttJson = json["mqtt"];
        config.mqtt = mqttJson["enabled"];
        config.mqtt_ip = mqttJson["ip"].as<String>();
        config.mqtt_port = mqttJson["port"];
        config.mqtt_user = mqttJson["user"].as<String>();
        config.mqtt_password = mqttJson["password"].as<String>();
        config.mqtt_topic = mqttJson["topic"].as<String>();
        config.mqtt_clean = mqttJson["clean"] | false;
    }

#if defined(ESPS_MODE_PIXEL)
    // Pixel
    config.pixel_type = PixelType(static_cast<uint8_t>(json["pixel"]["type"]));
    config.pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
    config.groupSize = json["pixel"]["groupSize"];
    config.zigSize = json["pixel"]["zigSize"];
    config.gamma = json["pixel"]["gamma"];
    config.gammaVal = json["pixel"]["gammaVal"];
    config.briteVal = json["pixel"]["briteVal"];

#elif defined(ESPS_MODE_SERIAL)
    // Serial
    config.serial_type = SerialType(static_cast<uint8_t>(json["serial"]["type"]));
    config.baudrate = BaudRate(static_cast<uint32_t>(json["serial"]["baudrate"]));
#endif
}

// Load configugration JSON file
void loadConfig() {
    // Zeroize Config struct
    memset(&config, 0, sizeof(config));

    effects.setFromDefaults();

    // Load CONFIG_FILE json. Create and init with defaults if not found
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        config.ssid = ssid;
        config.passphrase = passphrase;
        saveConfig();
    } else {
        // Parse CONFIG_FILE json
        size_t size = file.size();
        if (size > CONFIG_MAX_SIZE) {
            LOG_PORT.println(F("*** Configuration File too large ***"));
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        if (!json.success()) {
            LOG_PORT.println(F("*** Configuration File Format Error ***"));
            return;
        }

        dsNetworkConfig(json);
        dsDeviceConfig(json);
        dsEffectConfig(json);

        LOG_PORT.println(F("- Configuration loaded."));
    }

    // Validate it
    validateConfig();
}

// Serialize the current config into a JSON string
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    // Create buffer and root object
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    // Device
    JsonObject &device = json.createNestedObject("device");
    device["id"] = config.id.c_str();
    device["mode"] = config.devmode.toInt();

    // Network
    JsonObject &network = json.createNestedObject("network");
    network["ssid"] = config.ssid.c_str();
    if (creds)
        network["passphrase"] = config.passphrase.c_str();
    network["hostname"] = config.hostname.c_str();
    JsonArray &ip = network.createNestedArray("ip");
    JsonArray &netmask = network.createNestedArray("netmask");
    JsonArray &gateway = network.createNestedArray("gateway");
    for (int i = 0; i < 4; i++) {
        ip.add(config.ip[i]);
        netmask.add(config.netmask[i]);
        gateway.add(config.gateway[i]);
    }
    network["dhcp"] = config.dhcp;
    network["ap_fallback"] = config.ap_fallback;

    // Effects
    JsonObject &_effects = json.createNestedObject("effects");
    _effects["name"] = config.effect_name;

    _effects["mirror"] = config.effect_mirror;
    _effects["allleds"] = config.effect_allleds;
    _effects["reverse"] = config.effect_reverse;

    _effects["r"] = config.effect_color.r;
    _effects["g"] = config.effect_color.g;
    _effects["b"] = config.effect_color.b;

    _effects["brightness"] = config.effect_brightness;
    _effects["startenabled"] = config.effect_startenabled;
    _effects["idleenabled"] = config.effect_idleenabled;
    _effects["idletimeout"] = config.effect_idletimeout;


    // MQTT
    JsonObject &_mqtt = json.createNestedObject("mqtt");
    _mqtt["enabled"] = config.mqtt;
    _mqtt["ip"] = config.mqtt_ip.c_str();
    _mqtt["port"] = config.mqtt_port;
    _mqtt["user"] = config.mqtt_user.c_str();
    _mqtt["password"] = config.mqtt_password.c_str();
    _mqtt["topic"] = config.mqtt_topic.c_str();
    _mqtt["clean"] = config.mqtt_clean;

    // E131
    JsonObject &e131 = json.createNestedObject("e131");
    e131["universe"] = config.universe;
    e131["universe_limit"] = config.universe_limit;
    e131["channel_start"] = config.channel_start;
    e131["channel_count"] = config.channel_count;
    e131["multicast"] = config.multicast;

#if defined(ESPS_MODE_PIXEL)
    // Pixel
    JsonObject &pixel = json.createNestedObject("pixel");
    pixel["type"] = static_cast<uint8_t>(config.pixel_type);
    pixel["color"] = static_cast<uint8_t>(config.pixel_color);
    pixel["groupSize"] = config.groupSize;
    pixel["zigSize"] = config.zigSize;
    pixel["gamma"] = config.gamma;
    pixel["gammaVal"] = config.gammaVal;
    pixel["briteVal"] = config.briteVal;

#elif defined(ESPS_MODE_SERIAL)
    // Serial
    JsonObject &serial = json.createNestedObject("serial");
    serial["type"] = static_cast<uint8_t>(config.serial_type);
    serial["baudrate"] = static_cast<uint32_t>(config.baudrate);
#endif

    if (pretty)
        json.prettyPrintTo(jsonString);
    else
        json.printTo(jsonString);
}

// Save configuration JSON file
void saveConfig() {
    // Update Config
    updateConfig();

    // Serialize Config
    String jsonString;
    serializeConfig(jsonString, true, true);

    // Save Config
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

void idleTimeout() {
   idleTicker.attach(config.effect_idletimeout, idleTimeout);
    if ( (config.effect_idleenabled) && (config.ds == DataSource::E131) ) {
        config.ds = DataSource::IDLEWEB;
        effects.setFromConfig();
    }
}


/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
void loop() {
    e131_packet_t packet;

    // Reboot handler
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    // Render output for current data source
    if ( (config.ds == DataSource::E131) || (config.ds == DataSource::IDLEWEB) ) {
            // Parse a packet and update pixels
            while (!e131.isEmpty()) {
                idleTicker.attach(config.effect_idletimeout, idleTimeout);
                if (config.ds == DataSource::IDLEWEB) {
                    config.ds = DataSource::E131;
                }

                e131.pull(&packet);
                uint16_t universe = htons(packet.universe);
                uint8_t *data = packet.property_values + 1;
                //LOG_PORT.print(universe);
                //LOG_PORT.println(packet.sequence_number);
                if ((universe >= config.universe) && (universe <= uniLast)) {
                    // Universe offset and sequence tracking
                    uint8_t uniOffset = (universe - config.universe);
                    if (packet.sequence_number != seqTracker[uniOffset]++) {
                        LOG_PORT.print(F("Sequence Error - expected: "));
                        LOG_PORT.print(seqTracker[uniOffset] - 1);
                        LOG_PORT.print(F(" actual: "));
                        LOG_PORT.print(packet.sequence_number);
                        LOG_PORT.print(F(" universe: "));
                        LOG_PORT.println(universe);
                        seqError[uniOffset]++;
                        seqTracker[uniOffset] = packet.sequence_number + 1;
                    }

                    // Offset the channels if required
                    uint16_t offset = 0;
                    offset = config.channel_start - 1;

                    // Find start of data based off the Universe
                    int16_t dataStart = uniOffset * config.universe_limit - offset;

                    // Calculate how much data we need for this buffer
                    uint16_t dataStop = config.channel_count;
                    uint16_t channels = htons(packet.property_value_count) - 1;
                    if (config.universe_limit < channels)
                        channels = config.universe_limit;
                    if ((dataStart + channels) < dataStop)
                        dataStop = dataStart + channels;

                    // Set the data
                    uint16_t buffloc = 0;

                    // ignore data from start of first Universe before channel_start
                    if (dataStart < 0) {
                        dataStart = 0;
                        buffloc = config.channel_start - 1;
                    }

                    for (int i = dataStart; i < dataStop; i++) {
    #if defined(ESPS_MODE_PIXEL)
                        pixels.setValue(i, data[buffloc]);
    #elif defined(ESPS_MODE_SERIAL)
                        serial.setValue(i, data[buffloc]);
    #endif
                        buffloc++;
                    }
                }
            }
    }

    if ( (config.ds == DataSource::WEB)
      || (config.ds == DataSource::IDLEWEB)
      || (config.ds == DataSource::MQTT) ) {
            effects.run();
    }

/* Streaming refresh */
#if defined(ESPS_MODE_PIXEL)
    if (pixels.canRefresh())
        pixels.show();
#elif defined(ESPS_MODE_SERIAL)
    if (serial.canRefresh())
        serial.show();
#endif

// workaround crash - consume incoming bytes on serial port
    if (LOG_PORT.available()) {
        while (LOG_PORT.read() >= 0);
    }
}

