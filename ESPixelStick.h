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
#endif
#if defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#endif
#include "_E131.h"
#include "_ART.h"

#include "SSD1306.h"
#include "SSD1306Ui.h"

/* Name and version */
const char VERSION[] = "1.5 beta";

#define HTTP_PORT       80      /* Default web server port */
#define DATA_PIN        D4      /* Pixel output - GPIO2 */
#define EEPROM_BASE     0       /* EEPROM configuration base address */
#define UNIVERSE_LIMIT  510     /* Universe boundary - 510 Channels */
#define PPU_MAX         170     /* Max pixels per Universe */
#define PIXEL_LIMIT     1360    /* Total pixel limit - 40.85ms for 8 universes */
#define SERIAL_LIMIT    512     /* Channel limite for serial outputs */
#define E131_TIMEOUT    1000    /* Force refresh every second an E1.31 packet is not seen */
#define CONNECT_TIMEOUT 10000   /* 10 seconds */
#define REBOOT_DELAY    100     /* Delay for rebooting once reboot flag is set */
#define LOG_PORT        Serial  /* Serial port for console logging */
#define SERIAL_PORT     Serial /* Serial port for Renard / DMX output */ //Serial1

/* E1.33 / RDMnet stuff - to be moved to library */
#define RDMNET_DNSSD_SRV_TYPE   "draft-e133.tcp"
#define RDMNET_DEFAULT_SCOPE    "default"
#define RDMNET_DEFAULT_DOMAIN   "local"
#define RDMNET_DNSSD_TXTVERS    1
#define RDMNET_DNSSD_E133VERS   1

/* Configuration file params */
const char CONFIG_FILE[] = "/config.json";
#define CONFIG_MAX_SIZE 2048    /* Sanity limit for config file */

#define DEFAULT_PIN     D5


/* Mode Types */
typedef enum{
    MODE_PIXEL,
    MODE_SERIAL
} ESP_mode_t;

/* Protocol Types */
typedef enum{
    MODE_sACN,
    MODE_ARTNET
} stream_mode_t;

/* Configuration structure */
typedef struct {
    /* Device */
    String        id;           /* Device ID */
    ESP_mode_t    mode;         /* Global Output Mode */
    stream_mode_t protocol;     /* Streaming Protocol choose */
    bool          showrate;     /* Refresh every universe (true) or after last(false) */
    

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
    uint8_t     ppu;            /* Pixels per Universe boundary */
    float       gamma;          /* Value used to build gamma correction table */
#endif
#if defined(ESPS_MODE_SERIAL)
    /* Serial */
    SerialType  serial_type;    /* Serial type */
    BaudRate    baudrate;       /* Baudrate */
#endif
} config_t;

/* Globals */
E131            e131;
ART             art;
config_t        config;
uint32_t        *seqError;      /* Sequence error tracking for each universe */
uint16_t        uniLast = 1;    /* Last Universe to listen for */
bool            reboot = false; /* Flag to reboot the ESP */

/* Called from web handlers */
void saveConfig();
void updateConfig();

#endif /* ESPIXELSTICK_H_ */
