/*
* InputMQTT.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
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

#include "../ESPixelStick.h"
#include <Ticker.h>
#include <Int64String.h>
#include "InputMQTT.h"
#include "InputEffectEngine.hpp"
#include "InputFPPRemotePlayFile.hpp"
#include "InputFPPRemotePlayList.hpp"

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

    topic = config.hostname;

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputMQTT::~c_InputMQTT ()
{
    if (HasBeenInitialized)
    {
        mqtt.unsubscribe (topic.c_str ());
        mqtt.disconnect (/*force = */ true);
        mqttTicker.detach ();

        // allow the other input channels to run
        InputMgr.SetOperationalState (true);
    }

    if (nullptr != pEffectsEngine)
    {
        // DEBUG_V ("");
        delete pEffectsEngine;
        pEffectsEngine = nullptr;
    }
    // DEBUG_V ("");

    if (nullptr != pPlayFileEngine)
    {
        // DEBUG_V ("");
        delete pPlayFileEngine;
        pPlayFileEngine = nullptr;
    }

} // ~c_InputMQTT

//-----------------------------------------------------------------------------
void c_InputMQTT::Begin()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }
    HasBeenInitialized = true;

    RegisterWithMqtt ();

    // get things started
    NetworkStateChanged (InputMgr.GetNetworkState ());

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputMQTT::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // Serialize Config
    jsonConfig[CN_user]     = user;
    jsonConfig[CN_password] = password;
    jsonConfig[CN_topic]    = topic;
    jsonConfig[CN_clean]    = CleanSessionRequired;
    jsonConfig[CN_hadisco]  = hadisco;
    jsonConfig[CN_haprefix] = haprefix;
    jsonConfig[CN_lwt]      = lwt;
    jsonConfig[CN_effects]  = true;
    jsonConfig[CN_play]     = true;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputMQTT::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonObject Status = jsonStatus.createNestedObject (F ("mqtt"));
    Status[CN_id] = InputChannelId;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputMQTT::Process ()
{
    // DEBUG_START;

    if (IsInputChannelActive)
    {
        if (nullptr != pEffectsEngine)
        {
            // DEBUG_V ("");
            pEffectsEngine->Process ();
        }
    }

    if (nullptr != pPlayFileEngine)
    {
        pPlayFileEngine->Poll (InputDataBuffer, InputDataBufferSize);
    }

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputMQTT::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    // DEBUG_START;

    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

    if (nullptr != pEffectsEngine)
    {
        pEffectsEngine->SetBufferInfo (BufferStart, BufferSize);
    }

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputMQTT::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    disconnectFromMqtt ();
    String OldTopic = topic;
    setFromJSON (ip,                   jsonConfig, CN_ip);
    setFromJSON (port,                 jsonConfig, CN_port);
    setFromJSON (user,                 jsonConfig, CN_user);
    setFromJSON (password,             jsonConfig, CN_password);
    setFromJSON (topic,                jsonConfig, CN_topic);
    setFromJSON (CleanSessionRequired, jsonConfig, CN_clean);
    setFromJSON (hadisco,              jsonConfig, CN_hadisco);
    setFromJSON (haprefix,             jsonConfig, CN_haprefix);
    setFromJSON (lwt,                  jsonConfig, CN_lwt);

    validateConfiguration ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    if (OldTopic != topic)
    {
        mqtt.unsubscribe (OldTopic.c_str ());
        mqtt.unsubscribe ((OldTopic + CN_slashset).c_str ());
    }

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
    // DEBUG_START;

    using namespace std::placeholders;
    mqtt.onConnect    (std::bind (&c_InputMQTT::onMqttConnect,    this, _1));
    mqtt.onDisconnect (std::bind (&c_InputMQTT::onMqttDisconnect, this, _1));
    mqtt.onMessage    (std::bind (&c_InputMQTT::onMqttMessage,    this, _1, _2, _3, _4, _5, _6));

    // DEBUG_END;

} // RegisterWithMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::onNetworkConnect()
{
    // DEBUG_START;

    connectToMqtt();

    // DEBUG_END;

} // onConnect

//-----------------------------------------------------------------------------
void c_InputMQTT::onNetworkDisconnect()
{
    // DEBUG_START;

    mqttTicker.detach();
    disconnectFromMqtt ();

    // DEBUG_END;

} // onDisconnect

//-----------------------------------------------------------------------------
void c_InputMQTT::update()
{
    // DEBUG_START;

    // Update Home Assistant Discovery if enabled
    publishHA ();
    publishState ();

    // DEBUG_END;

} // update

//-----------------------------------------------------------------------------
void c_InputMQTT::connectToMqtt()
{
    // DEBUG_START;

    mqtt.setCleanSession (CleanSessionRequired);

    if (user.length () > 0)
    {
        // DEBUG_V (String ("User: ") + user);
        mqtt.setCredentials (user.c_str (), password.c_str ());
    }
    mqtt.setServer (ip.c_str (), port);

    logcon (String(F ("Connecting to broker ")) + ip + ":" + String(port));
    mqtt.connect ();
    mqtt.setWill (topic.c_str(), 1, true, lwt.c_str(), lwt.length());

    // DEBUG_END;

} // connectToMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::disconnectFromMqtt ()
{
    // DEBUG_START;

    logcon (F ("Disconnecting from broker"));
    mqtt.disconnect ();

    // DEBUG_END;
} // disconnectFromMqtt

//-----------------------------------------------------------------------------
void c_InputMQTT::onMqttConnect(bool sessionPresent)
{
    // DEBUG_START;

    logcon (F ("Connected"));

    // Get retained MQTT state
    mqtt.subscribe (topic.c_str (), 0);

    // Setup subscriptions
    mqtt.subscribe(String(topic + SET_COMMAND_TOPIC).c_str(), 0);

    mqtt.publish (topic.c_str(), 0, true, config.id.c_str());

    // Publish state
    update ();

    // DEBUG_END;

} // onMqttConnect

static const char DisconnectReason0[] = "TCP_DISCONNECTED";
static const char DisconnectReason1[] = "UNACCEPTABLE_PROTOCOL_VERSION";
static const char DisconnectReason2[] = "IDENTIFIER_REJECTED";
static const char DisconnectReason3[] = "SERVER_UNAVAILABLE";
static const char DisconnectReason4[] = "MALFORMED_CREDENTIALS";
static const char DisconnectReason5[] = "NOT_AUTHORIZED";
static const char DisconnectReason6[] = "NOT_ENOUGH_SPACE";
static const char DisconnectReason7[] = "TLS_BAD_FINGERPRINT";

static const char* DisconnectReasons[] =
{
    DisconnectReason0,
    DisconnectReason1,
    DisconnectReason2,
    DisconnectReason3,
    DisconnectReason4,
    DisconnectReason5,
    DisconnectReason6,
    DisconnectReason7
};

//-----------------------------------------------------------------------------
// void c_InputMQTT::onMqttDisconnect (AsyncMqttClientDisconnectReason reason) {
void c_InputMQTT::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    // DEBUG_START;

    logcon (String(F ("Disconnected: ")) + String(DisconnectReasons[uint8_t(reason)]));

    if (InputMgr.GetNetworkState ())
    {
        // DEBUG_V ("");
        // set up a two second delayed retry.
        mqttTicker.once (2, +[](c_InputMQTT* pMe)
            {
                // DEBUG_V ("");
                pMe->disconnectFromMqtt ();
                pMe->connectToMqtt ();
                // DEBUG_V ("");
            }, this);
    }
    // DEBUG_END;

} // onMqttDisconnect

//-----------------------------------------------------------------------------
void c_InputMQTT::onMqttMessage(
    char* RcvTopic,
    char* payload,
    AsyncMqttClientMessageProperties properties,
    size_t len,
    size_t index,
    size_t total)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V (String ("   topic: ") + String (topic));
        // DEBUG_V (String ("RcvTopic: ") + String (RcvTopic));
        // DEBUG_V (String (" payload: ") + String (payload));

        if (String (RcvTopic) != topic + CN_slashset)
        {
            // not a set for us
            break;
        }

        DynamicJsonDocument rootDoc (1024);
        DeserializationError error = deserializeJson (rootDoc, payload, len);

        // DEBUG_V ("Set new values");
        if (error)
        {
            logcon (String (F ("Deserialzation error. Error code = ")) + error.c_str ());
            break;
        }

        JsonObject root = rootDoc.as<JsonObject> ();
        // DEBUG_V ("");

        // if its a retained message and we want a clean session, ignore it
        if (properties.retain && CleanSessionRequired)
        {
            // DEBUG_V ("");
            break;
        }

        // DEBUG_V ("");
        String NewState;
        setFromJSON (NewState, root, CN_state);
        // DEBUG_V ("NewState: " + NewState);

        stateOn = NewState.equalsIgnoreCase(ON);
        InputMgr.SetOperationalState (!stateOn);
        // DEBUG_V (String("stateOn: ") + String(stateOn));

        if (stateOn)
        {
            // DEBUG_V ("State ON");
            String effectName;
            setFromJSON (effectName, root, CN_effect);
            // DEBUG_V ("effectName: " + effectName);

            if (effectName.equals(CN_playFseq))
            {
                // DEBUG_V ("");
                PlayFseq (root);
            }
            else
            {
                // DEBUG_V ("");
                PlayEffect (root);
            }
        }
        else
        {
            // DEBUG_V ("State OFF");

            if (nullptr != pEffectsEngine)
            {
                delete pEffectsEngine;
                pEffectsEngine = nullptr;
            }

            if (nullptr != pPlayFileEngine)
            {
                delete pPlayFileEngine;
                pPlayFileEngine = nullptr;
            }
            // DEBUG_V ("");
        }

        publishState ();

        // DEBUG_V ("");
    } while (false);

    // DEBUG_END;

} // onMqttMessage

//-----------------------------------------------------------------------------
void c_InputMQTT::PlayFseq (JsonObject & JsonConfig)
{
    // DEBUG_START;

    do // once
    {
        if (nullptr != pEffectsEngine)
        {
            // DEBUG_V ("Delete Effect Engine");
            delete pEffectsEngine;
            pEffectsEngine = nullptr;
        }
        // DEBUG_V ("");

        String FileName;
        setFromJSON (FileName, JsonConfig, CN_filename);

        uint32_t PlayCount = 1;
        setFromJSON (PlayCount, JsonConfig, CN_count);
        // DEBUG_V (String (" FileName: ") + FileName);
        // DEBUG_V (String ("PlayCount: ") + String(PlayCount));

        bool FileIsPlayList   = FileName.endsWith (String (F (".pl")));
        bool FileIsStandalone = FileName.endsWith (String (F (".fseq")));

        if (!FileIsPlayList && !FileIsStandalone)
        {
            // not a file we can process
            logcon (String (F ("ERROR: Unsupported file type for File Play operation. File:'")) + FileName + "'");
            break;
        }

        bool EngineFileIsPlayList   = false;
        // bool EngineFileIsStandalone = false;
        String PlayingFile;

        // do we have an engine running?
        if (nullptr != pPlayFileEngine)
        {
            PlayingFile = pPlayFileEngine->GetFileName ();

            EngineFileIsPlayList   = PlayingFile.endsWith (String (F (".pl")));
            // EngineFileIsStandalone = PlayingFile.endsWith (String (F (".fseq")));

            // is it the right engine?
            if (EngineFileIsPlayList != FileIsPlayList)
            {
                StopPlayFileEngine ();

                EngineFileIsPlayList   = false;
                // EngineFileIsStandalone = false;
            }
            else
            {
                // DEBUG_V ("Starting File");
                pPlayFileEngine->Start (FileName, 0, PlayCount);
                break;
            }
        }

        // DEBUG_V ("no engine is running.");

        if (FileIsPlayList)
        {
            // DEBUG_V ("Instantiate Play List Engine");
            pPlayFileEngine = new c_InputFPPRemotePlayList ();
        }
        else
        {
            // DEBUG_V ("Instantiate Play File Engine");
            pPlayFileEngine = new c_InputFPPRemotePlayFile ();
        }

        // DEBUG_V ("Start Playing");
        pPlayFileEngine->Start (FileName, 0, PlayCount);

    } while (false);

    // DEBUG_END;

} // PlayFseq

//-----------------------------------------------------------------------------
void c_InputMQTT::PlayEffect (JsonObject & JsonConfig)
{
    // DEBUG_START;

    StopPlayFileEngine ();

    // DEBUG_V ("");

    if (nullptr == pEffectsEngine)
    {
        // DEBUG_V ("Create Effect Engine");
        pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputChannelId_1, c_InputMgr::e_InputType::InputType_Effects, InputDataBuffer, InputDataBufferSize);
        pEffectsEngine->Begin ();
        pEffectsEngine->SetBufferInfo (InputDataBuffer, InputDataBufferSize);

        // DEBUG_V (String ("    InputDataBuffer: ") + String (uint32_t(InputDataBuffer)));
        // DEBUG_V (String ("InputDataBufferSize: ") + String (InputDataBufferSize));
    }
    // DEBUG_V ("");

    pEffectsEngine->SetOperationalState (true);
    // DEBUG_V ("");

    ((c_InputEffectEngine*)(pEffectsEngine))->SetMqttConfig (JsonConfig);

    // DEBUG_END;

} // PlayEffect

//-----------------------------------------------------------------------------
void c_InputMQTT::StopPlayFileEngine ()
{
    // DEBUG_START;

    if (nullptr != pPlayFileEngine)
    {
        delete pPlayFileEngine;
        pPlayFileEngine = nullptr;
    }

    // DEBUG_END;

} // StopPlayFileEngine

//-----------------------------------------------------------------------------
void c_InputMQTT::GetEngineConfig (JsonObject & JsonConfig)
{
    // DEBUG_START;

    bool EffectEngineIsRunning = (nullptr != pEffectsEngine);
    if (nullptr == pEffectsEngine)
    {
        // DEBUG_V ("");
        pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputChannelId_1, c_InputMgr::e_InputType::InputType_Effects, InputDataBuffer, InputDataBufferSize);
        pEffectsEngine->Begin ();
        pEffectsEngine->SetOperationalState (false);
    }
    // DEBUG_V ("");


    ((c_InputEffectEngine*)(pEffectsEngine))->GetMqttConfig (JsonConfig);

    if (!EffectEngineIsRunning)
    {
        delete pEffectsEngine;
        pEffectsEngine = nullptr;
    }

    if (nullptr != pPlayFileEngine)
    {
        JsonConfig[CN_effect] = CN_playFseq;
        JsonConfig[CN_filename] = pPlayFileEngine->GetFileName ();
    }
    else
    {
        JsonConfig[CN_filename] = String ("");
    }

    // DEBUG_END;

} // GetEngineConfig

//-----------------------------------------------------------------------------
void c_InputMQTT::GetEffectList (JsonObject & JsonConfig)
{
    // DEBUG_START;

    bool EffectEngineIsRunning = (nullptr != pEffectsEngine);
    if (nullptr == pEffectsEngine)
    {
        // DEBUG_V ("");
        pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputChannelId_1, c_InputMgr::e_InputType::InputType_Effects, InputDataBuffer, InputDataBufferSize);
        pEffectsEngine->Begin ();
        pEffectsEngine->SetOperationalState (false);
    }
    // DEBUG_V ("");

    JsonConfig[CN_brightness] = CN_true;
    ((c_InputEffectEngine*)(pEffectsEngine))->GetMqttEffectList (JsonConfig);
    JsonConfig[CN_effect] = CN_true;

    if (!EffectEngineIsRunning)
    {
        delete pEffectsEngine;
        pEffectsEngine = nullptr;
    }

    JsonConfig[CN_effect_list].add (CN_playFseq);

    // add the file play fields.
    // DEBUG_END;

} // GetEffectList

//-----------------------------------------------------------------------------
void c_InputMQTT::publishHA()
{
    // DEBUG_START;

    // Setup HA discovery
#ifdef ARDUINO_ARCH_ESP8266
    String chipId = String (ESP.getChipId (), HEX);
#else
    String chipId = int64String (ESP.getEfuseMac (), HEX);
#endif
    String ha_config = haprefix + F ("/light/") + chipId + F ("/config");

    // DEBUG_V (String ("ha_config: ") + ha_config);
    // DEBUG_V (String ("hadisco: ") + hadisco);

    if (hadisco)
    {
        // DEBUG_V ("");
        DynamicJsonDocument root(1024);
        JsonObject JsonConfig = root.to<JsonObject> ();

        JsonConfig[F ("platform")]      = F ("MQTT");
        JsonConfig[CN_name]             = config.hostname;
        JsonConfig[F ("schema")]        = F ("json");
        JsonConfig[F ("state_topic")]   = topic;
        JsonConfig[F ("command_topic")] = topic + CN_slashset;
        JsonConfig[F ("rgb")]           = CN_true;

        GetEffectList (JsonConfig);

        // Register the attributes topic
        JsonConfig[F ("json_attributes_topic")] = topic + F ("/attributes");

        // Create a unique id using the chip id, and fill in the device properties
        // to enable integration support in HomeAssistant.
        JsonConfig[F ("unique_id")] = CN_ESPixelStick + chipId;

        JsonObject device = JsonConfig.createNestedObject (CN_device);
        device[F ("identifiers")]  = WiFi.macAddress ();
        device[F ("manufacturer")] = CN_ESPixelStick;
        device[F ("model")]        = F ("Pixel Controller");
        device[CN_name]            = config.hostname;
        device[F ("sw_version")]   = String(CN_ESPixelStick) + " v" + VERSION;

        String HaJsonConfig;
        serializeJson(JsonConfig, HaJsonConfig);

        // DEBUG_V (String ("HaJsonConfig: ") + HaJsonConfig);
        mqtt.publish(ha_config.c_str(), 0, true, HaJsonConfig.c_str());

        // publishAttributes ();
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
    JsonObject JsonConfig = root.createNestedObject(F ("MQTT"));

    JsonConfig[CN_state] = (true == stateOn) ? String(ON) : String(OFF);

    // populate the effect information
    GetEngineConfig (JsonConfig);

    String JsonConfigString;
    serializeJson(JsonConfig, JsonConfigString);

    // DEBUG_V (String ("JsonConfigString: ") + JsonConfigString);
    // DEBUG_V (String ("topic: ") + topic);

    mqtt.publish(topic.c_str(), 0, true, JsonConfigString.c_str());

    // DEBUG_END;

} // publishState

//-----------------------------------------------------------------------------
void c_InputMQTT::NetworkStateChanged (bool IsConnected)
{
    NetworkStateChanged (IsConnected, false);
} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_InputMQTT::NetworkStateChanged (bool IsConnected, bool ReBootAllowed)
{
    // DEBUG_START;

    if (IsConnected)
    {
        onNetworkConnect ();
        // DEBUG_V ("");
    }
    else if (ReBootAllowed)
    {
        // handle a disconnect
        // E1.31 does not do this gracefully. A loss of connection needs a reboot
        extern bool reboot;
        reboot = true;
        logcon (F ("Requesting reboot on loss of network connection."));
    }
    else
    {
        onNetworkDisconnect ();
    }

    // DEBUG_END;

} // NetworkStateChanged
