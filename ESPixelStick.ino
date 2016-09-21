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

/* Output Mode - There can be only one! (-Conor MacLeod) */
#define ESPS_MODE_PIXEL
//#define ESPS_MODE_SERIAL

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = "ENTER_SSID_HERE";
const char passphrase[] = "ENTER_PASSPHRASE_HERE";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Hash.h>
#include "ESPixelStick.h"
#include "_E131.h"
#include "helpers.h"

/* Output Drivers */
#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
#include "page_config_pixel.h"
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#include "page_config_serial.h"
#else
#error "No valid output mode defined."
#endif

/* Common Web pages and handlers */
#include "page_root.h"
#include "page_admin.h"
#include "page_config_net.h"
#include "page_status_net.h"
#include "page_status_e131.h"

#if defined(ESPS_MODE_PIXEL)
PixelDriver     pixels;         /* Pixel object */
#elif defined(ESPS_MODE_SERIAL)
SerialDriver    serial;         /* Serial object */
#endif

uint8_t             *seqTracker;        /* Current sequence numbers for each Universe */
uint32_t            lastUpdate;         /* Update timeout tracker */
AsyncWebServer      web(HTTP_PORT);     /* Web Server */

/* Forward Declarations */
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void loadConfig();
int initWifi();
void initWeb();
void updateConfig();

void setup() {
    /* Generate and set hostname */
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    String hostname = "esps_" + String(chipId);
    WiFi.hostname(hostname);

    /* Initial pin states */
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    /* Setup serial log port */
    LOG_PORT.begin(115200);
    delay(10);

    /* Enable SPIFFS */
    SPIFFS.begin();

    LOG_PORT.println("");
    LOG_PORT.print(F("ESPixelStick v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.println("");

    /* Load configuration from SPIFFS */
    loadConfig();

    /* Fallback to default SSID and passphrase if we fail to connect */
    int status = initWifi();
    if (status != WL_CONNECTED) {
        LOG_PORT.println(F("*** Timeout - Reverting to default SSID ***"));
        config.ssid = ssid;
        config.passphrase = passphrase;
        status = initWifi();
    }

    /* If we fail again, go SoftAP or reboot */
    if (status != WL_CONNECTED) {
        if (config.ap_fallback) {
            LOG_PORT.println(F("**** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ****"));
            WiFi.mode(WIFI_AP);
            String ssid = "ESPixel " + String(chipId);
            WiFi.softAP(ssid.c_str());
        } else {
            LOG_PORT.println(F("**** FAILED TO ASSOCIATE WITH AP, REBOOTING ****"));
            ESP.restart();
        }
    }

    /* Configure and start the web server */
    initWeb();

    /* Setup mDNS / DNS-SD */
    //TODO: Reboot or restart mdns when config.id is changed?
    MDNS.setInstanceName(config.id + " (" + String(chipId) + ")");
    if (MDNS.begin(hostname.c_str())) {
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

    /* Configure the outputs */
#if defined (ESPS_MODE_PIXEL)
    pixels.setPin(DATA_PIN);
    updateConfig();
    pixels.show();
#else
    updateConfig();
#endif
}

int initWifi() {
    /* Switch to station mode and disconnect just in case */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.println(config.ssid);

    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
    if (config.dhcp) {
        LOG_PORT.print(F("Connecting with DHCP"));
    } else {
        /* We don't use DNS, so just set it to our gateway */
        WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
        );
        LOG_PORT.print(F("Connecting with Static IP"));
    }

    uint32_t timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        LOG_PORT.print(".");
        if (millis() - timeout > CONNECT_TIMEOUT) {
            LOG_PORT.println("");
            LOG_PORT.println(F("*** Failed to connect ***"));
            break;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_PORT.println("");
        LOG_PORT.print(F("Connected with IP: "));
        LOG_PORT.println(WiFi.localIP());

        if (config.multicast)
            e131.begin(E131_MULTICAST, config.universe);
        else
            e131.begin(E131_UNICAST);
    }

    return WiFi.status();
}

/* Configure and start the web server */
void initWeb() {
    /* Handle OTA update from asynchronous callbacks */
    Update.runAsync(true);

    /* Heap status handler */
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    /* Config file handler for testing */
    //web.serveStatic("/configfile", SPIFFS, "/config.json");

    /* JSON Config Handler */
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) {
        String jsonString;
        serializeConfig(jsonString);
        request->send(200, "text/json", jsonString);
    });

    /* AJAX Handlers */
    web.on("/rootvals", HTTP_GET, send_root_vals);
    web.on("/adminvals", HTTP_GET, send_admin_vals);
    web.on("/config/netvals", HTTP_GET, send_config_net_vals);
    web.on("/config/survey", HTTP_GET, send_survey_vals);
    web.on("/status/netvals", HTTP_GET, send_status_net_vals);
    web.on("/status/e131vals", HTTP_GET, send_status_e131_vals);

    /* POST Handlers */
    web.on("/admin.html", HTTP_POST, send_admin_html, handle_fw_upload);
    web.on("/config_net.html", HTTP_POST, send_config_net_html);

    /* Static handler */
#if defined(ESPS_MODE_PIXEL)
    web.on("/config/pixelvals", HTTP_GET, send_config_pixel_vals);
    web.on("/config_pixel.html", HTTP_POST, send_config_pixel_html);
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("pixel.html");
#elif defined(ESPS_MODE_SERIAL)
    web.on("/config/serialvals", HTTP_GET, send_config_serial_vals);
    web.on("/config_serial.html", HTTP_POST, send_config_serial_html);
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("serial.html");
#endif

    web.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Page not found");
    });

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

/* Configuration Validations */
void validateConfig() {
    /* E1.31 Limits */
    if (config.universe < 1)
        config.universe = 1;

    if (config.channel_start < 1)
        config.channel_start = 1;
    else if (config.channel_start > UNIVERSE_LIMIT)
        config.channel_start = UNIVERSE_LIMIT;

#if defined(ESPS_MODE_PIXEL)
    /* Generic channel limits for pixels */
    if (config.channel_count % 3)
        config.channel_count = (config.channel_count / 3) * 3;

    if (config.channel_count > PIXEL_LIMIT * 3)
        config.channel_count = PIXEL_LIMIT * 3;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    /* GECE Limits */
    if (config.pixel_type == PixelType::GECE) {
        config.pixel_color = PixelColor::RGB;
        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
    }

#elif defined(ESPS_MODE_SERIAL)
    /* Generic serial channel limits */
    if (config.channel_count > RENARD_LIMIT)
        config.channel_count = RENARD_LIMIT;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    if (config.serial_type == SerialType::DMX512 && config.channel_count > UNIVERSE_LIMIT)
        config.channel_count = UNIVERSE_LIMIT;

    /* Baud rate check */
    if (config.baudrate > BaudRate::BR_460800)
        config.baudrate = BaudRate::BR_460800;
    else if (config.baudrate < BaudRate::BR_38400)
        config.baudrate = BaudRate::BR_57600;
#endif
}

void updateConfig() {
    /* Validate first */
    validateConfig();

    /* Find the last universe we should listen for */
    uint16_t span = config.channel_start + config.channel_count - 1;
    if (span % UNIVERSE_LIMIT)
        uniLast = config.universe + span / UNIVERSE_LIMIT;
    else
        uniLast = config.universe + span / UNIVERSE_LIMIT - 1;

    /* Setup the sequence error tracker */
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if (seqTracker = static_cast<uint8_t *>(malloc(uniTotal)))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if (seqError = static_cast<uint32_t *>(malloc(uniTotal * 4)))
        memset(seqError, 0x00, uniTotal * 4);

    /* Zero out packet stats */
    e131.stats.num_packets = 0;

    /* Initialize for our pixel type */
#if defined(ESPS_MODE_PIXEL)
    pixels.begin(config.pixel_type, config.pixel_color, config.channel_count / 3);
    pixels.setGamma(config.gamma);
#elif defined(ESPS_MODE_SERIAL)
    serial.begin(&SEROUT_PORT, config.serial_type, config.channel_count, config.baudrate);
#endif
    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.println(uniLast);
}

/* Load configugration JSON file */
void loadConfig() {
    /* Zeroize Config struct */
    memset(&config, 0, sizeof(config));

    /* Load CONFIG_FILE json. Create and init with defaults if not found */
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        config.ssid = ssid;
        config.passphrase = passphrase;
        saveConfig();
    } else {
        /* Parse CONFIG_FILE json */
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

        /* Device */
        config.id = json["device"]["id"].as<String>();

        /* Fallback to embedded ssid and passphrase if null in config */
        if (strlen(json["network"]["ssid"]))
            config.ssid = json["network"]["ssid"].as<String>();
        else
            config.ssid = ssid;

        if (strlen(json["network"]["passphrase"]))
            config.passphrase = json["network"]["passphrase"].as<String>();
        else
            config.passphrase = passphrase;

        /* Network */
        for (int i = 0; i < 4; i++) {
            config.ip[i] = json["network"]["ip"][i];
            config.netmask[i] = json["network"]["netmask"][i];
            config.gateway[i] = json["network"]["gateway"][i];
        }
        config.dhcp = json["network"]["dhcp"];
        config.ap_fallback = json["network"]["ap_fallback"];

        /* E131 */
        config.universe = json["e131"]["universe"];
        config.channel_start = json["e131"]["channel_start"];
        config.channel_count = json["e131"]["channel_count"];
        config.multicast = json["e131"]["multicast"];

#if defined(ESPS_MODE_PIXEL)
        /* Pixel */
        config.pixel_type = PixelType(static_cast<uint8_t>(json["pixel"]["type"]));
        config.pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
        config.gamma = json["pixel"]["gamma"];

#elif defined(ESPS_MODE_SERIAL)
        /* Serial */
        config.serial_type = SerialType(static_cast<uint8_t>(json["serial"]["type"]));
        config.baudrate = BaudRate(static_cast<uint32_t>(json["serial"]["baudrate"]));
#endif

        LOG_PORT.println(F("- Configuration loaded."));
    }

    /* Validate it */
    validateConfig();
}

/* Serialize the current config into a JSON string */
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    /* Create buffer and root object */
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    /* Device */
    JsonObject &device = json.createNestedObject("device");
    device["id"] = config.id.c_str();

    /* Network */
    JsonObject &network = json.createNestedObject("network");
    network["ssid"] = config.ssid.c_str();
    if (creds)
        network["passphrase"] = config.passphrase.c_str();
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

    /* E131 */
    JsonObject &e131 = json.createNestedObject("e131");
    e131["universe"] = config.universe;
    e131["channel_start"] = config.channel_start;
    e131["channel_count"] = config.channel_count;
    e131["multicast"] = config.multicast;

#if defined(ESPS_MODE_PIXEL)
    /* Pixel */
    JsonObject &pixel = json.createNestedObject("pixel");
    pixel["type"] = static_cast<uint8_t>(config.pixel_type);
    pixel["color"] = static_cast<uint8_t>(config.pixel_color);
    pixel["gamma"] = config.gamma;

#elif defined(ESPS_MODE_SERIAL)
    /* Serial */
    JsonObject &serial = json.createNestedObject("serial");
    serial["type"] = static_cast<uint8_t>(config.serial_type);
    serial["baudrate"] = static_cast<uint32_t>(config.baudrate);
#endif

    if (pretty)
        json.prettyPrintTo(jsonString);
    else
        json.printTo(jsonString);
}

/* Save configuration JSON file */
void saveConfig() {
    /* Update Config */
    updateConfig();

    /* Serialize Config */
    String jsonString;
    serializeConfig(jsonString, true, true);

    /* Save Config */
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

/* Main Loop */
void loop() {
    /* Reboot handler */
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    /* Parse a packet and update pixels */
    if (e131.parsePacket()) {
        if ((e131.universe >= config.universe) && (e131.universe <= uniLast)) {
            /* Universe offset and sequence tracking */
            uint8_t uniOffset = (e131.universe - config.universe);
            if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                seqError[uniOffset]++;
                seqTracker[uniOffset] = e131.packet->sequence_number + 1;
            }

            /* Offset the channel if required for the first universe */
            uint16_t offset = 0;
            if (e131.universe == config.universe)
                offset = config.channel_start - 1;

            /* Find start of data based off the Universe */
            uint16_t dataStart = uniOffset * UNIVERSE_LIMIT;

            /* Caculate how much data we need for this buffer */
            uint16_t dataStop = config.channel_count;
            if ((dataStart + UNIVERSE_LIMIT) < dataStop)
                dataStop = dataStart + UNIVERSE_LIMIT;

            /* Set the data */
            uint16_t buffloc = 0;
            for (int i = dataStart; i < dataStop; i++) {
#if defined(ESPS_MODE_PIXEL)
                pixels.setValue(i, e131.data[buffloc + offset]);
#elif defined(ESPS_MODE_SERIAL)
                serial.setValue(i, e131.data[buffloc + offset]);
#endif
                buffloc++;
            }
        }
    }

/* Streaming refresh */
#if defined(ESPS_MODE_PIXEL)
    if (pixels.canRefresh())
        pixels.show();
#elif defined(ESPS_MODE_SERIAL)
    if (serial.canRefresh())
        serial.show();
#endif
}
