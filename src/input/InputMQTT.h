#pragma once
/*
* InputMQTT.h
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

#include "InputCommon.hpp"
#include <AsyncMqttClient.h>

class c_InputMQTT : public c_InputCommon
{
  public:

      c_InputMQTT (
          c_InputMgr::e_InputChannelIds NewInputChannelId,
          c_InputMgr::e_InputType       NewChannelType,
          uint8_t                     * BufferStart,
          uint16_t                      BufferSize);
      
      ~c_InputMQTT ();

      // functions to be provided by the derived class
      void Begin ();                           ///< set up the operating environment based on the current config (or defaults)
      bool SetConfig (JsonObject& jsonConfig); ///< Set a new config in the driver
      void GetConfig (JsonObject& jsonConfig); ///< Get the current config used by the driver
      void GetStatus (JsonObject& jsonStatus);
      void Process ();                         ///< Call from loop(),  renders Input data
      void GetDriverName (String& sDriverName) { sDriverName = "MQTT"; } ///< get the name for the instantiated driver
      void SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize);

private:
#define MQTT_PORT       1883    ///< Default MQTT port

    AsyncMqttClient mqtt;           // MQTT object
    Ticker          mqttTicker;     // Ticker to handle MQTT
    c_InputCommon * pEffectsEngine = nullptr;

    // from original config struct
    String      ip;
    uint16_t    port = MQTT_PORT;
    String      user;
    String      password;
    String      topic;
    bool        clean = false;
    bool        hadisco = false;
    String      haprefix = "homeassistant";

    void validateConfiguration ();
    void RegisterWithMqtt ();

    void setup ();         ///< Call from setup()
    void onConnect ();     ///< Call from onWifiConnect()
    void onDisconnect ();  ///< Call from onWiFiDisconnect()
    void validate ();      ///< Call from validateConfig()
    void update ();        ///< Call from updateConfig()

    void load ();          ///< Load configuration from SPIFFS
    void save ();          ///< Save configuration to SPIFFS

    // const char* CONFIG_FILE = "/mqtt.json";
    const char* SET_COMMAND_TOPIC = "/set";
    const char* ON = "ON";
    const char* OFF = "OFF";
    bool stateOn = false;

    /////////////////////////////////////////////////////////
    //
    //  MQTT Section
    //
    /////////////////////////////////////////////////////////

    void disconnectFromMqtt ();
    void connectToMqtt (); // onMqttDisconnect, onWifiConnect
    void onMqttConnect (bool sessionPresent); // setup
    void onMqttDisconnect (AsyncMqttClientDisconnectReason reason); // setup
    void onMqttMessage (char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total); // setup
    void publishHA (); // updateConfig
    void publishState (); // onMqttConnect, onMqttMessage, procT, updateConfig
};
