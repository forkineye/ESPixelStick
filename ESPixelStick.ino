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
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include "ESPixelDriver.h"
#include "ESPixelStick.h"
#include "_E131.h"
#include "helpers.h"

/* Web pages and handlers */
#include "page_microajax.js.h"
#include "page_style.css.h"
#include "page_root.h"
#include "page_config_net.h"
#include "page_config_pixel.h"
#include "page_status_net.h"
#include "page_status_e131.h"


/*************************************************/
/*      BEGIN - User Configuration Defaults      */
/*************************************************/

#define NUM_PIXELS      170     /* Number of pixels */
#define UNIVERSE        1       /* First Universe to listen for */
#define CHANNEL_START   1       /* Channel to start listening at */

const char ssid[] = "SSID_NOT_SET";             /* Replace with your SSID */
const char passphrase[] = "PASSWORD_NOT_SET";   /* Replace with your WPA2 passphrase */

/*************************************************/
/*       END - User Configuration Defaults       */
/*************************************************/

ESPixelDriver	pixels;         /* Pixel object */
//TODO: Dynamically allocate seqTracker to support more than 4 universes w/ PIXELS_MAX change
uint8_t         *seqTracker;    /* Current sequence numbers for each Universe */
uint32_t        lastPacket;     /* Packet timeout tracker */

void setup() {
    /* Initial pin states */
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);
    
    Serial.begin(115200);
    delay(10);

    Serial.println("");
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        Serial.print((char)(pgm_read_byte(VERSION + i)));
    Serial.println("");
    
    /* Load configuration from EEPROM */
    EEPROM.begin(sizeof(config));
    loadConfig();

    /* Fallback to default SSID and passphrase if we fail to connect */
    int status = initWifi();
    if (status != WL_CONNECTED) {
        Serial.println(F("*** Timeout - Reverting to default SSID ***"));
        strncpy(config.ssid, ssid, sizeof(config.ssid));
        strncpy(config.passphrase, passphrase, sizeof(config.passphrase));
        status = initWifi();
    }

    //TODO: Change this to switch to softAP mode 
    /* If we fail again, reboot */
    if (status != WL_CONNECTED) {
        Serial.println(F("**** FAILED TO ASSOCIATE WITH AP ****"));
        ESP.restart();
    }

    /* Configure and start the web server */
    initWeb();
    /* Setup DNS-SD */
/* -- not working
    if (MDNS.begin("esp8266")) {
        MDNS.addService("e131", "udp", E131_DEF_PORT);
        MDNS.addService("http", "tcp", HTTP_PORT);
    } else {
        Serial.println(F("** Error setting up MDNS responder **"));
    }
*/    

    /* Initialize globals */
    //memset(seqTracker, 0x00, sizeof(seqTracker));
    //memset(seqError, 0x00, sizeof(seqTracker));

    /* Configure UART1 for WS2811 output */
    pixels.setPin(DATA_PIN);    /* For protocols that require bit-banging */
    updatePixelConfig();
    pixels.show();
}

int initWifi() {
    /* Switch to station mode and disconnect just in case */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("");
    Serial.print(F("Connecting to "));
    Serial.print(config.ssid);
    
    WiFi.begin(config.ssid, config.passphrase);

    uint32_t timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (Serial)
            Serial.print(".");
        if (millis() - timeout > CONNECT_TIMEOUT) {
            if (Serial) {
                Serial.println("");
                Serial.println(F("*** Failed to connect ***"));
            }
            break;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        if (config.dhcp) {
            Serial.print(F("Connected DHCP with IP: "));
        }  else {
            /* We don't use DNS, so just set it to our gateway */
            WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3])
            );
            Serial.print(F("Connected with Static IP: "));

        }
        Serial.println(WiFi.localIP());

        if (config.multicast)
            e131.begin(E131_MULTICAST, config.universe);
        else
            e131.begin(E131_UNICAST);
    }

    return WiFi.status();
}

/* Configure and start the web server */
void initWeb() {
    /* JavaScript and Stylesheets */
    web.on ("/style.css", []() { web.send(200, "text/plain", PAGE_STYLE_CSS); });
    web.on ("/microajax.js", []() { web.send(200, "text/plain", PAGE_MICROAJAX_JS); });

    /* HTML Pages */
    web.on("/", []() { web.send(200, "text/html", PAGE_ROOT); });
    web.on("/config/net.html", send_config_net_html);
    web.on("/config/pixel.html", send_config_pixel_html);
    web.on("/status/net.html", []() { web.send(200, "text/html", PAGE_STATUS_NET); });
    web.on("/status/e131.html", []() { web.send(200, "text/html", PAGE_STATUS_E131); });

    /* AJAX Handlers */
    web.on("/rootvals", send_root_vals_html);
    web.on("/config/netvals", send_config_net_vals_html);
    web.on("/config/pixelvals", send_config_pixel_vals_html);
    web.on("/config/connectionstate", send_connection_state_vals_html);
    web.on("/status/netvals", send_status_net_vals_html);
    web.on("/status/e131vals", send_status_e131_vals_html);

    web.onNotFound([]() { web.send(404, "text/html", "Page not Found"); });
    web.begin();

    Serial.print(F("- Web Server started on port "));
    Serial.println(HTTP_PORT);
}

/* Configuration Validations */
void validateConfig() {
    /* Generic count limit */
    if (config.pixel_count > 680)
        config.pixel_count = 680;
    else if (config.pixel_count < 1)
        config.pixel_count = 1;

    /* Generic PPU Limit */
    if (config.ppu > PIXELS_MAX)
        config.ppu = PIXELS_MAX;
    else if (config.ppu < 1)
        config.ppu = 1;

    /* GECE Limits */
    if (config.pixel_type == PIXEL_GECE) {
        uniLast = config.universe;
        config.pixel_color = COLOR_RGB;
        if (config.pixel_count > 63)
            config.pixel_count = 63;
    } else {
        uint16_t count = config.pixel_count * 3;
        uint16_t bounds = config.ppu * 3;
        if (count % bounds)
            uniLast = config.universe + count / bounds;
        else 
            uniLast = config.universe + count / bounds - 1;
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
    //pixels.updateType(config.pixel_type, config.pixel_color);
    pixels.updateLength(config.pixel_count);
    Serial.print(F("- Listening for "));
    Serial.print(config.pixel_count * 3);
    Serial.print(F(" channels, from Universe "));
    Serial.print(config.universe);
    Serial.print(F(" to "));
    Serial.println(uniLast);
}

/* Attempt to load configuration from EEPROM.  Initialize or upgrade as required */
void loadConfig() {
    EEPROM.get(EEPROM_BASE, config);
    if (memcmp_P(config.id, CONFIG_ID, sizeof(config.id))) {
        Serial.println(F("- No configuration found."));

        /* Initialize configuration structure */
        memset(&config, 0, sizeof(config));
        memcpy_P(config.id, CONFIG_ID, sizeof(config.id));
        config.version = CONFIG_VERSION;
        strncpy(config.name, "ESPixelStick", sizeof(config.name));
        strncpy(config.ssid, ssid, sizeof(config.ssid));
        strncpy(config.passphrase, passphrase, sizeof(config.passphrase));
        config.ip[0] = 0; config.ip[1] = 0; config.ip[2] = 0; config.ip[3] = 0;
        config.netmask[0] = 0; config.netmask[1] = 0; config.netmask[2] = 0; config.netmask[3] = 0;
        config.gateway[0] = 0; config.gateway[1] = 0; config.gateway[2] = 0; config.gateway[3] = 0;
        config.dhcp = 1;
        config.multicast = 0;
        config.universe = UNIVERSE;
        config.channel_start = CHANNEL_START;
        config.pixel_count = NUM_PIXELS;
        config.pixel_type = PIXEL_WS2811;
        config.pixel_color = COLOR_RGB;
        config.ppu = PIXELS_MAX;
        config.gamma = 1.0;

        /* Write the configuration structre */
        EEPROM.put(EEPROM_BASE, config);
        EEPROM.commit();
        Serial.println(F("* Default configuration saved."));
    } else {
        if (config.version < CONFIG_VERSION) {
            /* Config updates and resets for V2 */
            config.version = CONFIG_VERSION;
            config.ppu = PIXELS_MAX;
            config.gamma = 1.0;
            Serial.println(F("- Configuration upgraded."));
        } else {
            Serial.println(F("- Configuration loaded."));
        }
    }

    /* Validate it */
    validateConfig();
}

void saveConfig() {  
    /* Write the configuration structre */
    EEPROM.put(EEPROM_BASE, config);
    EEPROM.commit();
    Serial.println(F("* New configuration saved."));
}

/* Main Loop */
void loop() {
    /* Handle incoming web requests if needed */
    web.handleClient();

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
            uint16_t pixelStop = config.pixel_count;
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
}
