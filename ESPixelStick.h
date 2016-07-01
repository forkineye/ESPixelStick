/*
* ESPixelStick.h
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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

#include "PixelDriver.h"
#include "_E131.h"

/* Name and version */
const char VERSION[] = "1.5 beta";

#define HTTP_PORT       80      /* Default web server port */
#define DATA_PIN        2       /* Pixel output - GPIO2 */
#define EEPROM_BASE     0       /* EEPROM configuration base address */
#define UNIVERSE_LIMIT  510     /* Universe boundary - 510 Channels */
#define PPU_MAX         170     /* Max pixels per Universe */
#define PIXEL_LIMIT     1360    /* Total pixel limit - 40.85ms for 8 universes */
#define E131_TIMEOUT    1000    /* Force refresh every second an E1.31 packet is not seen */
#define CONNECT_TIMEOUT 10000   /* 10 seconds */
#define REBOOT_DELAY    100     /* Delay for rebooting once reboot flag is set */
#define LOG_PORT        Serial  /* Serial port for console logging */
#define SERIAL_PORT     Serial1 /* Serial port for Renard / DMX output */

//#define LOGF(...) LOG_PORT.printf(__VA_ARGS__)  /* Macro for console logging */


/* Configuration ID, Version -- EEPROM specific, to be removed soon */
#define CONFIG_VERSION 4
const uint8_t CONFIG_ID[4] PROGMEM = { 'F', 'O', 'R', 'K'};

/* Configuration file params */
const char CONFIG_FILE[] = "/config.json";
#define CONFIG_MAX_SIZE 2048    /* Sanity limit for config file */


/* Mode Types */
enum class OutputMode : uint8_t {
    PIXEL,
    DMX512,
    RENARD
};

/* Configuration structure */
typedef struct {
    /* Network */
    char        ssid[33];       /* 32 bytes max - null terminated */
    char        passphrase[64]; /* 63 bytes max - null terminated */
    char        hostname[33];   /* 32 bytes max - null terminated */
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    boolean     dhcp;           /* Use DHCP */
    boolean     ap_fallback;    /* Fallback to AP if fail to associate */

    /* Device */
    char        id[32];         /* Device ID */
    OutputMode  mode;           /* Global Output Mode */

    /* E131 */
    uint16_t    universe;       /* Universe to listen for */
    uint16_t    channel_start;  /* Channel to start listening at - 1 based */
    uint16_t    channel_count;  /* Number of channels */
    boolean     multicast;      /* Enable multicast listener */

    /* Pixels */
    PixelType   pixel_type;     /* Pixel type */
    PixelColor  pixel_color;    /* Pixel color order */
    uint8_t     ppu;            /* Pixels per Universe boundary - Max PIXELS_MAX (Default 170) */
    float       gamma;          /* Value used to build gamma correction table */

    /* DMX */
    boolean     dmx_passthru;   /* DMX Passthrough */

    /* Renard */
    uint32_t    renard_baud;    /* Baudrate for Renard */
} config_t;

/* Globals */
E131                e131;
AsyncWebServer      web(HTTP_PORT);
config_t            config;
uint32_t            *seqError;      /* Sequence error tracking for each universe */
uint16_t            uniLast = 1;    /* Last Universe to listen for */
boolean             reboot = false; /* Flag to reboot the ESP */

const char PTYPE_HTML[] = "text/html";
const char PTYPE_PLAIN[] = "text/plain";

void saveConfig();
void updatePixelConfig();
void updateRenardConfig();
void updateDMXConfig();
void sendPage(const char *data, int count, const char *type);

#endif /* ESPIXELSTICK_H_ */
