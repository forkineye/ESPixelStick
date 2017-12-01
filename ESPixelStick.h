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

const char VERSION[] = "3.1-dev (gece uart)";
const char BUILD_DATE[] = __DATE__;

/*****************************************/
/*        BEGIN - Configuration          */
/*****************************************/

/* Output Mode - There can be only one! (-Conor MacLeod) */
#define ESPS_MODE_PIXEL
//#define ESPS_MODE_SERIAL

/* Include support for PWM */
//#define ESPS_SUPPORT_PWM

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncUDP.h>
#include <ESPAsyncWebServer.h>

#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#endif

#define HTTP_PORT       80      /* Default web server port */
#define MQTT_PORT       1883    /* Default MQTT port */
#define DATA_PIN        2       /* Pixel output - GPIO2 */
#define EEPROM_BASE     0       /* EEPROM configuration base address */
#define UNIVERSE_MAX    512     /* Max channels in a DMX Universe */
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
#define CONFIG_MAX_SIZE 2048    /* Sanity limit for config file */

/* Pixel Types */
class DevCap {
 public:
    bool MPIXEL : 1;
    bool MSERIAL : 1;
    bool MPWM : 1;
    uint8_t toInt() {
        return (MPWM << 2 | MSERIAL << 1 | MPIXEL);
    }
};

/*
enum class DevMode : uint8_t {
    MPIXEL,
    MSERIAL
};
*/

/* Test Modes */
enum class TestMode : uint8_t {
    DISABLED,
    STATIC,
    CHASE,
    RAINBOW,
    VIEW_STREAM,
    MQTT
};

typedef struct {
    uint8_t r, g, b;    /* Hold requested color */
    uint16_t step;      /* Step in testing routine */
    uint32_t last;      /* Last update */
} testing_t;

/* Configuration structure */
typedef struct {
    /* Device */
    String      id;             /* Device ID */
    DevCap      devmode;        /* Device Mode - used for reporting mode, can't be set */
    TestMode    testmode;       /* Testing mode */

    /* Network */
    String      ssid;
    String      passphrase;
    String      hostname;
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    bool        dhcp;           /* Use DHCP? */
    bool        ap_fallback;    /* Fallback to AP if fail to associate? */

    /* MQTT */
    bool        mqtt;           /* Use MQTT? */
    String      mqtt_ip;
    uint16_t    mqtt_port;
    String      mqtt_user;
    String      mqtt_password;
    String      mqtt_topic;

    /* E131 */
    uint16_t    universe;       /* Universe to listen for */
    uint16_t    universe_limit; /* Universe boundary limit */
    uint16_t    channel_start;  /* Channel to start listening at - 1 based */
    uint16_t    channel_count;  /* Number of channels */
    bool        multicast;      /* Enable multicast listener */

#if defined(ESPS_MODE_PIXEL)
    /* Pixels */
    PixelType   pixel_type;     /* Pixel type */
    PixelColor  pixel_color;    /* Pixel color order */
    bool        gamma;          /* Use gamma map? */
    float       gammaVal;       /* gamma value to use */
    float       briteVal;       /* brightness lto use */
#elif defined(ESPS_MODE_SERIAL)
    /* Serial */
    SerialType  serial_type;    /* Serial type */
    BaudRate    baudrate;       /* Baudrate */
#endif

#if defined(ESPS_SUPPORT_PWM)
    bool        pwm_global_enabled; /* is pwm runtime enabled? */
    int         pwm_freq;           /* pwm frequency */
    bool        pwm_gamma;          /* is pwm gamma enabled? */
    uint16_t    pwm_gpio_dmx[17];   /* which dmx channel is gpio[n] mapped to? */
    uint32_t    pwm_gpio_enabled;   /* is gpio[n] enabled? */
    uint32_t    pwm_gpio_invert;    /* is gpio[n] active high or active low? */
    uint32_t    pwm_gpio_digital;   /* is gpio[n] digital or "analog"? */
#endif
} config_t;


/* Forward Declarations */
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void dsNetworkConfig(JsonObject &json);
void dsDeviceConfig(JsonObject &json);
void saveConfig();

void connectWifi();
void onWifiConnect(const WiFiEventStationModeGotIP &event);
void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event);
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* p_payload,
        AsyncMqttClientMessageProperties properties, size_t len,size_t index, size_t total);
void publishRGBState();
void publishRGBBrightness();
void publishRGBColor();
void setStatic(uint8_t r, uint8_t g, uint8_t b);

#endif /* ESPIXELSTICK_H_ */
