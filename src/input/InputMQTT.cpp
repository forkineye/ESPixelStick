/*
* InputMQTT.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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

#include <ArduinoJson.h>
#include "../ESPixelStick.h"
#include <Ticker.h>
#include <Int64String.h>
#include "InputMQTT.h"
#include "InputEffectEngine.hpp"
#include "../FileIO.h"

#if defined ARDUINO_ARCH_ESP32
#   include <functional>
#endif

//-----------------------------------------------------------------------------
c_InputMQTT::c_InputMQTT (
    c_InputMgr::e_InputChannelIds NewInputChannelId,
    c_InputMgr::e_InputType       NewChannelType,
    uint8_t                     * BufferStart,
    uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)

{
    // DEBUG_START;

    pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputChannelId_1, c_InputMgr::e_InputType::InputType_Effects, InputDataBuffer, InputDataBufferSize);
    pEffectsEngine->SetOperationalState (false);

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputMQTT::~c_InputMQTT ()
{
    mqtt.unsubscribe (topic.c_str ());
    mqtt.disconnect (/*force = */ true);
    mqttTicker.detach ();

    // allow the other input channels to run
    InputMgr.SetOperationalState (true);

    if (nullptr != pEffectsEngine)
    {
        delete pEffectsEngine;
        pEffectsEngine = nullptr;
    }
} // ~c_InputMQTT

//-----------------------------------------------------------------------------
void c_InputMQTT::Begin() 
{
    // DEBUG_START;

    Serial.println (String (F ("** MQTT Initialization for channel '")) + InputChannelId + String (F ("' **")));

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }
    HasBeenInitialized = true;

    pEffectsEngine->Begin ();

    RegisterWithMqtt ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputMQTT::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // Serialize Config
    jsonConfig["ip"]       = ip;
    jsonConfig["port"]     = port;
    jsonConfig["user"]     = user;
    jsonConfig["password"] = password;
    jsonConfig["topic"]    = topic;
    jsonConfig["clean"]    = clean;
    jsonConfig["hadisco"]  = hadisco;
    jsonConfig["haprefix"] = haprefix;

    pEffectsEngine->GetConfig (jsonConfig);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputMQTT::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject mqttStatus = jsonStatus.createNestedObject (F ("mqtt"));
    // mqttStatus["unifirst"] = startUniverse;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputMQTT::Process ()
{
    // DEBUG_START;
    // ignoring IsInputChannelActive
    
    pEffectsEngine->Process ();

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputMQTT::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

    pEffectsEngine->SetBufferInfo (BufferStart, BufferSize);

} // SetBufferInfo

//-----------------------------------------------------------------------------
boolean c_InputMQTT::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    disconnectFromMqtt ();

    FileIO::setFromJSON (ip,       jsonConfig["ip"]);
    FileIO::setFromJSON (port,     jsonConfig["port"]);
    FileIO::setFromJSON (user,     jsonConfig["user"]);
    FileIO::setFromJSON (password, jsonConfig["password"]);
    FileIO::setFromJSON (topic,    jsonConfig["topic"]);
    FileIO::setFromJSON (clean,    jsonConfig["clean"]);
    FileIO::setFromJSON (hadisco,  jsonConfig["hadisco"]);
    FileIO::setFromJSON (haprefix, jsonConfig["haprefix"]);

    pEffectsEngine->SetConfig (jsonConfig);

    validateConfiguration ();
    RegisterWithMqtt ();
    connectToMqtt ();

    // DEBUG_END;
    return true;
} // SetConfig

//-----------------------------------------------------------------------------
//TODO: Add MQTT configuration validation
void c_InputMQTT::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate

/////////////////////////////////////////////////////////
//
//  MQTT Section
//
/////////////////////////////////////////////////////////

void c_InputMQTT::RegisterWithMqtt ()
{
    using namespace std::placeholders;
    mqtt.onConnect (std::bind (&c_InputMQTT::onMqttConnect, this, _1));
    mqtt.onDisconnect (std::bind (&c_InputMQTT::onMqttDisconnect, this, _1));
    mqtt.onMessage (std::bind (&c_InputMQTT::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
    mqtt.setServer (ip.c_str (), port);
    
    // Unset clean session (defaults to true) so we get retained messages of QoS > 0
    mqtt.setCleanSession (clean);
    if (user.length () > 0)
    {
        mqtt.setCredentials (user.c_str (), password.c_str ());
    }

} // RegisterWithMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::onConnect()
{
    // DEBUG_START;

    connectToMqtt();
    
    // DEBUG_END;

} // onConnect

//-----------------------------------------------------------------------------
void c_InputMQTT::onDisconnect()
{
    // DEBUG_START;

    mqttTicker.detach();

    // DEBUG_END;

} // onDisconnect

//-----------------------------------------------------------------------------
void c_InputMQTT::update()
{
    // DEBUG_START;

    // Update Home Assistant Discovery if enabled
    publishHA (hadisco);
    publishState ();

    // DEBUG_END;

} // update

//-----------------------------------------------------------------------------
void c_InputMQTT::connectToMqtt()
{
    // DEBUG_START;

    LOG_PORT.print(F("- Connecting to MQTT Broker "));
    LOG_PORT.println(ip);
    mqtt.connect();

    // DEBUG_END;
} // connectToMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::disconnectFromMqtt ()
{
    // DEBUG_START;

    LOG_PORT.println (F ("- Disconnecting from MQTT Broker "));
    mqtt.disconnect ();

    // DEBUG_END;
} // disconnectFromMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::onMqttConnect(bool sessionPresent)
{
    // DEBUG_START;

    LOG_PORT.println(F("- MQTT Connected"));

    // Get retained MQTT state
    mqtt.subscribe(topic.c_str(), 0);
    mqtt.unsubscribe(topic.c_str());

    // Setup subscriptions
    mqtt.subscribe(String(topic + SET_COMMAND_TOPIC).c_str(), 0);

    // Publish state
    publishState();

    // DEBUG_END;

} // onMqttConnect

//-----------------------------------------------------------------------------
// void c_InputMQTT::onMqttDisconnect (AsyncMqttClientDisconnectReason reason) {
void c_InputMQTT::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) 
{
    // DEBUG_START;

    LOG_PORT.println(F("- MQTT Disconnected"));
    if (WiFi.isConnected ())
    {
        // set up a two second delayed action.
        mqttTicker.once (2, +[](c_InputMQTT* pMe) { pMe->connectToMqtt (); }, this);
    }
} // onMqttDisconnect

//-----------------------------------------------------------------------------
void c_InputMQTT::onMqttMessage(
    char* topic, 
    char* payload,
    AsyncMqttClientMessageProperties properties, 
    size_t len, 
    size_t index, 
    size_t total) 
{
    // DEBUG_START;
    do // once
    {
        // DEBUG_V (String("payload: ") + String(payload));

        if ('{' != payload[0])
        {
            // not a json payload.
            break;
        }

        DynamicJsonDocument r (1024);
        DeserializationError error = deserializeJson (r, payload, len);

        // DEBUG_V ("");
        if (error) 
        {
            LOG_PORT.println (String (F ("MQTT: Deserialzation Error. Error code = ")) + error.c_str ());
            break;
        }

        JsonObject root = r.as<JsonObject> ();
        // DEBUG_V ("");

        // if its a retained message and we want clean session, ignore it
        if (properties.retain && clean) 
        {
            // DEBUG_V ("");
            break;
        }

        // DEBUG_V ("");

        if (root.containsKey ("state")) 
        {
            // DEBUG_V ("");
            if (strcmp (root["state"], ON) == 0)
            {
                // DEBUG_V ("");
                stateOn = true;
                // blank the other input channels
                InputMgr.SetOperationalState (false);
                // DEBUG_V ("");
                pEffectsEngine->SetOperationalState (true);

                // DEBUG_V ("");
            }
            else if (strcmp (root["state"], OFF) == 0) 
            {
                // DEBUG_V ("");
                stateOn = false;

                // allow the other input channels to run
                InputMgr.SetOperationalState (true);
                // DEBUG_V ("");
                pEffectsEngine->SetOperationalState (false);
                // DEBUG_V ("");
            }
        }
        // DEBUG_V ("");

        ((c_InputEffectEngine*)(pEffectsEngine))->SetMqttConfig (root);
        // DEBUG_V ("");

        publishState ();
        // DEBUG_V ("");
    } while (false);

    // DEBUG_END;

} // onMqttMessage

//-----------------------------------------------------------------------------
void c_InputMQTT::publishHA(bool join)
{
    // DEBUG_START;

    // Setup HA discovery
#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = int64String (ESP.getEfuseMac (), HEX);
#endif
    String ha_config = haprefix + "/light/" + chipId + "/config";

    if (join) 
    {
        DynamicJsonDocument root(1024);
        JsonObject JsonConfig = root.as<JsonObject> ();

//TODO: Fix - how to reference config.id from here? Pass in pointer to cfg struct from main?
        JsonConfig["name"]          = "MartinFixMe";
        JsonConfig["schema"]        = "json";
        JsonConfig["state_topic"]   = topic;
        JsonConfig["command_topic"] = topic + "/set";
        JsonConfig["rgb"]           = "true";
        JsonConfig["brightness"]    = "true";

        pEffectsEngine->GetConfig (JsonConfig);

        String HaJsonConfig;
        serializeJson(root, HaJsonConfig);
        mqtt.publish(ha_config.c_str(), 0, true, HaJsonConfig.c_str());
    } 
    else 
    {
        mqtt.publish(ha_config.c_str(), 0, true, "");
    }
    // DEBUG_END;
} // publishHA

//-----------------------------------------------------------------------------
void c_InputMQTT::publishState()
{
    // DEBUG_START;
    
    DynamicJsonDocument root(1024);
    JsonObject JsonConfig = root.createNestedObject("MQTT");

    JsonConfig["state"] = (true == stateOn) ? String(ON) : String(OFF);

    // populate the effect information
    ((c_InputEffectEngine*)(pEffectsEngine))->GetMqttConfig (JsonConfig);

    String JsonConfigString;
    serializeJson(JsonConfig, JsonConfigString);
    // DEBUG_V (String ("JsonConfigString: ") + JsonConfigString);
    // DEBUG_V (String ("topic: ") + topic);

    mqtt.publish(topic.c_str(), 0, true, JsonConfigString.c_str());

    // DEBUG_END;

} // publishState
