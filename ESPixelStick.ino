/*
* ESPixelStick.ino
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
* http://www.forkineye.com
*
* Notes:
* - For best performance, set to 160MHz (Tools->CPU Frequency).
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

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "_E131.h"
#include "helpers.h"

/* Output Drivers */
#include "PixelDriver.h"
#include "RenardDriver.h"
#include "DMX512Driver.h"

/* Web pages and handlers */
#include "page_root.h"
#include "page_admin.h"
#include "page_config_net.h"
#include "page_config_pixel.h"
#include "page_config_serial.h"
#include "page_status_net.h"
#include "page_status_e131.h"

/*****************************************/
/*    BEGIN - Fallback Configuration     */
/*****************************************/

/* 
   Default configuration values are now in data/config.json.
   If ssid or passphrase in config.json is null, or the AP 
   fails to associate, these values will be used as a fallback.
*/
const char ssid[] = "SSID_NOT_SET";             /* Replace with your SSID */
const char passphrase[] = "PASSPHRASE_NOT_SET"; /* Replace with your WPA2 passphrase */

/*****************************************/
/*     END - Fallback Configuration      */
/*****************************************/

PixelDriver     pixels;         /* Pixel object */
RenardDriver    renard;         /* Renard object */
DMX512Driver    dmx;            /* DMX object */
uint8_t         *seqTracker;    /* Current sequence numbers for each Universe */
uint32_t        lastPacket;     /* Packet timeout tracker */

/* Forward Declarations */
void loadConfig();
int initWifi();
void initWeb();

void setup() {
    /* Generate and set hostname */
    char hostname[16] = { 0 };
    sprintf(hostname, "ESP_%06X", ESP.getChipId());
    WiFi.hostname(hostname);

    /* Initial pin states */
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    /* Enable SPIFFS */
    SPIFFS.begin();
    
    LOG_PORT.begin(115200);
    delay(10);

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
        strncpy(config.ssid, ssid, sizeof(config.ssid));
        strncpy(config.passphrase, passphrase, sizeof(config.passphrase));
        status = initWifi();
    }

    /* If we fail again, go SoftAP */
    if (status != WL_CONNECTED) {
        LOG_PORT.println(F("**** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ****"));
        WiFi.mode(WIFI_AP);
        String ssid = "ESPixel " + (String)ESP.getChipId();
        WiFi.softAP(ssid.c_str());
        //ESP.restart();
    }

    /* Configure and start the web server */
    initWeb();
    /* Setup mDNS / DNS-SD */
    if (MDNS.begin(hostname)) {
        MDNS.addService("e131", "udp", E131_DEFAULT_PORT); 
        MDNS.addService("http", "tcp", HTTP_PORT);
    } else {
        LOG_PORT.println(F("** Error setting up mDNS responder **"));
    }

    /* Configure our outputs */
    switch (config.mode) {
        case OutputMode::PIXEL:
            pixels.setPin(DATA_PIN);    /* For protocols that require bit-banging */
            updatePixelConfig();
            pixels.show();
            break;
        case OutputMode::DMX512:
            updateDMXConfig();
            break;
        case OutputMode::RENARD:
            updateRenardConfig();
            break;
    }
}

int initWifi() {
    /* Switch to station mode and disconnect just in case */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.print(config.ssid);
    
    WiFi.begin(config.ssid, config.passphrase);

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
        if (config.dhcp) {
            LOG_PORT.print(F("Connected DHCP with IP: "));
        }  else {
            /* We don't use DNS, so just set it to our gateway */
            WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
            );
            LOG_PORT.print(F("Connected with Static IP: "));

        }
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
    /* Heap status handler */
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    /* Temp Config handler */
    web.serveStatic("/config", SPIFFS, "/config.json");

    /* AJAX Handlers */
    web.on("/rootvals", HTTP_GET, send_root_vals);
    web.on("/adminvals", HTTP_GET, send_admin_vals);
    web.on("/config/netvals", HTTP_GET, send_config_net_vals);
    web.on("/config/connectionstate", HTTP_GET, send_connection_state_vals);
    web.on("/config/pixelvals", HTTP_GET, send_config_pixel_vals);
    web.on("/config/renardvals", HTTP_GET, send_config_renard_vals);
    web.on("/config/dmxvals", HTTP_GET, send_config_dmx_vals);
    web.on("/status/netvals", HTTP_GET, send_status_net_vals);
    web.on("/status/e131vals", HTTP_GET, send_status_e131_vals);

    /* POST Handlers */
    web.on("/admin.html", HTTP_POST, send_admin_html);
    web.on("/config_net.html", HTTP_POST, send_config_net_html);
    web.on("/config_pixel.html", HTTP_POST, send_config_pixel_html);
    web.on("/config_renard.html", HTTP_POST, send_config_renard_html);
    web.on("/config_dmx.html", HTTP_POST, send_config_dmx_html);

    /* Static handler */
    if (config.mode == OutputMode::RENARD)
        web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("renard.html");
    else if (config.mode == OutputMode::DMX512)
        web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("dmx512.html");
    else
        web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");

    web.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

/* Configuration Validations */
void validateConfig() {
    /* Generic channel limit */
    if (config.channel_count > PIXEL_LIMIT * 3)
        config.channel_count = PIXEL_LIMIT * 3;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    /* Generic PPU Limit */
    if (config.ppu > PPU_MAX)
        config.ppu = PPU_MAX;
    else if (config.ppu < 1)
        config.ppu = 1;

    /* GECE Limits */
    if (config.pixel_type == PixelType::GECE) {
        uniLast = config.universe;
        config.pixel_color = PixelColor::RGB;
        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
    } else {
        uint16_t bounds = config.ppu * 3;
        if (config.channel_count % bounds)
            uniLast = config.universe + config.channel_count / bounds;
        else 
            uniLast = config.universe + config.channel_count / bounds - 1;
    }
}

void updatePixelConfig() {
    /* Validate first */
    validateConfig();

    /* Setup the sequence error tracker */
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = (uint8_t *)malloc(uniTotal)))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = (uint32_t *)malloc(uniTotal * 4)))
        memset(seqError, 0x00, uniTotal * 4);

    /* Zero out packet stats */
    e131.stats.num_packets = 0;
    
    /* Initialize for our pixel type */
    pixels.begin(config.pixel_type, config.pixel_color);
    pixels.updateLength(config.channel_count / 3);
    pixels.setGamma(config.gamma);
    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.println(uniLast);
}

void updateRenardConfig() {
    /* Validate first */
    //need serial specific validation settings
    //validateConfig();

    /* Setup the sequence error tracker */
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = (uint8_t *)malloc(uniTotal)))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = (uint32_t *)malloc(uniTotal * 4)))
        memset(seqError, 0x00, uniTotal * 4);

    /* Zero out packet stats */
    e131.stats.num_packets = 0;
    
    /* Initialize for our serial type */
    renard.begin(&SERIAL_PORT, config.channel_count, config.renard_baud);

    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.println(uniLast);
}

void updateDMXConfig() {
    /* Validate first */
    //need serial specific validation settings
    //validateConfig();

    /* Setup the sequence error tracker */
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = (uint8_t *)malloc(uniTotal)))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = (uint32_t *)malloc(uniTotal * 4)))
        memset(seqError, 0x00, uniTotal * 4);

    /* Zero out packet stats */
    e131.stats.num_packets = 0;
    
    /* Initialize for our serial type */
    dmx.begin(&SERIAL_PORT, config.channel_count);

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
        strncpy(config.ssid, ssid, sizeof(config.ssid));
        strncpy(config.passphrase, passphrase, sizeof(config.passphrase));
        sprintf(config.hostname, "ESP_%06X", ESP.getChipId());
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

        /* Fallback to embedded ssid and passphrase if null in config */
        if (strlen(json["network"]["ssid"]))
            strncpy(config.ssid, json["network"]["ssid"], sizeof(config.ssid));
        else
            strncpy(config.ssid, ssid, sizeof(config.ssid));
        
        if (strlen(json["network"]["passphrase"]))
            strncpy(config.passphrase, json["network"]["passphrase"], sizeof(config.passphrase));
        else
            strncpy(config.passphrase, passphrase, sizeof(config.passphrase));

        /* Fallback to auto-generated hostname if null in config */
        if (strlen(json["network"]["hostname"]))
            strncpy(config.hostname, json["network"]["hostname"], sizeof(config.hostname));
        else
            sprintf(config.hostname, "ESP_%06X", ESP.getChipId());

        /* Network */
        for (int i = 0; i < 4; i++) {
            config.ip[i] = json["network"]["ip"][i];
            config.netmask[i] = json["network"]["netmask"][i];
            config.gateway[i] = json["network"]["gateway"][i];
        }
        config.dhcp = json["network"]["dhcp"];
        config.ap_fallback = json["network"]["ap_fallback"];

        /* Device */
        strncpy(config.id, json["device"]["id"], sizeof(config.id));
        config.mode = OutputMode(static_cast<uint8_t>(json["device"]["mode"]));

        /* E131 */
        config.universe = json["e131"]["universe"];
        config.channel_start = json["e131"]["channel_start"];
        config.channel_count = json["e131"]["channel_count"];
        config.multicast = json["e131"]["multicast"];
        
        /* Pixel */
        config.pixel_type = PixelType(static_cast<uint8_t>(json["pixel"]["type"]));
        config.pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
        config.ppu = json["pixel"]["ppu"];
        config.gamma = json["pixel"]["gamma"];

        /* DMX512 */
        config.dmx_passthru = json["dmx"]["passthru"];

        /* Renard */
        config.renard_baud = json["renard"]["baudrate"];

        LOG_PORT.println(F("- Configuration loaded."));
    }

    /* Validate it */
    validateConfig();
}

/* Save configuration JSON file */
void saveConfig() {  
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        /* Create all required JSON objects and save the file */
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.createObject();

        /* Network */
        JsonObject &network = json.createNestedObject("network");
        network["ssid"] = config.ssid;
        network["passphrase"] = config.passphrase;
        network["hostname"] = config.hostname;
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

        /* Device */
        JsonObject &device = json.createNestedObject("device");
        device["id"] = config.id;
        device["mode"] = static_cast<uint8_t>(config.mode);

        /* E131 */
        JsonObject &e131 = json.createNestedObject("e131");
        e131["universe"] = config.universe;
        e131["channel_start"] = config.channel_start;
        e131["channel_count"] = config.channel_count;
        e131["multicast"] = config.multicast;

        /* Pixel */
        JsonObject &pixel = json.createNestedObject("pixel");
        pixel["type"] = static_cast<uint8_t>(config.pixel_type);
        pixel["color"] = static_cast<uint8_t>(config.pixel_color);
        pixel["ppu"] = config.ppu;
        pixel["gamma"] = config.gamma;

        /* DMX512 */
        JsonObject &dmx512 = json.createNestedObject("dmx512");
        dmx512["passthru"] = config.dmx_passthru;

        /* Renard */
        JsonObject &renard = json.createNestedObject("renard");
        renard["baudrate"] = config.renard_baud;

        json.prettyPrintTo(file);
        LOG_PORT.println(F("* New configuration saved."));
    }
}

/* Main Loop */
void loop() {
    /* Reboot handler */
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

    /* Configure our outputs and pixels */
    switch (config.mode) {
        case OutputMode::PIXEL:
            /* Parse a packet and update pixels */
            if(e131.parsePacket()) {
                if ((e131.universe >= config.universe) && (e131.universe <= uniLast)) {
                    /* Universe offset and sequence tracking */
                    uint8_t uniOffset = (e131.universe - config.universe);
                    if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                        seqError[uniOffset]++;
                        seqTracker[uniOffset] = e131.packet->sequence_number + 1;
                    }

                    /* Find out starting pixel based off the Universe */
                    uint16_t pixelStart = uniOffset * config.ppu;

                    /* Calculate how many pixels we need from this buffer */
                    uint16_t pixelStop = config.channel_count / 3;
                    if ((pixelStart + config.ppu) < pixelStop)
                        pixelStop = pixelStart + config.ppu;

                    /* Offset the channel if required for the first universe */
                    uint16_t offset = 0;
                    if (e131.universe == config.universe)
                        offset = config.channel_start - 1;

                    /* Set the pixel data */
                    uint16_t buffloc = 0;
                    for (int i = pixelStart; i < pixelStop; i++) {
                        int j = buffloc++ * 3 + offset;
                        pixels.setPixelColor(i, e131.data[j], e131.data[j+1], e131.data[j+2]);
                    }

                    /* Refresh when last universe shows up */
                    if (e131.universe == uniLast) {
                        lastPacket = millis();
                        pixels.show();
                    }
                }
            }

            //TODO: Use this for setting defaults states at a later date
            /* Force refresh every second if there is no data received */
            if ((millis() - lastPacket) > E131_TIMEOUT) {
                lastPacket = millis();
                pixels.show();
            }
            break;
            
        /* Parse a packet and update Renard */
        case OutputMode::RENARD:
            if (e131.parsePacket()) {
                if (e131.universe == config.universe) {
                    // Universe offset and sequence tracking 
                    /* uint8_t uniOffset = (e131.universe - config.universe);
                    if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                        seqError[uniOffset]++;
                        seqTracker[uniOffset] = e131.packet->sequence_number + 1;
                    }
                    */
                    uint16_t offset = config.channel_start - 1;

                    // Set the serial data 
                    renard.startPacket();
                    for(int i = 0; i<config.channel_count; i++) {
                        renard.setValue(i, e131.data[i + offset]);    
                    }

                    // Refresh  
                    renard.show();
                }
            }
            break;

        /* Parse a packet and update DMX */
        case OutputMode::DMX512:
            if (e131.parsePacket()) {
                if (e131.universe == config.universe) {
                    // Universe offset and sequence tracking 
                    /* uint8_t uniOffset = (e131.universe - config.universe);
                    if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                        seqError[uniOffset]++;
                        seqTracker[uniOffset] = e131.packet->sequence_number + 1;
                    }
                    */
                    uint16_t offset = config.channel_start - 1;

                    // Set the serial data 
                    dmx.startPacket();
                    for(int i = 0; i<config.channel_count; i++) {
                        dmx.setValue(i, e131.data[i + offset]);    
                    }

                    // Refresh  
                    dmx.show();
                }
            }
            break;
    }
}
