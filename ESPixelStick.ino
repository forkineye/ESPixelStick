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
*  TODO Virtual Variables
*
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include "ESPixelDriver.h"
#include "ESerialDriver.h"
#include "ESPixelStick.h"
#include "_E131.h"
#include "helpers.h"
#include "_ART.h"

/* OLED Librarys */
#include <Wire.h>
#include <SPI.h>
#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "images.h"

/* Web pages and handlers */
#include "page_header.h"
#include "page_root.h"
#include "page_admin.h"
#include "page_config_net.h"
#include "page_config_pixel.h"
#include "page_config_serial.h"
#include "page_status_net.h"
#include "page_status_e131.h"


/*************************************************/
/*      BEGIN - User Configuration Defaults      */
/*************************************************/

/* REQUIRED */
const char ssid[] = "ThorNet";             /* Replace with your SSID */
const char passphrase[] = "LepaGGX86";   /* Replace with your WPA2 passphrase */


// Pin definitions for I2C
#define OLED_SDA    D2  // pin 14
#define OLED_SDC    D3  // D3 pin 12
#define OLED_ADDR   0x3C

/* Hardware Wemos D1 mini SPI pins
 D5 GPIO14   CLK         - D0 pin OLED display
 D6 GPIO12   MISO (DIN)  - not connected
 D7 GPIO13   MOSI (DOUT) - D1 pin OLED display
 D8 GPIO15   CS / SS     - CS pin OLED display
 D0 GPIO16   RST         - RST pin OLED display
 D2 GPIO4    DC          - DC pin OLED
*/

// Pin definitions for SPI
#define OLED_RESET  D0  // RESET
#define OLED_DC     D2  // Data/Command
#define OLED_CS     D8  // Chip select

// Uncomment one of the following based on OLED type
// SSD1306 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI
SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SDC);    // For I2C
SSD1306Ui ui(&display );

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
bool (*frames[])(SSD1306 *display, SSD1306UiState* state, int x, int y) = { drawFrame1, drawFrame2};

// how many frames are there?
int frameCount = 2;

bool (*overlays[])(SSD1306 *display, SSD1306UiState* state)             = { msOverlay };
int overlaysCount = 1;

int remainingTimeBudget;


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

ESPixelDriver  pixels;         /* Pixel object */
ESerialDriver serial;         /*serial object */
uint8_t         *seqTracker;    /* Current sequence numbers for each Universe */
uint32_t        lastPacket;     /* Packet timeout tracker */
bool            initReady = 0;

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
    initDefaultRequest();
    
    loadConfig();

    initOled();
	  
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
        Serial.println(F("**** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ****"));
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
    switch(config.mode){
      case MODE_PIXEL:
        pixels.setPin(DATA_PIN);    /* For protocols that require bit-banging */
        updatePixelConfig();
        pixels.show();
      break;
      case MODE_SERIAL:
        serial.begin(&Serial, config.serial_type, config.channel_count, config.serial_baud);

      break;
      
    };

    //TODO Upper class
    switch(config.protocol){
      case MODE_sACN:
        //e131 = new E131;
      break;
      case MODE_ARTNET:
        //e131 = new ART;
      break;
    };

    ui.nextFrame();
    ui.update();

    initReady = 1;

}

/* clear settings from EEPROM  if D5 is high*/
void initDefaultRequest() {
    pinMode(DEFAULT_PIN, INPUT); //INPUT_PULLUP
    if(digitalRead(DEFAULT_PIN) == LOW){
      for (int i = 0 ; i < sizeof(config) ; i++) {
        EEPROM.write(i, 0);
      }
      Serial.println("EEPROM Cleared!");
    }
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

        switch(config.protocol){
          case MODE_sACN:
            if (config.multicast)
                e131.begin(MULTICAST, config.universe, uniLast - config.universe + 1);
            else
                e131.begin(UNICAST);
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
    
    /* Mode Specific Pages */
    switch(config.mode){
      case MODE_PIXEL:
        //HTML
        web.on("/", []() { sendPage(PAGE_ROOT_PIXEL, sizeof(PAGE_ROOT_PIXEL), PTYPE_HTML); });
        web.on("/config/pixel.html", send_config_pixel_html);
        //AJAX
        web.on("/config/pixelvals", send_config_pixel_vals);
      break;
      case MODE_SERIAL:
        //HTML
        web.on("/", []() { sendPage(PAGE_ROOT_SERIAL, sizeof(PAGE_ROOT_SERIAL), PTYPE_HTML); });
        web.on("/config/serial.html", send_config_serial_html);
        //AJAX
        web.on("/config/serialvals", send_config_serial_vals);
      break;
    };
    
    /*Common Pages */
    /* HTML Pages */
    web.on("/admin.html", send_admin_html);
    web.on("/config/net.html", send_config_net_html);
    web.on("/status/net.html", []() { sendPage(PAGE_STATUS_NET, sizeof(PAGE_STATUS_NET), PTYPE_HTML); });
    web.on("/status/e131.html", []() { sendPage(PAGE_STATUS_E131, sizeof(PAGE_STATUS_E131), PTYPE_HTML); });

    /* AJAX Handlers */
    web.on("/rootvals", send_root_vals);
    web.on("/adminvals", send_admin_vals);
    web.on("/config/netvals", send_config_net_vals);
    web.on("/config/connectionstate", send_connection_state_vals);
    web.on("/status/netvals", send_status_net_vals);
    web.on("/status/e131vals", send_status_e131_vals);

    /* Admin Handlers */
    web.on("/reboot", []() { sendPage(PAGE_ADMIN_REBOOT, sizeof(PAGE_ADMIN_REBOOT), PTYPE_HTML); ESP.restart(); });

    web.onNotFound([]() { web.send(404, PTYPE_HTML, "Page not Found"); });
    web.begin();

    Serial.print(F("- Web Server started on: "));
    if(WiFi.status() != WL_CONNECTED)
      Serial.print(WiFi.softAPIP());
    else
      Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(HTTP_PORT);
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
        //uint16_t dmxchannelend = count - ((512 - config.channel_start) / 3) * 3;
        uint16_t dmxchannelend = count + config.channel_start  -1;
        
        if (dmxchannelend != 512) // count % bounds
            //uniLast = config.universe + count / bounds;
            uniLast = config.universe + dmxchannelend / 512;
        else 
            uniLast = config.universe + dmxchannelend / 512 -1;
        /*    
        if (count % bounds) // count % bounds
            //uniLast = config.universe + count / bounds;
            uniLast = config.universe + count / bounds;
        else 
            uniLast = config.universe + count / bounds - 1;
        */
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

    if(initReady == 1)
      ui.update();

}

void updateSerialConfig() {
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

    switch(config.protocol){
      case MODE_sACN:
        e131.stats.num_packets = 0;
      break;
      case MODE_ARTNET:
        e131.stats.num_packets = 0;
      break;
    }; 
    
    /* Initialize for our serial type */
    serial.begin(&Serial, config.serial_type, config.channel_count, config.serial_baud);

    Serial.print(F("- Listening for "));
    Serial.print(config.channel_count);
    Serial.print(F(" channels, from Universe "));
    Serial.print(config.universe);
    Serial.print(F(" to "));
    Serial.println(uniLast);

    if(initReady == 1)
      ui.update();
}

/* Initialize configuration structure */
void initConfig() {
    memset(&config, 0, sizeof(config));
    memcpy_P(config.id, CONFIG_ID, sizeof(config.id));
    config.version = CONFIG_VERSION;
    strncpy(config.name, "ESPixelStick", sizeof(config.name));
    config.mode = MODE_PIXEL;
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
    config.channel_count=150;
    config.serial_type=SERIAL_RENARD;
    config.serial_baud=57600;
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

    if(initReady == 1)
      ui.update();
}

void saveConfig() {  
    /* Write the configuration structre */
    EEPROM.put(EEPROM_BASE, config);
    EEPROM.commit();
    Serial.println(F("* New configuration saved."));
}

bool msOverlay(SSD1306 *display, SSD1306UiState* state) {
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

bool drawFrame1(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  // draw an xbm image.
  // Please note that everything that should be transitioned
  // needs to be drawn relative to x and y

  // if this frame need to be refreshed at the targetFPS you need to
  // return true
  display->drawXbm(x + 34, y + 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  return false;
}

bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  // Text alignment demo
  display->setFont(ArialMT_Plain_10);

  // The coordinates define the left starting point of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 11 + y, "Uni: " + (String)config.universe + " - " + (String)uniLast);

  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 22, "Addr: " + (String)config.channel_start + " -> " + (String)(config.pixel_count * 3) + "Ch");

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

/* Main Loop */
void loop() {
    /* Handle incoming web requests if needed */
  web.handleClient();

  switch(config.protocol){
  case MODE_sACN:
    /* Configure our outputs and pixels */
    switch(config.mode){
    case MODE_PIXEL:
        /* Parse a packet and update pixels */
        if(e131.parsePacket()) {
          
          if ((e131.universe >= config.universe) && (e131.universe <= uniLast)) {

            /* Universe offset and sequence tracking */
            uint8_t uniOffset = (e131.universe - config.universe);
            
            if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
                seqError[uniOffset]++;
                seqTracker[uniOffset] = e131.packet->sequence_number + 1;
            }

            /* Find out starting pixel based off the Universe and Startaddress*/
            //uint16_t pixelStart = uniOffset * config.ppu;

            /* Calculate how many pixels we need from this buffer */
            uint16_t pixelStop = config.pixel_count;
            //if ((pixelStart + config.ppu) < pixelStop)
            //    pixelStop = pixelStart + config.ppu;

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


            //TODO Flickering of next universe
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
      
      /* Parse a packet and update serial */
      case MODE_SERIAL:
        if(e131.parsePacket()) {
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
            serial.startPacket();
            for(int i = 0; i<config.channel_count; i++){
              serial.setValue(i, e131.data[i + offset]);  
            }

            // Refresh  
            serial.show();
          }
        }
              
      break;
        
      };
  break;
  
  case MODE_ARTNET:
    /* Configure our outputs and pixels */
    switch(config.mode){
    case MODE_PIXEL:
        /* Parse a packet and update pixels */
        if(art.parsePacket()) {
          
          if ((art.universe >= config.universe) && (art.universe <= uniLast)) {
            /* Universe offset and sequence tracking */
            uint8_t uniOffset = (art.universe - config.universe);
            
            if (art.packet->sequence_number != seqTracker[uniOffset]++) {
                seqError[uniOffset]++;
                seqTracker[uniOffset] = art.packet->sequence_number + 1;
            }

            /* Find out starting pixel based off the Universe and Startaddress*/
            //uint16_t pixelStart = uniOffset * config.ppu;

            /* Calculate how many pixels we need from this buffer */
            uint16_t pixelStop = config.pixel_count;
            //if ((pixelStart + config.ppu) < pixelStop)
            //    pixelStop = pixelStart + config.ppu;

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


            //TODO Flickering of next universe
            /* Refresh when last universe shows up */
            if (art.universe == uniLast) {
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
      
      /* Parse a packet and update serial */
      case MODE_SERIAL:
        if(art.parsePacket()) {
          if (art.universe == config.universe) {
            // Universe offset and sequence tracking 
           /* uint8_t uniOffset = (e131.universe - config.universe);
            if (e131.packet->sequence_number != seqTracker[uniOffset]++) {
              seqError[uniOffset]++;
              seqTracker[uniOffset] = e131.packet->sequence_number + 1;
            }
            */
            uint16_t offset = config.channel_start - 1;

            // Set the serial data 
            serial.startPacket();
            for(int i = 0; i<config.channel_count; i++){
              serial.setValue(i, art.data[i + offset]);  
            }

            // Refresh  
            serial.show();
          }
        }
              
      break;
        
      };
  break;
  }; 
    
    
    
}
