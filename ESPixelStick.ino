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
#define ESPS_MODE_SERIAL

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = "SSID";
const char passphrase[] = "PASSWORD";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "PixelDriver.h"
#include "SerialDriver.h"
#include "_E131.h"
#include "_ART.h"
#include "helpers.h"

/* Output Drivers */
#if defined (ESPS_MODE_PIXEL)
#include "PixelDriver.h"
#include "page_config_pixel.h"
#endif
#if defined (ESPS_MODE_SERIAL)
#include "SerialDriver.h"
#include "page_config_serial.h"
//#else
//#error "No valid output mode defined."
#endif

/* Common Web pages and handlers */
#include "page_root.h"
#include "page_admin.h"
#include "page_config_net.h"
#include "page_status_net.h"
#include "page_status_e131.h"

/* OLED Librarys */
#include <Wire.h>
#include <SPI.h>
#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "images.h"

#if defined (ESPS_MODE_PIXEL)
PixelDriver     pixels;         /* Pixel object */
#endif
#if defined (ESPS_MODE_SERIAL)
SerialDriver    serial;         /* Serial object */
#endif

uint8_t             *seqTracker;        /* Current sequence numbers for each Universe */
uint32_t            lastUpdate;         /* Update timeout tracker */
AsyncWebServer      web(HTTP_PORT);     /* Web Server */
bool                initReady = 0;
bool*               uniGot;
uint8_t             uniTotal = 1;

/* Forward Declarations */
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void loadConfig();
int initWifi();
void initWeb();
void updateConfig();
void initOled();
bool overlay(SSD1306 *display, SSD1306UiState* state);
bool bootscreen(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool infoscreen(SSD1306 *display, SSD1306UiState* state, int x, int y);

/* OLED Display */
// Uncomment one of the following based on OLED type
// SSD1306 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI
SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SDC);    // For I2C
SSD1306Ui ui(&display );

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
bool (*frames[])(SSD1306 *display, SSD1306UiState* state, int x, int y) = { bootscreen, infoscreen};

// how many frames are there?
int frameCount = 2;

bool (*overlays[])(SSD1306 *display, SSD1306UiState* state)             = { overlay };
int overlaysCount = 1;

int remainingTimeBudget;


void setup() {
    /* Generate and set hostname */
    char chipId[7] = { 0 };
    sprintf(chipId, "%06x", ESP.getChipId());
    String hostname = "esps_" + String(chipId);
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

    /* Init OLED */
    initOled();

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
        MDNS.addServiceTxt("e131", "udp", "Manuf", "Jearde");
    } else {
        LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
    }

    /* Configure the outputs */
#if defined (ESPS_MODE_PIXEL)
    if(config.mode == MODE_PIXEL){
      pixels.setPin(DATA_PIN);
      updateConfig();
      pixels.show();
    }
    else
      updateConfig();
#else
    updateConfig();
#endif

    ui.nextFrame();
    ui.update();

    initReady = 1;
}

/* clear settings from EEPROM  if D5 is high*/
void initDefaultRequest() {
    pinMode(DEFAULT_PIN, INPUT); //INPUT_PULLUP
    if(digitalRead(DEFAULT_PIN) == LOW){
      SPIFFS.remove(CONFIG_FILE);
      LOG_PORT.println(F("SPIFFS Config Cleared!"));
    }
}

void initConfig() {
    memset(&config, 0, sizeof(config));
    //memcpy_P(config.id, CONFIG_ID, sizeof(config.id));
    //config.version = CONFIG_VERSION;
    //strncpy(config.name, "ESPixelStick", sizeof(config.name));
    config.mode = MODE_PIXEL;
    config.protocol = MODE_ARTNET;
    config.ssid = ssid;
    config.passphrase = passphrase;
    config.ip[0] = 0; config.ip[1] = 0; config.ip[2] = 0; config.ip[3] = 0;
    config.netmask[0] = 0; config.netmask[1] = 0; config.netmask[2] = 0; config.netmask[3] = 0;
    config.gateway[0] = 0; config.gateway[1] = 0; config.gateway[2] = 0; config.gateway[3] = 0;
    config.dhcp = 1;
    config.multicast = 0;
    config.universe = 0;
    config.channel_start = 1;
    //config.pixel_count = 170;
    config.pixel_type = PixelType::WS2811;
    config.pixel_color = PixelColor::GRB;
    config.ppu = 170;
    config.gamma = 1.0;
    config.channel_count=150;
    config.serial_type = SerialType::DMX512;
    config.baudrate = BaudRate::BR_250000;
    config.showrate = false;
}

int initWifi() {
    /* Switch to station mode and disconnect just in case */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.print(config.ssid);
    
    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());

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

        LOG_PORT.print(F("Protocol: "));
        LOG_PORT.println(config.protocol);
        switch(config.protocol){
          case MODE_sACN:
            if (config.multicast)
                e131.begin(E131_MULTICAST, config.universe, uniLast - config.universe + 1);
            else
                e131.begin(E131_UNICAST);
          break;
          case MODE_ARTNET:
            if (config.multicast)
                art.begin(MULTICAST, config.universe, uniLast - config.universe + 1);
            else
                art.begin(UNICAST);
          break;
        };
    }

    return WiFi.status();
}

/* Configure and start the web server */
void initWeb() {
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
    web.on("/admin.html", HTTP_POST, send_admin_html);
    web.on("/config_net.html", HTTP_POST, send_config_net_html);

    /* Static handler */
#if defined (ESPS_MODE_PIXEL)
    if(config.mode == MODE_PIXEL){
      web.on("/config/pixelvals", HTTP_GET, send_config_pixel_vals);
      web.on("/config_pixel.html", HTTP_POST, send_config_pixel_html);
      web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("pixel.html");
    }
#endif
#if defined (ESPS_MODE_SERIAL)
    if(config.mode == MODE_SERIAL){
      web.on("/config/serialvals", HTTP_GET, send_config_serial_vals);
      web.on("/config_serial.html", HTTP_POST, send_config_serial_html);
      web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("serial.html");
    }
#endif

    web.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

void initOled() {
  ui.setTargetFPS(15);
  
  //ui.setTimePerFrame(20);
  ui.disableAutoTransition();
  
  ui.setActiveSymbole(activeSymbole);
  ui.setInactiveSymbole(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  // Inital UI takes care of initalising the display too.
  ui.init();

  display.flipScreenVertically();
  ui.update();
}

/* Configuration Validations */
void validateConfig() {
#if !defined ESPS_MODE_SERIAL
    config.mode = MODE_PIXEL;
#elif !defined ESPS_MODE_PIXEL
    config.mode = MODE_SERIAL;
#endif
  
    /* E1.31 Limits */
    if (config.universe < 1)
        config.universe = 1;

    if (config.channel_start < 1)
        config.channel_start = 1;
    else if (config.channel_start > 512)
        config.channel_start = 512;

    switch(config.mode){
      case MODE_PIXEL:
      /* Generic channel limits for pixels */
        if (config.channel_count > PIXEL_LIMIT * 3)
            config.channel_count = PIXEL_LIMIT * 3;
        else if (config.channel_count < 1)
            config.channel_count = 1;
      break;
      case MODE_SERIAL:
        /* Generic serial channel limits */
        if (config.channel_count > SERIAL_LIMIT)
            config.channel_count = SERIAL_LIMIT;
        else if (config.channel_count < 1)
            config.channel_count = 1;
      break;
    };
    
#if defined (ESPS_MODE_PIXEL)    
    /* PPU Limits */
    if (config.ppu > PPU_MAX || config.ppu < 1)
        config.ppu = PPU_MAX;

    /* GECE Limits */
    if (config.pixel_type == PixelType::GECE) {
        uniLast = config.universe;
        config.pixel_color = PixelColor::RGB;
        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
    } else {
        uint16_t bounds = config.ppu * 3;
        //uint16_t count = config.channel_count * 3;
        uint16_t dmxchannelend = config.channel_count + config.channel_start  -1;
        if (dmxchannelend != 512)
            uniLast = config.universe + dmxchannelend / 512;
        else 
            uniLast = config.universe + dmxchannelend / 512 -1;
    }

#elif defined (ESPS_MODE_SERIAL)
    /* Baud rate check */
    if (config.baudrate > BaudRate::BR_250000)
        config.baudrate = BaudRate::BR_250000;
    else if (config.baudrate < BaudRate::BR_38400)
        config.baudrate = BaudRate::BR_57600;
#endif
}

void updateConfig() {
    /* Validate first */
    validateConfig();

    /* Setup the sequence error tracker */
    uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if (seqTracker = static_cast<uint8_t *>(malloc(uniTotal)))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if (seqError = static_cast<uint32_t *>(malloc(uniTotal * 4)))
        memset(seqError, 0x00, uniTotal * 4);

    /* Zero out packet stats */
    e131.stats.num_packets = 0;
    
    switch(config.protocol){
      case MODE_sACN:
        e131.stats.num_packets = 0;
      break;
      case MODE_ARTNET:
        art.stats.num_packets = 0;
      break;
    };
    

    /* Initialize for our pixel type */
#if defined (ESPS_MODE_PIXEL)
    if(config.mode == MODE_PIXEL){
      pixels.begin(config.pixel_type, config.pixel_color);
      pixels.updateLength(config.channel_count / 3);
      pixels.setGamma(config.gamma);
    }
#endif 
#if defined (ESPS_MODE_SERIAL)    
    if(config.mode == MODE_SERIAL){
      serial.begin(&SERIAL_PORT, config.serial_type, config.channel_count, config.baudrate);
    }
#endif    
    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.print(uniLast);
    LOG_PORT.print(F(" : "));
    LOG_PORT.println(uniTotal);

    //uniGot = new bool[uniTotal];

    //TODO Crash
    //if(initReady == 1)
      //ui.update();
}

/* Load configugration JSON file */
void loadConfig() {
    /* Zeroize Config struct */
    memset(&config, 0, sizeof(config));

    initDefaultRequest();
    
    /* Load CONFIG_FILE json. Create and init with defaults if not found */
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        initConfig();
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
        
        uint8_t mode_t = json["device"]["mode"];
        if(mode_t == 0)
          config.mode = MODE_PIXEL;
        else if(mode_t == 1)
          config.mode = MODE_SERIAL;
        mode_t = json["device"]["protocol"];
        if(mode_t == 0)
          config.protocol = MODE_sACN;
        else if(mode_t == 1)
          config.protocol = MODE_ARTNET;
        else
          LOG_PORT.println(F("*** Failed to ser. Mode ***"));
        
        config.showrate = json["device"]["showrate"];

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
switch(config.protocol){
        case MODE_sACN:
        /* E131 */
        config.universe = json["e131"]["universe"];
        config.channel_start = json["e131"]["channel_start"];
        config.channel_count = json["e131"]["channel_count"];
        config.multicast = json["e131"]["multicast"];
        break;
        case MODE_ARTNET:
        /* ARTNET */
        config.universe = json["art"]["universe"];
        config.channel_start = json["art"]["channel_start"];
        config.channel_count = json["art"]["channel_count"];
        config.multicast = json["art"]["multicast"];
        break;
}; 

#if defined (ESPS_MODE_PIXEL)
        /* Pixel */
        config.pixel_type = PixelType(static_cast<uint8_t>(json["pixel"]["type"]));
        config.pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
        config.ppu = json["pixel"]["ppu"];
        config.gamma = json["pixel"]["gamma"];
#endif   
#if defined (ESPS_MODE_SERIAL)
        /* Serial */
        config.serial_type = SerialType(static_cast<uint8_t>(json["serial"]["type"]));
        config.baudrate = BaudRate(static_cast<uint32_t>(json["serial"]["baudrate"]));
#endif        
        
        LOG_PORT.println(F("- Configuration loaded."));
    }

    /* Validate it */
    validateConfig();
    
    if(initReady == 1)
      ui.update();
}

/* Serialize the current config into a JSON string */
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    /* Create buffer and root object */
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    /* Device */
    JsonObject &device = json.createNestedObject("device");
    device["id"] = config.id.c_str();

    //TODO Make json objects
    
    uint8_t mode_t = 3;
    if(config.mode == MODE_PIXEL)
      mode_t = 0;
    else if(config.mode == MODE_SERIAL)
      mode_t = 1;
    device["mode"] = mode_t;
    
    mode_t = 3;
    if(config.protocol == MODE_sACN)
      mode_t = 0;
    else if(config.protocol == MODE_ARTNET)
      mode_t = 1;
    else
      LOG_PORT.println(F("*** Failed to ser. Mode ***"));
    device["protocol"] = mode_t;

    device["showrate"] = config.showrate;
    
    //device["mode"] = config.mode;
    //device["protocol"] = config.protocol;

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

    /* ARTNET */
    JsonObject &art = json.createNestedObject("art");
    art["universe"] = config.universe;
    art["channel_start"] = config.channel_start;
    art["channel_count"] = config.channel_count;
    art["multicast"] = config.multicast;

#if defined (ESPS_MODE_PIXEL)
    /* Pixel */
    JsonObject &pixel = json.createNestedObject("pixel");
    pixel["type"] = static_cast<uint8_t>(config.pixel_type);
    pixel["color"] = static_cast<uint8_t>(config.pixel_color);
    pixel["ppu"] = config.ppu;
    pixel["gamma"] = config.gamma;
#endif   
#if defined (ESPS_MODE_SERIAL)
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
//    updateConfig();

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
        LOG_PORT.println(F("* New configuration saved."));
    }
}


bool overlay(SSD1306 *display, SSD1306UiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, WiFi.localIP().toString());
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 0, (String)ESP.getChipId());
  switch(config.protocol){
    case MODE_sACN:
      display->drawString(10, 50,"sACN");
    break;
    case MODE_ARTNET:
      display->drawString(10, 50,"ArtNet");
    break;
  };
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(118, 50, "v" + (String)VERSION);
  return true;
}

bool bootscreen(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  // draw an xbm image.
  // Please note that everything that should be transitioned
  // needs to be drawn relative to x and y

  // if this frame need to be refreshed at the targetFPS you need to
  // return true
  display->drawXbm(x + 34, y + 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  return false;
}

bool infoscreen(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  // Text alignment demo
  display->setFont(ArialMT_Plain_10);

  // The coordinates define the left starting point of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 11 + y, "Uni: " + (String)config.universe + " - " + (String)uniLast);

  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 22, "Addr: " + (String)config.channel_start + " -> " + (String)(config.channel_count) + "Ch");

  // The coordinates define the right end of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String opmode;
  switch(config.mode){
    case MODE_PIXEL:
      opmode = "Pixel";
    break;
    case MODE_SERIAL:
      opmode = "Serial";
    break;  
  };
  display->drawString(0 + x, 33, "Multicast: " + (String)config.multicast + "  Mode: " + opmode);
  return false;
}
/*
bool checkUnivseresSend(){
  LOG_PORT.print(F("uniGot: ["));
  for (int i=0; i<=uniTotal; i++){
    if(uniGot[i] != true){
      LOG_PORT.print(F("0"));
      LOG_PORT.print(F("]"));
    }
  }
  
  for (int i=0; i<=uniTotal;i++)
    uniGot[i] = false;
  LOG_PORT.println(F("]"));
  return true;
}
*/

/* Main Loop */
void loop() {
    /* Reboot handler */
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }
switch(config.mode){
#if defined (ESPS_MODE_PIXEL)
  case MODE_PIXEL:
    /* Parse a packet and update pixels */
    switch(config.protocol){
    case MODE_sACN:
     if(e131.parsePacket()) {
        if ((e131.universe >= config.universe) && (e131.universe <= uniLast)) {
            /* Universe offset and sequence tracking */
            uint8_t uniOffset = (e131.universe - config.universe);
            if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                seqError[uniOffset]++;
                seqTracker[uniOffset] = e131.packet->sequence_number + 1;
            }

            /* Calculate how many pixels we need from this buffer */
            uint16_t pixelStop = config.channel_count / 3;

            /* Offset the channel if required for the first universe */
            uint16_t offset = 0;
            uint16_t pixelStart = 0;
            if (e131.universe == config.universe)
                offset = config.channel_start - 1;
            else if(uniOffset == 1)
                pixelStart = (512 - config.channel_start) / 3;
            else
                pixelStart = ((512 - config.channel_start) / 3) + config.ppu * (uniOffset-1);

            /* Set the pixel data */
            uint16_t buffloc = 0;
            for (int i = pixelStart; i < pixelStop; i++) {
                int j = buffloc++ * 3 + offset;
                pixels.setPixelColor(i, e131.data[j], e131.data[j+1], e131.data[j+2]);
            }

            /*
            LOG_PORT.print(F("Universe Package: "));
            LOG_PORT.println(e131.universe);
            */
            /* Refresh when last universe shows up */
            if (config.showrate == true || e131.universe == uniLast) {
                lastUpdate = millis();
                pixels.show();
            }
        }
     }
    break;
    case MODE_ARTNET:
     if(art.parsePacket()) {
        if ((art.universe >= config.universe) && (art.universe <= uniLast)) {
            /* Universe offset and sequence tracking */
            uint8_t uniOffset = (art.universe - config.universe);
            if (art.packet->sequence_number != seqTracker[uniOffset]++) {
                seqError[uniOffset]++;
                seqTracker[uniOffset] = art.packet->sequence_number + 1;
            }

            /* Calculate how many pixels we need from this buffer */
            uint16_t pixelStop = config.channel_count / 3;

            /* Offset the channel if required for the first universe */
            uint16_t offset = 0;
            uint16_t pixelStart = 0;
            if (art.universe == config.universe)
                offset = config.channel_start - 1;
            else if(uniOffset == 1)
                pixelStart = (512 - config.channel_start) / 3;
            else
                pixelStart = ((512 - config.channel_start) / 3) + config.ppu * (uniOffset-1);

            /* Set the pixel data */
            uint16_t buffloc = 0;
            for (int i = pixelStart; i < pixelStop; i++) {
                int j = buffloc++ * 3 + offset;
                pixels.setPixelColor(i, art.data[j], art.data[j+1], art.data[j+2]);
            }

            /* Refresh when last universe shows up */
            if (config.showrate == true || art.universe == uniLast) {
                lastUpdate = millis();
                pixels.show();
            }
        }
     }
    break;
    }; 

    //TODO: Use this for setting defaults states at a later date
    /* Force refresh every second if there is no data received */
    if ((millis() - lastUpdate) > E131_TIMEOUT) {
        lastUpdate = millis();
        pixels.show();
    }
  break;
#endif
#if defined (ESPS_MODE_SERIAL)
  case MODE_SERIAL:
    /* Parse a packet and update Serial */
    switch(config.protocol){
    case MODE_sACN:
      if (e131.parsePacket()) {
          if (e131.universe == config.universe) {
              /* Universe offset and sequence tracking */
              uint8_t uniOffset = (e131.universe - config.universe);
              if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                  seqError[uniOffset]++;
                  seqTracker[uniOffset] = e131.packet->sequence_number + 1;
              }
  
              uint16_t offset = config.channel_start - 1;
  
              /* Set the serial data */
              serial.startPacket();
              for(int i = 0; i<config.channel_count; i++) {
                  serial.setValue(i, e131.data[i + offset]);    
              }
  
              /* Refresh */
              serial.show();
          }
      }
    break;
    case MODE_ARTNET:
      if (art.parsePacket()) {
          if (art.universe == config.universe) {
              /* Universe offset and sequence tracking */
              uint8_t uniOffset = (art.universe - config.universe);
              if (art.packet->sequence_number != seqTracker[uniOffset]++) {
                  seqError[uniOffset]++;
                  seqTracker[uniOffset] = art.packet->sequence_number + 1;
              }
  
              uint16_t offset = config.channel_start - 1;
  
              /* Set the serial data */
              serial.startPacket();
              for(int i = 0; i<config.channel_count; i++) {
                  serial.setValue(i, art.data[i + offset]);    
              }
  
              /* Refresh */
              serial.show();
          }
      }
    break;
    };
  break;
#endif
  }; 
}
