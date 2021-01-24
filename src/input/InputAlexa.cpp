/*
* InputAlexa.cpp
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

#include <ArduinoJson.h>
#include "../ESPixelStick.h"
#include <Ticker.h>
#include <Int64String.h>
#include "InputAlexa.h"
#include "InputEffectEngine.hpp"

#if defined ARDUINO_ARCH_ESP32
#   include <functional>
#endif

//-----------------------------------------------------------------------------
c_InputAlexa::c_InputAlexa (
    c_InputMgr::e_InputChannelIds NewInputChannelId,
    c_InputMgr::e_InputType       NewChannelType,
    uint8_t                     * BufferStart,
    uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)

{
    // DEBUG_START;
    pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputChannelId_1, c_InputMgr::e_InputType::InputType_Effects, InputDataBuffer, InputDataBufferSize);
    pEffectsEngine->SetOperationalState (false);

    WebMgr.RegisterAlexaCallback ([this](EspalexaDevice* pDevice) {this->onMessage (pDevice); });

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputAlexa::~c_InputAlexa ()
{

    // allow the other input channels to run
    InputMgr.SetOperationalState (true);
    WebMgr.RegisterAlexaCallback ((DeviceCallbackFunction)nullptr);

    delete pEffectsEngine;
    pEffectsEngine = nullptr;

} // ~c_InputAlexa

//-----------------------------------------------------------------------------
void c_InputAlexa::Begin() 
{
    // DEBUG_START;

    LOG_PORT.println (String (F ("** 'Alexa' Initialization for input: '")) + InputChannelId + String (F ("' **")));

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }
    HasBeenInitialized = true;

    pEffectsEngine->Begin ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputAlexa::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;


    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputAlexa::GetStatus (JsonObject& /* jsonStatus */)
{
    // DEBUG_START;

    // JsonObject mqttStatus = jsonStatus.createNestedObject (F ("mqtt"));
    // mqttStatus["unifirst"] = startUniverse;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputAlexa::Process ()
{
    // DEBUG_START;
    // ignoring IsInputChannelActive
    
    pEffectsEngine->Process ();

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputAlexa::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

    pEffectsEngine->SetBufferInfo (BufferStart, BufferSize);

} // SetBufferInfo

//-----------------------------------------------------------------------------
boolean c_InputAlexa::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // disconnectFromMqtt ();

    // pEffectsEngine->SetConfig (jsonConfig);

    // validateConfiguration ();
    // RegisterWithMqtt ();
    // connectToMqtt ();

    // DEBUG_END;
    return true;
} // SetConfig

//-----------------------------------------------------------------------------
//TODO: Add MQTT configuration validation
void c_InputAlexa::validateConfiguration ()
{
    // DEBUG_START;
    // DEBUG_END;

} // validate

/////////////////////////////////////////////////////////
//
//  Alexa Section
//
/////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
void c_InputAlexa::onMessage(EspalexaDevice * pDevice)
{
 // DEBUG_START;
    do // once
    {
        char HexColor[] = "#000000 ";
        sprintf (HexColor, "#%02x%02x%02x", pDevice->getR (), pDevice->getG (), pDevice->getB ());

        DynamicJsonDocument JsonConfigDoc (1024);
        JsonObject JsonConfig = JsonConfigDoc.createNestedObject (F("config"));

        JsonConfig["EffectSpeed"]      = 1;
        JsonConfig["EffectReverse"]    = false;
        JsonConfig["EffectMirror"]     = false;
        JsonConfig["EffectAllLeds"]    = true;
        JsonConfig["EffectBrightness"] = pDevice->getValue ();
        JsonConfig["currenteffect"]    = "Solid";
        JsonConfig["EffectColor"]      = HexColor;

        pEffectsEngine->SetConfig (JsonConfig);

        InputMgr.SetOperationalState (!(pDevice->getState ()));
        pEffectsEngine->SetOperationalState (pDevice->getState ());

        memset (InputDataBuffer, 0x0, InputDataBufferSize);

        // DEBUG_V ("");

    } while (false);

 // DEBUG_END;

} // onMessage

