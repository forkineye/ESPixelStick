/*
* WebIO.h - Static methods for common file web service routines
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#ifndef _WEBIO_H_
#define _WEBIO_H_

#include <Arduino.h>
#include "ESPixelStick.h"
#include "EFUpdate.h"
#include "input/_Input.h"
#include "output/_Output.h"

extern bool     reboot;     ///< Reboot flag
extern _Input   *input;     ///< Pointer to currently enabled input module
extern _Output  *output;    ///< Pointer to currently enabled output module

extern const std::map<const String, _Input*>    INPUT_MODES;        ///< Input Modes
extern const std::map<const String, _Output*>   OUTPUT_MODES;       ///< Output Modes
extern std::map<const String, _Input*>::const_iterator itInput;     ///< Input iterator
extern std::map<const String, _Output*>::const_iterator itOutput;   ///< Output iterator


/// Contains static methods utilized for processing and working with Web Services
/** Two web service text message formats are supported.
 * - Simple Messages: Start with the character 'X'
 * - JSON Messages: All other messages are processed as JSON
 */
class WebIO {
  private:
    /// Process simple format 'X' messages
    static void procSimple(uint8_t *data, AsyncWebSocketClient *client) {
        switch (data[1]) {
            case SimpleMessage::GET_STATUS:
                DynamicJsonDocument json(1024);

                // system statistics
                JsonObject system = json.createNestedObject("system");
                system["rssi"] = (String)WiFi.RSSI();
                system["freeheap"] = (String)ESP.getFreeHeap();
                system["uptime"] = (String)millis();

        /*
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
        */
                String response;
                serializeJson(json, response);
                client->text("XJ" + response);
                break;
        }
    }

    /// Process JSON messages
    static void procJSON(uint8_t *data, AsyncWebSocketClient *client) {
        DynamicJsonDocument json(1024);

        DeserializationError error = deserializeJson(json, reinterpret_cast<char*>(data));
        if (error) {
            LOG_PORT.println(F("*** WebIO::procJSON(): Parse Error ***"));
            LOG_PORT.println(reinterpret_cast<char*>(data));
            return;
        }

        /** Following commands are supported:
         * - get: returns requested configuration
         * - set: receive and applies configuration
         * - opt: returns select option lists
         * - ele: returns raw element data to build elements
         */
        if (json.containsKey("cmd")) {
            String target;

            // Process "GET" command - return requested configuration as JSON
            if (target = json["cmd"]["get"].as<String>()) {
                if (target.equalsIgnoreCase("core")) {
                    String jsonString;
                    serializeCore(jsonString, false, true);
                    client->text("{\"get\":" + jsonString + "}");
                }
                if (target.equalsIgnoreCase("input"))
                    client->text("{\"get\":" + input->serialize() + "}");
                if (target.equalsIgnoreCase("output"))
                    client->text("{\"get\":" + output->serialize() + "}");
            }

            // Process "SET" command - receive configuration as JSON
            if (target = json["cmd"]["set"].as<String>()) {
                if (target.equalsIgnoreCase("core")) {
                    LOG_PORT.println("*** WebIO set config core***");
                }
                if (target.equalsIgnoreCase("input"))
                    LOG_PORT.println("*** WebIO set config input ***");
                if (target.equalsIgnoreCase("output"))
                    LOG_PORT.println("*** WebIO set config output ***");
            }

            // Generate select option list data
            if (target = json["cmd"]["opt"].as<String>()) {
                if (target.equalsIgnoreCase("core")) {
                    DynamicJsonDocument json(1024);
                    JsonObject core = json.createNestedObject("device");
                    JsonArray input = core.createNestedArray("input");
                    JsonArray output = core.createNestedArray("output");

                    itInput = INPUT_MODES.begin();
                    while (itInput != INPUT_MODES.end()) {
                        JsonObject data = input.createNestedObject();
                        data[itInput->first.c_str()] =  itInput->second->getBrief();
                        itInput++;
                    }

                    itOutput = OUTPUT_MODES.begin();
                    while (itOutput != OUTPUT_MODES.end()) {
                        JsonObject data = output.createNestedObject();
                        data[itOutput->first.c_str()] =  itOutput->second->getBrief();
                        itOutput++;
                    }

                    String jsonString;
                    serializeJson(json, jsonString);
                    client->text("{\"opt\":" + jsonString + "}");
                }
                if (target.equalsIgnoreCase("input"))
                    LOG_PORT.println("*** WebIO opt input ***");
                if (target.equalsIgnoreCase("output"))
                    LOG_PORT.println("*** WebIO opt output ***");
            }


            // Process "ELE" command - return requested element data as JSON
            if (target = json["cmd"]["ele"].as<String>()) {
                if (target.equalsIgnoreCase("core")) {
                    DynamicJsonDocument json(1024);
                    JsonObject core = json.createNestedObject("core");
                    JsonArray input = core.createNestedArray("input");
                    JsonArray output = core.createNestedArray("output");

                    itInput = INPUT_MODES.begin();
                    while (itInput != INPUT_MODES.end()) {
                        input.add(itInput->first);
                        itInput++;
                    }

                    itOutput = OUTPUT_MODES.begin();
                    while (itOutput != OUTPUT_MODES.end()) {
                        output.add(itOutput->first);
                        itOutput++;
                    }

                    String jsonString;
                    serializeJson(json, jsonString);
                    client->text("{\"ele\":" + jsonString + "}");
                }
                if (target.equalsIgnoreCase("input"))
                    LOG_PORT.println("*** WebIO ele input ***");
                if (target.equalsIgnoreCase("output"))
                    LOG_PORT.println("*** WebIO ele output ***");
            }

        }



/* From wshandler:
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
            case '4':   // Set Gamma (but no save)
                dsGammaConfig(json.as<JsonObject>());
                client->text("S4");
                break;
        }
*/
    }

  public:
    /// Valid "Simple" message types
    enum SimpleMessage {
        GET_STATUS = 'J'
    };

    /// Handle Web Service events
    /** Handles all Web Service event types and performs initial parsing of Web Service event data.
     * Text messages that start with 'X' are treated as "Simple" format messages, else they're parsed as JSON.
    */
    static void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
            AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_DATA: {
                AwsFrameInfo *info = static_cast<AwsFrameInfo*>(arg);
                if (info->opcode == WS_TEXT) {
                    if (data[0] == 'X') {
                        procSimple(data, client);
                    } else {
                        procJSON(data, client);
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

    static void onFirmwareUpload(AsyncWebServerRequest *request, String filename,
            size_t index, uint8_t *data, size_t len, bool final) {
        static EFUpdate efupdate; /// EFU Update Handler
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
    //        saveConfig();
            reboot = true;
        }
    }

};

#endif /* _WEBIO_H_ */