/*
* MQTT.h
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

#ifndef MQTT_H_
#define MQTT_H_

#include <AsyncMqttClient.h>
#include <Ticker.h>

#define MQTT_PORT       1883    ///< Default MQTT port

class MQTTService {
  public:
    AsyncMqttClient     mqtt;           // MQTT object
    Ticker              mqttTicker;     // Ticker to handle MQTT

    // from original config struct
    bool        enabled;
    String      ip = " ";
    uint16_t    port = MQTT_PORT;
    String      user = " ";
    String      password = " ";
    String      topic;
    bool        clean;
    bool        hadisco;
    String      haprefix = "homeassistant";


    void setup();         ///< Call from setup()
    void onConnect();     ///< Call from onWifiConnect()
    void onDisconnect();  ///< Call from onWiFiDisconnect()
    void validate();      ///< Call from validateConfig()
    void update();        ///< Call from updateConfig()

    void load();          ///< Load configuration from SPIFFS
    void save();          ///< Save configuration to SPIFFS

  private:
    const char *CONFIG_FILE = "/mqtt.json";
    const char *SET_COMMAND_TOPIC = "/set";
    const char *ON = "ON";
    const char *OFF = "OFF";

    /////////////////////////////////////////////////////////
    //
    //  MQTT Section
    //
    /////////////////////////////////////////////////////////

    void connectToMqtt(); // onMqttDisconnect, onWifiConnect
    void onMqttConnect(bool sessionPresent); // setup
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason); // setup
    void onMqttMessage(char* topic, char* payload,
            AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total); // setup
    void publishHA(bool join); // updateConfig
    void publishState(); // onMqttConnect, onMqttMessage, procT, updateConfig
};

#endif  // MQTT_H_