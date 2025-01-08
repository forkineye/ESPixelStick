/*
* InputAlexa.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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

#include "ESPixelStick.h"
#include "input/InputAlexa.h"
#include "input/InputEffectEngine.hpp"
#include "utility/SaferStringConversion.hpp"
#include <Int64String.h>

#if defined ARDUINO_ARCH_ESP32
#   include <functional>
#endif

//-----------------------------------------------------------------------------
c_InputAlexa::c_InputAlexa (
    c_InputMgr::e_InputChannelIds NewInputChannelId,
    c_InputMgr::e_InputType       NewChannelType,
    uint32_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)

{
    // DEBUG_START;

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputAlexa::~c_InputAlexa ()
{
    if (HasBeenInitialized)
    {
        // allow the other input channels to run
        InputMgr.SetOperationalState (true);
        WebMgr.RegisterAlexaCallback ((DeviceCallbackFunction)nullptr);

        if (pEffectsEngine)
        {
            delete pEffectsEngine;
            pEffectsEngine = nullptr;
        }
    }

} // ~c_InputAlexa

//-----------------------------------------------------------------------------
void c_InputAlexa::Begin()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        // DEBUG_END;
        return;
    }
    HasBeenInitialized = true;

    pEffectsEngine = new c_InputEffectEngine (c_InputMgr::e_InputChannelIds::InputSecondaryChannelId, c_InputMgr::e_InputType::InputType_Effects, InputDataBufferSize);
    pEffectsEngine->SetOperationalState (false);

    WebMgr.RegisterAlexaCallback ([this](EspalexaDevice* pDevice) {this->onMessage (pDevice); });

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
    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputAlexa::Process ()
{
    // DEBUG_START;
    if (IsInputChannelActive)
    {
        pEffectsEngine->Process ();
    }

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputAlexa::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;

    // DEBUG_V (String ("InputDataBufferSize: ") + String (InputDataBufferSize));

    pEffectsEngine->SetBufferInfo (BufferSize);

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputAlexa::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // DEBUG_END;

    return true;
} // SetConfig

//-----------------------------------------------------------------------------
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
        OutputMgr.ClearBuffer ();

        char HexColor[8];
        ESP_ERROR_CHECK(saferRgbToHtmlColorString(HexColor, pDevice->getR (), pDevice->getG (), pDevice->getB ()));

        // DEBUG_V (String ("pDevice->getR: ") + String (pDevice->getR ()));
        // DEBUG_V (String ("pDevice->getG: ") + String (pDevice->getG ()));
        // DEBUG_V (String ("pDevice->getB: ") + String (pDevice->getB ()));

        JsonDocument JsonConfigDoc;
        JsonObject JsonConfig = JsonConfigDoc[(char*)CN_config].to<JsonObject> ();

        JsonWrite(JsonConfig, CN_EffectSpeed,      1);
        JsonWrite(JsonConfig, CN_EffectReverse,    false);
        JsonWrite(JsonConfig, CN_EffectMirror,     false);
        JsonWrite(JsonConfig, CN_EffectAllLeds,    true);
        JsonWrite(JsonConfig, CN_EffectBrightness, map (pDevice->getValue (), 0, 255, 0, 100));
        JsonWrite(JsonConfig, CN_currenteffect,    F ("Solid"));
        JsonWrite(JsonConfig, CN_EffectColor,      HexColor);
        // DEBUG_V (String ("CN_EffectBrightness: ") + String (pDevice->getValue ()));
        // DEBUG_V (String ("getState: ") + String (pDevice->getState ()));

        InputMgr.SetOperationalState (!(pDevice->getState ()));
        SetOperationalState (pDevice->getState ());
        pEffectsEngine->SetOperationalState (pDevice->getState ());
        pEffectsEngine->SetConfig (JsonConfig);

        // DEBUG_V ("");

    } while (false);

    // DEBUG_END;

} // onMessage
