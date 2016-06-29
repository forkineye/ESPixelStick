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

#include "ESPixelDriver.h"
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


/* Configuration ID and Version */
#define CONFIG_VERSION 4
const uint8_t CONFIG_ID[4] PROGMEM = { 'F', 'O', 'R', 'K'};

/* Mode Types */
typedef enum {
    MODE_PIXEL,
    MODE_SERIAL
} ESP_mode_t;

/* Configuration structure */
typedef struct {
    /* header */
    uint8_t     id[4];          /* Configuration structure ID */
    uint8_t     version;        /* Configuration structure version */
    uint8_t     reserved;       /* Reserved for future use - struct alignment */

    /* general config */
    char        name[32];       /* Device Name */
    ESP_mode_t  mode;           /* Global Output Mode */

    /* network config */
    char        ssid[32];       /* 31 bytes max - null terminated */
    char        passphrase[64]; /* 63 bytes max - null terminated */
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    uint8_t     dhcp;           /* DHCP enabled boolean */
    uint8_t     multicast;      /* Multicast listener enabled boolean */

    /* dmx and pixel config */
    float       gamma;          /* Value used to build gamma correction table */
    uint16_t    universe;       /* Universe to listen for */
    uint16_t    channel_start;  /* Channel to start listening at - 1 based */
    uint16_t    pixel_count;    /* Number of pixels */
    pixel_t     pixel_type;     /* Pixel type */
    color_t     pixel_color;    /* Pixel color order */
    uint8_t     ppu;            /* Pixels per Universe boundary - Max PIXELS_MAX (Default 170) */

    /* serial config */
    uint16_t    channel_count;  /* Number of channels */
    serial_t    serial_type;    /* Type of Serial Output */
    uint32_t    serial_baud;    /* Baudrate of Serial Port */
} __attribute__((packed)) config_t;

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
void updateSerialConfig();
void sendPage(const char *data, int count, const char *type);

#endif /* ESPIXELSTICK_H_ */
