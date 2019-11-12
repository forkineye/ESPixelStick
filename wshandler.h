/*
* wshandler.h
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

#ifndef WSHANDLER_H_
#define WSHANDLER_H_

#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
extern PixelDriver  pixels;     // Pixel object
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
extern SerialDriver serial;     // Serial object
#endif

extern EffectEngine effects;    // EffectEngine for test modes

extern ESPAsyncE131 e131;       // ESPAsyncE131 with X buffers
extern ESPAsyncDDP  ddp;        // ESPAsyncDDP with X buffers
extern config_t     config;     // Current configuration
extern uint32_t     *seqError;  // Sequence error tracking for each universe
extern uint16_t     uniLast;    // Last Universe to listen for
extern bool         reboot;     // Reboot flag

extern const char CONFIG_FILE[];

/*
  Packet Commands
    E1 - Get Elements

    G1 - Get Config
    G2 - Get Config Status
    G3 - Get Current Effect and Effect Config Options
    G4 - Get Gamma table values

    T0 - Disable Testing
    T1 - Static Testing
    T2 - Blink Test
    T3 - Flash Test
    T4 - Chase Test
    T5 - Rainbow Test
    T6 - Fire flicker
    T7 - Lightning
    T8 - Breathe

    V1 - View Stream

    S1 - Set Network Config
    S2 - Set Device Config
    S3 - Set Effect Startup Config
    S4 - Set Gamma and Brightness (but dont save)

    XJ - Get RSSI,heap,uptime, e131 stats in json

    X6 - Reboot
*/

EFUpdate efupdate;
uint8_t * WSframetemp;
uint8_t * confuploadtemp;

void procX(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case 'J': {

            DynamicJsonDocument json(1024);

            // system statistics
            JsonObject system = json.createNestedObject("system");
            system["rssi"] = (String)WiFi.RSSI();
            system["freeheap"] = (String)ESP.getFreeHeap();
            system["uptime"] = (String)millis();

            // E131 statistics
            JsonObject e131J = json.createNestedObject("e131");
            uint32_t seqErrors = 0;
            for (int i = 0; i < ((uniLast + 1) - config.universe); i++)
                seqErrors =+ seqError[i];

            e131J["universe"] = (String)config.universe;
            e131J["uniLast"] = (String)uniLast;
            e131J["num_packets"] = (String)e131.stats.num_packets;
            e131J["seq_errors"] = (String)seqErrors;
            e131J["packet_errors"] = (String)e131.stats.packet_errors;
            e131J["last_clientIP"] = e131.stats.last_clientIP.toString();

            JsonObject ddpJ = json.createNestedObject("ddp");
            ddpJ["num_packets"] = (String)ddp.stats.packetsReceived;
            ddpJ["seq_errors"] = (String)ddp.stats.errors;
            ddpJ["num_bytes"] = (String)ddp.stats.bytesReceived;
            ddpJ["max_channel"] = (String)ddp.stats.ddpMaxChannel;
            ddpJ["min_channel"] = (String)ddp.stats.ddpMinChannel;
            
            String response;
            serializeJson(json, response);
            client->text("XJ" + response);
            break;
        }

        case '6':  // Init 6 baby, reboot!
            reboot = true;
    }
}

void procE(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1':
            // Create buffer and root object
            DynamicJsonDocument json(1024);

#if defined (ESPS_MODE_PIXEL)
            // Pixel Types
            JsonObject p_type = json.createNestedObject("p_type");
            p_type["WS2811 800kHz"] = static_cast<uint8_t>(PixelType::WS2811);
            p_type["GE Color Effects"] = static_cast<uint8_t>(PixelType::GECE);

            // Pixel Colors
            JsonObject p_color = json.createNestedObject("p_color");
            p_color["RGB"] = static_cast<uint8_t>(PixelColor::RGB);
            p_color["GRB"] = static_cast<uint8_t>(PixelColor::GRB);
            p_color["BRG"] = static_cast<uint8_t>(PixelColor::BRG);
            p_color["RBG"] = static_cast<uint8_t>(PixelColor::RBG);
            p_color["GBR"] = static_cast<uint8_t>(PixelColor::GBR);
            p_color["BGR"] = static_cast<uint8_t>(PixelColor::BGR);

#elif defined (ESPS_MODE_SERIAL)
            // Serial Protocols
            JsonObject s_proto = json.createNestedObject("s_proto");
            s_proto["DMX512"] = static_cast<uint8_t>(SerialType::DMX512);
            s_proto["Renard"] = static_cast<uint8_t>(SerialType::RENARD);

            // Serial Baudrates
            JsonObject s_baud = json.createNestedObject("s_baud");
            s_baud["38400"] = static_cast<uint32_t>(BaudRate::BR_38400);
            s_baud["57600"] = static_cast<uint32_t>(BaudRate::BR_57600);
            s_baud["115200"] = static_cast<uint32_t>(BaudRate::BR_115200);
            s_baud["230400"] = static_cast<uint32_t>(BaudRate::BR_230400);
            s_baud["250000"] = static_cast<uint32_t>(BaudRate::BR_250000);
            s_baud["460800"] = static_cast<uint32_t>(BaudRate::BR_460800);
#endif

            String response;
            serializeJson(json, response);
            client->text("E1" + response);
            break;
    }
}

void procG(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1': {
            String response;
            serializeConfig(response, false, true);
            client->text("G1" + response);
            break;
        }

        case '2': {
            // Create buffer and root object
            DynamicJsonDocument json(1024);

            json["ssid"] = (String)WiFi.SSID();
            json["hostname"] = (String)WiFi.hostname();
            json["ip"] = WiFi.localIP().toString();
            json["mac"] = WiFi.macAddress();
            json["version"] = (String)VERSION;
            json["built"] = (String)BUILD_DATE;
            json["flashchipid"] = String(ESP.getFlashChipId(), HEX);
            json["usedflashsize"] = (String)ESP.getFlashChipSize();
            json["realflashsize"] = (String)ESP.getFlashChipRealSize();
            json["freeheap"] = (String)ESP.getFreeHeap();

            String response;
            serializeJson(json, response);
            client->text("G2" + response);
            break;
        }

        case '3': {
            String response;
            DynamicJsonDocument json(2048);

// dump the current running effect options
            JsonObject effect = json.createNestedObject("currentEffect");
            if (config.ds == DataSource::E131) {
                effect["name"] = "Disabled";
            } else {
                effect["name"] = (String)effects.getEffect() ? effects.getEffect() : "";
            }
            effect["brightness"] = effects.getBrightness();
            effect["speed"] = effects.getSpeed();
            effect["r"] = effects.getColor().r;
            effect["g"] = effects.getColor().g;
            effect["b"] = effects.getColor().b;
            effect["reverse"] = effects.getReverse();
            effect["mirror"] = effects.getMirror();
            effect["allleds"] = effects.getAllLeds();
            effect["startenabled"] = config.effect_startenabled;
            effect["idleenabled"] = config.effect_idleenabled;
            effect["idletimeout"] = config.effect_idletimeout;


// dump all the known effect and options
            JsonObject effectList = json.createNestedObject("effectList");
            for(int i=0; i < effects.getEffectCount(); i++){
                JsonObject effect = effectList.createNestedObject( effects.getEffectInfo(i)->htmlid );
                effect["name"] = effects.getEffectInfo(i)->name;
                effect["htmlid"] = effects.getEffectInfo(i)->htmlid;
                effect["hasColor"] = effects.getEffectInfo(i)->hasColor;
                effect["hasMirror"] = effects.getEffectInfo(i)->hasMirror;
                effect["hasReverse"] = effects.getEffectInfo(i)->hasReverse;
                effect["hasAllLeds"] = effects.getEffectInfo(i)->hasAllLeds;
                effect["wsTCode"] = effects.getEffectInfo(i)->wsTCode;
            }

            serializeJson(json, response);
            client->text("G3" + response);
//LOG_PORT.print(response);
            break;
        }
#if defined(ESPS_MODE_PIXEL)
        case '4': {
            DynamicJsonDocument json(1024);
            JsonArray gamma = json.createNestedArray("gamma");
            for (int i=0; i<256; i++) {
                gamma.add(GAMMA_TABLE[i]);
            }
            String response;
            serializeJson(json, response);
            client->text("G4" + response);
            break;
        }
#endif
    }
}

void procS(uint8_t *data, AsyncWebSocketClient *client) {

    DynamicJsonDocument json(1024);
    DeserializationError error = deserializeJson(json, reinterpret_cast<char*>(data + 2));

    if (error) {
        LOG_PORT.println(F("*** procS(): Parse Error ***"));
        LOG_PORT.println(reinterpret_cast<char*>(data));
        return;
    }

    bool reboot = false;
    switch (data[1]) {
        case '1':   // Set Network Config
            dsNetworkConfig(json.as<JsonObject>());
            saveConfig();
            client->text("S1");
            break;
        case '2':   // Set Device Config
            // Reboot if MQTT changed
            if (config.mqtt != json["mqtt"]["enabled"])
                reboot = true;

            dsDeviceConfig(json.as<JsonObject>());
            saveConfig();

            if (reboot)
                client->text("S1");
            else
                client->text("S2");
            break;
        case '3':   // Set Effect Startup Config
            dsEffectConfig(json.as<JsonObject>());
            saveConfig();
            client->text("S3");
            break;
#if defined(ESPS_MODE_PIXEL)
        case '4':   // Set Gamma (but no save)
            dsGammaConfig(json.as<JsonObject>());
            client->text("S4");
            break;
#endif
    }
}

void procT(uint8_t *data, AsyncWebSocketClient *client) {

    if (data[1] == '0') {
            //TODO: Store previous data source when effect is selected so we can switch back to it
            config.ds = DataSource::E131;
            effects.clearAll();
    }
    else if ( (data[1] >= '1') && (data[1] <= '8') ) {
        String TCode;
        TCode += (char)data[0];
        TCode += (char)data[1];
        const EffectDesc* effectInfo = effects.getEffectInfo(TCode);

        if (effectInfo) {

            DynamicJsonDocument j(1024);
            DeserializationError error = deserializeJson(j, reinterpret_cast<char*>(data + 2));

            // weird ... no error handling on json parsing

            JsonObject json = j.as<JsonObject>();

            config.ds = DataSource::WEB;
            effects.setEffect( effectInfo->name );

            if ( effectInfo->hasColor ) {
                if (json.containsKey("r") && json.containsKey("g") && json.containsKey("b")) {
                    effects.setColor({json["r"], json["g"], json["b"]});
                }
            }
            if ( effectInfo->hasMirror ) {
                if (json.containsKey("mirror")) {
                    effects.setMirror(json["mirror"]);
                }
            }
            if ( effectInfo->hasReverse ) {
                if (json.containsKey("reverse")) {
                    effects.setReverse(json["reverse"]);
                }
            }
            if ( effectInfo->hasAllLeds ) {
                if (json.containsKey("allleds")) {
                    effects.setAllLeds(json["allleds"]);
                }
            }
            if (json.containsKey("speed")) {
                effects.setSpeed(json["speed"]);
            }
            if (json.containsKey("brightness")) {
                effects.setBrightness(json["brightness"]);
            }
        }
    }

    if (config.mqtt)
        publishState();
}

void procV(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1': {  // View stream
#if defined(ESPS_MODE_PIXEL)
            client->binary(pixels.getData(), config.channel_count);
#elif defined(ESPS_MODE_SERIAL)
            if (config.serial_type == SerialType::DMX512)
                client->binary(&serial.getData()[1], config.channel_count);
            else
                client->binary(&serial.getData()[2], config.channel_count);
#endif
            break;
        }
    }
}

void handle_fw_upload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Upload Started: "));
        LOG_PORT.println(filename.c_str());
        efupdate.begin();
    }

    if (!efupdate.process(data, len)) {
        LOG_PORT.print(F("*** UPDATE ERROR: "));
        LOG_PORT.println(String(efupdate.getError()));
    }

    if (efupdate.hasError())
        request->send(200, "text/plain", "Update Error: " +
                String(efupdate.getError()));

    if (final) {
        request->send(200, "text/plain", "Update Finished: " +
                String(efupdate.getError()));
        LOG_PORT.println(F("* Upload Finished."));
        efupdate.end();
        SPIFFS.begin();
        saveConfig();
        reboot = true;
    }
}

void handle_config_upload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    static File file;
    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Config Upload Started: "));
        LOG_PORT.println(filename.c_str());

        if (confuploadtemp) {
          free (confuploadtemp);
          confuploadtemp = nullptr;
        }
        confuploadtemp = (uint8_t*) malloc(CONFIG_MAX_SIZE);
    }

    LOG_PORT.printf("index %d len %d\n", index, len);
    memcpy(confuploadtemp + index, data, len);
    confuploadtemp[index + len] = 0;

    if (final) {
        int filesize = index+len;
        LOG_PORT.print(F("* Config Upload Finished:"));
        LOG_PORT.printf(" %d bytes", filesize);

        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, reinterpret_cast<char*>(confuploadtemp));

        if (error) {
            LOG_PORT.println(F("*** Parse Error ***"));
            LOG_PORT.println(reinterpret_cast<char*>(confuploadtemp));
            request->send(500, "text/plain", "Config Update Error." );
        } else {
            dsNetworkConfig(json.as<JsonObject>());
            dsDeviceConfig(json.as<JsonObject>());
            dsEffectConfig(json.as<JsonObject>());
            saveConfig();
            request->send(200, "text/plain", "Config Update Finished: " );
//          reboot = true;
        }

        if (confuploadtemp) {
            free (confuploadtemp);
            confuploadtemp = nullptr;
        }
    }
}

void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
        AwsEventType type, void * arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_DATA: {
            AwsFrameInfo *info = static_cast<AwsFrameInfo*>(arg);
            if (info->opcode == WS_TEXT) {
                switch (data[0]) {
                    case 'X':
                        procX(data, client);
                        break;
                    case 'E':
                        procE(data, client);
                        break;
                    case 'G':
                        procG(data, client);
                        break;
                    case 'S':
                        procS(data, client);
                        break;
                    case 'T':
                        procT(data, client);
                        break;
                    case 'V':
                        procV(data, client);
                        break;
                }
            } else {
                LOG_PORT.println(F("-- binary message --"));
            }
            break;
        }
        case WS_EVT_CONNECT:
            LOG_PORT.print(F("* WS Connect - "));
            LOG_PORT.println(client->id());
            break;
        case WS_EVT_DISCONNECT:
            LOG_PORT.print(F("* WS Disconnect - "));
            LOG_PORT.println(client->id());
            break;
        case WS_EVT_PONG:
            LOG_PORT.println(F("* WS PONG *"));
            break;
        case WS_EVT_ERROR:
            LOG_PORT.println(F("** WS ERROR **"));
            break;
    }
}

#endif /* ESPIXELSTICK_H_ */
