/*
* MQTT.cpp
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
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

#include <FS.h>
#include <ArduinoJson.h>
#include "MQTT.h"
#include "../ESPixelStick.h"
#include <Ticker.h>

#if defined ARDUINO_ARCH_ESP32
#   include <SPIFFS.h>
#   include <functional>
#endif

void MQTTService::setup() {
    if (enabled) {
        using namespace std::placeholders;
        mqtt.onConnect(std::bind(&MQTTService::onMqttConnect, this, _1));
        mqtt.onDisconnect(std::bind(&MQTTService::onMqttDisconnect, this, _1));
        mqtt.onMessage(std::bind(&MQTTService::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        mqtt.setServer(ip.c_str(), port);
        // Unset clean session (defaults to true) so we get retained messages of QoS > 0
        mqtt.setCleanSession(clean);
    if (user.length() > 0)
        mqtt.setCredentials(user.c_str(), password.c_str());
    }

}

/** Loads MQTT configuration file from SPIFFS. If configuration file is not found, a default one will be created and saved. */
void MQTTService::load() {
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("* MQTT configuration file found."));

        // Generate default topic
#ifdef ARDUINO_ARCH_ESP8266
        String chipId = String (ESP.getChipId (), HEX);
#else
        String chipId = String ((unsigned long)ESP.getEfuseMac (), HEX);
#endif
        topic = "diy/esps/" + chipId;

        // Save configuration
        save();
    } else {
        // Parse CONFIG_FILE json
        size_t size = file.size();
        if (size > CONFIG_MAX_SIZE) {
            LOG_PORT.println(F("*** Configuration File too large ***"));
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);

        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        if (error) {
            LOG_PORT.println(F("*** Configuration File Format Error ***"));
            LOG_PORT.println (String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            return;
        }

        if (json.containsKey("mqtt")) {
            JsonObject mqttJson = json["mqtt"];
            enabled = mqttJson["enabled"];
            ip = mqttJson["ip"].as<String>();
            port = mqttJson["port"];
            user = mqttJson["user"].as<String>();
            password = mqttJson["password"].as<String>();
            topic = mqttJson["topic"].as<String>();
            clean = mqttJson["clean"] | false;
            hadisco = mqttJson["hadisco"] | false;
            haprefix = mqttJson["haprefix"].as<String>();
            LOG_PORT.println(F("- Configuration loaded."));
        } else {
            LOG_PORT.println("* No MQTT settings found.");
        }
    }
}

//TODO: Add MQTT configuration validation
void MQTTService::validate() {

}

/** Save MQTT configuration file to SPIFFS */
void MQTTService::save() {
    // Update Config
    //updateConfig();

    // Serialize Config
    DynamicJsonDocument json(1024);
    JsonObject _mqtt = json.createNestedObject(F("mqtt"));
    _mqtt["enabled"] = enabled;
    _mqtt["ip"] = ip.c_str();
    _mqtt["port"] = port;
    _mqtt["user"] = user.c_str();
    _mqtt["password"] = password.c_str();
    _mqtt["topic"] = topic.c_str();
    _mqtt["clean"] = clean;
    _mqtt["hadisco"] = hadisco;
    _mqtt["haprefix"] = haprefix.c_str();
    String jsonString;
    serializeJsonPretty(json, jsonString);

    // Save Config
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

void MQTTService::onConnect() {
    if (enabled)
        connectToMqtt();

}

void MQTTService::onDisconnect() {
    mqttTicker.detach();
}

void MQTTService::update() {
    // Update Home Assistant Discovery if enabled
    if (enabled) {
        publishHA(hadisco);
        publishState();
    }
}

/////////////////////////////////////////////////////////
//
//  MQTT Section
//
/////////////////////////////////////////////////////////

void MQTTService::connectToMqtt() {
    LOG_PORT.print(F("- Connecting to MQTT Broker "));
    LOG_PORT.println(ip);
    mqtt.connect();
}

void MQTTService::onMqttConnect(bool sessionPresent) {
    LOG_PORT.println(F("- MQTT Connected"));

    // Get retained MQTT state
    mqtt.subscribe(topic.c_str(), 0);
    mqtt.unsubscribe(topic.c_str());

    // Setup subscriptions
    mqtt.subscribe(String(topic + SET_COMMAND_TOPIC).c_str(), 0);

    // Publish state
    publishState();
}

// void MQTTService::onMqttDisconnect (AsyncMqttClientDisconnectReason reason) {
void MQTTService::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    LOG_PORT.println(F("- MQTT Disconnected"));
    if (WiFi.isConnected ())
    {
        // set up a two second delayed action.
        mqttTicker.once (2, +[](MQTTService* pMe) { pMe->connectToMqtt (); }, this);
    }
}

void MQTTService::onMqttMessage(char* topic, char* payload,
        AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

    DynamicJsonDocument r(1024);
    DeserializationError error = deserializeJson(r, payload);

    if (error) {
        LOG_PORT.println (String (F ("MQTT: Deserialzation Error. Error code = ")) + error.c_str ());
        return;
    }

    JsonObject root = r.as<JsonObject>();

    // if its a retained message and we want clean session, ignore it
    if ( properties.retain && clean ) {
        return;
    }

    bool stateOn = false;

    if (root.containsKey("state")) {
        if (strcmp(root["state"], ON) == 0) {
            stateOn = true;
        } else if (strcmp(root["state"], OFF) == 0) {
            stateOn = false;
        }
    }
/*
    if (root.containsKey("brightness")) {
        effects.setBrightness((float)root["brightness"] / 255.0);
    }

    if (root.containsKey("speed")) {
        effects.setSpeed(root["speed"]);
    }

    if (root.containsKey("color")) {
        effects.setColor({
            root["color"]["r"],
            root["color"]["g"],
            root["color"]["b"]
        });
    }

    if (root.containsKey("effect")) {
        // Set the explict effect provided by the MQTT client
        effects.setEffect(root["effect"]);
    }

    if (root.containsKey("reverse")) {
        effects.setReverse(root["reverse"]);
    }

    if (root.containsKey("mirror")) {
        effects.setMirror(root["mirror"]);
    }

    if (root.containsKey("allleds")) {
        effects.setAllLeds(root["allleds"]);
    }

    // Set data source based on state - Fall back to E131 when off
    if (stateOn) {
        if (effects.getEffect().equalsIgnoreCase("Disabled"))
            effects.setEffect("Solid");
        config.ds = DataSource::MQTT;
    } else {
        config.ds = DataSource::E131;
        effects.clearAll();
    }
*/
    publishState();
}

void MQTTService::publishHA(bool join) {
    // Setup HA discovery
#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = String ((unsigned long)ESP.getEfuseMac (), HEX);
#endif
    String ha_config = haprefix + "/light/" + chipId + "/config";

    if (join) {
        DynamicJsonDocument root(1024);
//TODO: Fix - how to reference config.id from here? Pass in pointer to cfg struct from main?
//        root["name"] = config.id;
        root["schema"] = "json";
        root["state_topic"] = topic;
        root["command_topic"] = topic + "/set";
        root["rgb"] = "true";
        root["brightness"] = "true";
/*
        root["effect"] = "true";
        JsonArray effect_list = root.createNestedArray("effect_list");
        // effect[0] is 'disabled', skip it
        for (uint8_t i = 1; i < effects.getEffectCount(); i++) {
            effect_list.add(effects.getEffectInfo(i)->name);
        }
*/
        char buffer[measureJson(root) + 1];
        serializeJson(root, buffer, sizeof(buffer));
        mqtt.publish(ha_config.c_str(), 0, true, buffer);
    } else {
        mqtt.publish(ha_config.c_str(), 0, true, "");
    }
}

void MQTTService::publishState() {
/*
    DynamicJsonDocument root(1024);
    if ((config.ds != DataSource::E131 && config.ds != DataSource::ZCPP) && (!effects.getEffect().equalsIgnoreCase("Disabled")))
        root["state"] = ON;
    else
        root["state"] = OFF;

    JsonObject color = root.createNestedObject(F("color"));
    color["r"] = effects.getColor().r;
    color["g"] = effects.getColor().g;
    color["b"] = effects.getColor().b;
    root["brightness"] = effects.getBrightness()*255;
    root["speed"] = effects.getSpeed();
    if (!effects.getEffect().equalsIgnoreCase("Disabled")) {
        root["effect"] = effects.getEffect();
    }
    root["reverse"] = effects.getReverse();
    root["mirror"] = effects.getMirror();
    root["allleds"] = effects.getAllLeds();

    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    mqtt.publish(config.mqtt_topic.c_str(), 0, true, buffer);
*/
}
