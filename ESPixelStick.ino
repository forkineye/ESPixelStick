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
#include "page_header.h"
#include "page_root.h"
#include "page_admin.h"
#include "page_config_net.h"
#include "page_config_pixel.h"
#include "page_status_net.h"
#include "page_status_e131.h"


/*************************************************/
/*      BEGIN - User Configuration Defaults      */
/*************************************************/

/* REQUIRED */
const char ssid[] = "SSID_NOT_SET";             /* Replace with your SSID */
const char passphrase[] = "PASSWORD_NOT_SET";   /* Replace with your WPA2 passphrase */

/* OPTIONAL */
#define UNIVERSE        1               /* Universe to listen for */
#define CHANNEL_START   1               /* Channel to start listening at */
#define NUM_PIXELS      170             /* Number of pixels */
#define PIXEL_TYPE      PIXEL_WS2811    /* Pixel type - See ESPixelDriver.h for full list */
#define COLOR_ORDER     COLOR_RGB       /* Color Order - See ESPixelDriver.h for full list */
#define PPU             170             /* Pixels per Universe */

/*  Use only 1.0 or 0.0 to enable / disable the internal gamma map for now.  In the future
 *  this will be the gamma value used for dynamic gamma table creation, but the current
 *  stable release of the ESP SDK has pow() broken.
 */
#define GAMMA           1.0             /* Gamma */

/*************************************************/
/*       END - User Configuration Defaults       */
/*************************************************/

ESPixelDriver	pixels;         /* Pixel object */
uint8_t         *seqTracker;    /* Current sequence numbers for each Universe */
uint32_t        lastPacket;     /* Packet timeout tracker */

void setup() {
    /* Initial pin states */
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);
    
    Serial.begin(115200);
    delay(10);

    Serial.println("");
    Serial.print(F("ESPixelStick v"));
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

    
    /* If we fail again, go SoftAP */
    if (status != WL_CONNECTED) {
        Serial.println(F("**** FAILED TO ASSOCIATE WITH AP ****"));
	WiFi.mode(WIFI_AP);
        String ssid = "ESPixel " + (String)ESP.getChipId();
        WiFi.softAP(ssid.c_str());
        //ESP.restart();
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

    /* Configure our outputs and pixels */
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
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
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

/* Read a page from PROGMEM and send it */
void sendPage(const char *data, int count, const char *type) {
    int szHeader = sizeof(PAGE_HEADER);
    char *buffer = (char*)malloc(count + szHeader);
    if (buffer) {
        memcpy_P(buffer, PAGE_HEADER, szHeader);
        memcpy_P(buffer + szHeader - 1, data, count);   /* back up over the null byte from the header string */
        web.send(200, type, buffer);
        free(buffer);
    } else {
        Serial.print(F("*** Malloc failed for "));
        Serial.print(count);
        Serial.println(F(" bytes in sendPage() ***"));
    }
}

/* Configure and start the web server */
void initWeb() {
    /* HTML Pages */
    web.on("/", []() { sendPage(PAGE_ROOT, sizeof(PAGE_ROOT), PTYPE_HTML); });
    web.on("/admin.html", send_admin_html);
    web.on("/config/net.html", send_config_net_html);
    web.on("/config/pixel.html", send_config_pixel_html);
    web.on("/status/net.html", []() { sendPage(PAGE_STATUS_NET, sizeof(PAGE_STATUS_NET), PTYPE_HTML); });
    web.on("/status/e131.html", []() { sendPage(PAGE_STATUS_E131, sizeof(PAGE_STATUS_E131), PTYPE_HTML); });

    /* AJAX Handlers */
    web.on("/rootvals", send_root_vals);
    web.on("/adminvals", send_admin_vals);
    web.on("/config/netvals", send_config_net_vals);
    web.on("/config/pixelvals", send_config_pixel_vals);
    web.on("/config/connectionstate", send_connection_state_vals);
    web.on("/status/netvals", send_status_net_vals);
    web.on("/status/e131vals", send_status_e131_vals);

    /* Admin Handlers */
    web.on("/reboot", []() { sendPage(PAGE_ADMIN_REBOOT, sizeof(PAGE_ADMIN_REBOOT), PTYPE_HTML); ESP.restart(); });

    web.onNotFound([]() { web.send(404, PTYPE_HTML, "Page not Found"); });
    web.begin();

    Serial.print(F("- Web Server started on port "));
    Serial.println(HTTP_PORT);
}

/* Configuration Validations */
void validateConfig() {
    /* Generic count limit */
    if (config.pixel_count > PIXEL_LIMIT)
        config.pixel_count = PIXEL_LIMIT;
    else if (config.pixel_count < 1)
        config.pixel_count = 1;

    /* Generic PPU Limit */
    if (config.ppu > PPU_MAX)
        config.ppu = PPU_MAX;
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
    pixels.updateLength(config.pixel_count);
    pixels.setGamma(config.gamma);
    Serial.print(F("- Listening for "));
    Serial.print(config.pixel_count * 3);
    Serial.print(F(" channels, from Universe "));
    Serial.print(config.universe);
    Serial.print(F(" to "));
    Serial.println(uniLast);
}

/* Initialize configuration structure */
void initConfig() {
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
    config.pixel_type = PIXEL_TYPE;
    config.pixel_color = COLOR_ORDER;
    config.ppu = PPU;
    config.gamma = GAMMA;    
}

/* Attempt to load configuration from EEPROM.  Initialize or upgrade as required */
void loadConfig() {
    EEPROM.get(EEPROM_BASE, config);
    if (memcmp_P(config.id, CONFIG_ID, sizeof(config.id))) {
        Serial.println(F("- No configuration found."));

        /* Initialize config structure */
        initConfig();

        /* Write the configuration structure */
        EEPROM.put(EEPROM_BASE, config);
        EEPROM.commit();
        Serial.println(F("* Default configuration saved."));
    } else {
        if (config.version < CONFIG_VERSION) {
            /* Major struct changes in V3 for alignment require re-initialization */
            if (config.version < 3) {
                char ssid[32];
                char pass[64];
                strncpy(ssid, config.ssid - 1, sizeof(ssid));
                strncpy(pass, config.passphrase - 1, sizeof(pass));
                initConfig();
                strncpy(config.ssid, ssid, sizeof(config.ssid));
                strncpy(config.passphrase, pass, sizeof(config.passphrase));
            }
            
            EEPROM.put(EEPROM_BASE, config);
            EEPROM.commit();
            Serial.println(F("* Configuration upgraded."));
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
