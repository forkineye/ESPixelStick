/*
* ESPixelStick.h
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

#ifndef ESPIXELSTICK_H_
#define ESPIXELSTICK_H_

#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#endif

/* Name and version */
const char VERSION[] = "2.1-dev (20161214)";

#define HTTP_PORT       80      /* Default web server port */
#define DATA_PIN        2       /* Pixel output - GPIO2 */
#define EEPROM_BASE     0       /* EEPROM configuration base address */
#define UNIVERSE_LIMIT  512     /* Universe boundary - 512 Channels */
#define PIXEL_LIMIT     1360    /* Total pixel limit - 40.85ms for 8 universes */
#define RENARD_LIMIT    2048    /* Channel limit for serial outputs */
#define E131_TIMEOUT    1000    /* Force refresh every second an E1.31 packet is not seen */
#define CONNECT_TIMEOUT 15000   /* 15 seconds */
#define REBOOT_DELAY    100     /* Delay for rebooting once reboot flag is set */
#define LOG_PORT        Serial  /* Serial port for console logging */

/* E1.33 / RDMnet stuff - to be moved to library */
#define RDMNET_DNSSD_SRV_TYPE   "draft-e133.tcp"
#define RDMNET_DEFAULT_SCOPE    "default"
#define RDMNET_DEFAULT_DOMAIN   "local"
#define RDMNET_DNSSD_TXTVERS    1
#define RDMNET_DNSSD_E133VERS   1

/* Configuration file params */
const char CONFIG_FILE[] = "/config.json";
#define CONFIG_MAX_SIZE 2048    /* Sanity limit for config file */

/* Pixel Types */
enum class DevMode : uint8_t {
    MPIXEL,
    MSERIAL
};

/* Test Modes */
enum class TestMode : uint8_t {
    DISABLED,
    STATIC,
    CHASE,
    RAINBOW,
    VIEW_STREAM
};

typedef struct {
    uint8_t r,g,b;              //hold requested color
    uint16_t step;               //step in testing routine
    uint32_t last;              //last update
} testing_t;

/* Configuration structure */
typedef struct {
    /* Device */
    String      id;             /* Device ID */
    DevMode     devmode;        /* Device Mode - used for reporting mode, can't be set */
    TestMode    testmode;       /* Testing mode */

    /* Network */
    String      ssid;
    String      passphrase;
    String      hostname;
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    bool        dhcp;           /* Use DHCP */
    bool        ap_fallback;    /* Fallback to AP if fail to associate */

    /* E131 */
    uint16_t    universe;       /* Universe to listen for */
    uint16_t    channel_start;  /* Channel to start listening at - 1 based */
    uint16_t    channel_count;  /* Number of channels */
    bool        multicast;      /* Enable multicast listener */

#if defined(ESPS_MODE_PIXEL)
    /* Pixels */
    PixelType   pixel_type;     /* Pixel type */
    PixelColor  pixel_color;    /* Pixel color order */
    bool        gamma;          /* Use gamma map? */

#elif defined(ESPS_MODE_SERIAL)
    /* Serial */
    SerialType  serial_type;    /* Serial type */
    BaudRate    baudrate;       /* Baudrate */
#endif
} config_t;

/* Globals */
E131            e131;
testing_t       testing;
config_t        config;
uint32_t        *seqError;      /* Sequence error tracking for each universe */
uint16_t        uniLast = 1;    /* Last Universe to listen for */
bool            reboot = false; /* Reboot flag */
AsyncWebServer  web(HTTP_PORT); /* Web Server */
AsyncWebSocket  ws("/ws");      /* Web Socket Plugin */

/* Output Drivers */
#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
PixelDriver     pixels;         /* Pixel object */
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
SerialDriver    serial;         /* Serial object */
#else
#error "No valid output mode defined."
#endif

/* Forward Declarations */
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void dsNetworkConfig(JsonObject &json);
void dsDeviceConfig(JsonObject &json);
void saveConfig();

/* Plain text friendly MAC */
String getMacAddress() {
    uint8_t mac[6];
    char macStr[18] = {0};
    WiFi.macAddress(mac);
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return  String(macStr);
}

#endif /* ESPIXELSTICK_H_ */
