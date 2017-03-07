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

#ifndef WSHANDLER_H_
#define WSHANDLER_H_

/* 
  Packet Commands
    E1 - Get Elements

    G1 - Get Config
    G2 - Get Config Status
    
    T0 - Disable Testing
    T1 - Static Testing

    S1 - Set Network Config
    S2 - Set Device Config

    X1 - Get RSSI
    X2 - Get E131 Status
    X6 - Reboot
*/

EFUpdate efupdate;

void procX(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1':
            client->text("X1" + (String)WiFi.RSSI());
            break;
        case '2': {
            uint32_t seqErrors = 0;
            for (int i = 0; i < ((uniLast + 1) - config.universe); i++)
                seqErrors =+ seqError[i];

            client->text("X2" + (String)config.universe + ":" +
                    (String)uniLast + ":" +
                    (String)e131.stats.num_packets + ":" +
                    (String)seqErrors + ":" +
                    (String)e131.stats.packet_errors);
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
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.createObject();

#if defined (ESPS_MODE_PIXEL)
            // Pixel Types
            JsonObject &p_type = json.createNestedObject("p_type");
            p_type["WS2811 800kHz"] = static_cast<uint8_t>(PixelType::WS2811);
            p_type["GE Color Effects"] = static_cast<uint8_t>(PixelType::GECE);

            // Pixel Colors
            JsonObject &p_color = json.createNestedObject("p_color");
            p_color["RGB"] = static_cast<uint8_t>(PixelColor::RGB);
            p_color["GRB"] = static_cast<uint8_t>(PixelColor::GRB);
            p_color["BRG"] = static_cast<uint8_t>(PixelColor::BRG);
            p_color["RBG"] = static_cast<uint8_t>(PixelColor::RBG);
            p_color["GBR"] = static_cast<uint8_t>(PixelColor::GBR);
            p_color["BGR"] = static_cast<uint8_t>(PixelColor::BGR);

#elif defined (ESPS_MODE_SERIAL)
            // Serial Protocols
            JsonObject &s_proto = json.createNestedObject("s_proto");
            s_proto["DMX512"] = static_cast<uint8_t>(SerialType::DMX512);
            s_proto["Renard"] = static_cast<uint8_t>(SerialType::RENARD);

            // Serial Baudrates
            JsonObject &s_baud = json.createNestedObject("s_baud");
            s_baud["38400"] = static_cast<uint32_t>(BaudRate::BR_38400);
            s_baud["57600"] = static_cast<uint32_t>(BaudRate::BR_57600);
            s_baud["115200"] = static_cast<uint32_t>(BaudRate::BR_115200);
            s_baud["230400"] = static_cast<uint32_t>(BaudRate::BR_230400);
            s_baud["250000"] = static_cast<uint32_t>(BaudRate::BR_250000);
            s_baud["460800"] = static_cast<uint32_t>(BaudRate::BR_460800);
#endif

            String response;
            json.printTo(response);
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
        case '2':
            // Create buffer and root object
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.createObject();

            json["ssid"] = (String)WiFi.SSID();
            json["ip"] = WiFi.localIP().toString();
            json["mac"] = getMacAddress();
            json["version"] = (String)VERSION;
            json["testing"] = static_cast<uint8_t>(config.testmode);

            String response;
            json.printTo(response);
            client->text("G2" + response);
            break;
    }
}

void procS(uint8_t *data, AsyncWebSocketClient *client) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(reinterpret_cast<char*>(data + 2));
    if (!json.success()) {
        LOG_PORT.println(F("*** procS(): Parse Error ***"));
        LOG_PORT.println(reinterpret_cast<char*>(data));
        return;
    }

    switch (data[1]) {
        case '1':   // Set Network Config
            dsNetworkConfig(json);
            saveConfig();
            client->text("S1");
            break;
        case '2':   // Set Device Config
            dsDeviceConfig(json);
            saveConfig();
            client->text("S2");
            break;
    }
}

/*
enum class TestMode : uint8_t {
    DISABLED,
    STATIC,
    CHASE,
    RAINBOW,
    VIEW_STREAM
};
*/

void procT(uint8_t *data, AsyncWebSocketClient *client) {
    
    switch(data[1]) {
        case '0':
            config.testmode = TestMode::DISABLED;
            //clear whole string
#if defined(ESPS_MODE_PIXEL)
          for(int y =0; y < config.channel_count; y++) pixels.setValue(y, 0);
#elif defined(ESPS_MODE_SERIAL)
          for(int y =0; y < config.channel_count; y++) serial.setValue(y, 0);
#endif
            break;

        case '1': {//static color
            config.testmode = TestMode::STATIC;
            testing.step = 0;
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(reinterpret_cast<char*>(data + 2));

            testing.r = json["r"];
            testing.g = json["g"];
            testing.b = json["b"];
            break;
        }
        case '2': {//chase
            config.testmode = TestMode::CHASE;
            testing.step = 0;
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(reinterpret_cast<char*>(data + 2));

            testing.r = json["r"];
            testing.g = json["g"];
            testing.b = json["b"];
        
          break;
        }
        case '3': //rainbow
            config.testmode = TestMode::RAINBOW;
            testing.step = 0;
          break;
        case '4': {//view stream
            config.testmode = TestMode::VIEW_STREAM;
#if defined(ESPS_MODE_PIXEL)
            client->binary(pixels.getData(),config.channel_count);
#elif defined(ESPS_MODE_SERIAL)
            uint16_t qsize = config.channel_count;
            uint8_t* bufq = (uint8_t *)(malloc(qsize));
            uint8_t* bufi = serial.getData();
            if (bufq){
              uint16_t i = 0;
              if( config.serial_type == DMX512){
                while(i<config.channel_count){
                  bufq[i]=bufi[1+i++];
                }
              }
              else {
                while(i<config.channel_count+2){
                  bufq[i]=bufi[2+i++];
                }
                
              }
              client->binary(bufq,qsize);
              free(bufq);
            }
            else
              client->binary(serial.getData(),config.channel_count);
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
        LOG_PORT.println(F("* Upload Finished."));
        efupdate.end();
        SPIFFS.begin();
        saveConfig();
        reboot = true;
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
        case WS_EVT_ERROR:
            LOG_PORT.println(F("** WS ERROR **"));
            break;
    }
}

#endif /* ESPIXELSTICK_H_ */
