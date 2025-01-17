/*
* InputEffectEngine.cpp - Code to wrap ESPAsyncE131 for input
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
#include "utility/SaferStringConversion.hpp"
#include "input/InputEffectEngine.hpp"
#include <vector>

//-----------------------------------------------------------------------------
// Local Structure and Data Definitions
//-----------------------------------------------------------------------------

// List of all the supported effects and their names
static const c_InputEffectEngine::EffectDescriptor_t ListOfEffects[] =
{//                                                                              Mirror      AllLeds      wsTCode
    //    name                              func                   htmlid               Color      Reverse

        // { "Disabled",     nullptr,                             "t_disabled",     1,    1,    1,    1,  "T0"     },
        { "Solid",        &c_InputEffectEngine::effectSolidColor, "t_static",       1,    0,    0,    0,  "T1"     },
        { "Blink",        &c_InputEffectEngine::effectBlink,      "t_blink",        1,    0,    0,    0,  "T2"     },
        { "Flash",        &c_InputEffectEngine::effectFlash,      "t_flash",        1,    0,    0,    0,  "T3"     },
        { "Rainbow",      &c_InputEffectEngine::effectRainbow,    "t_rainbow",      0,    1,    1,    1,  "T5"     },
        { "Chase",        &c_InputEffectEngine::effectChase,      "t_chase",        1,    1,    1,    0,  "T4"     },
        { "Fire flicker", &c_InputEffectEngine::effectFireFlicker,"t_fireflicker",  1,    0,    0,    0,  "T6"     },
        { "Lightning",    &c_InputEffectEngine::effectLightning,  "t_lightning",    1,    0,    0,    0,  "T7"     },
        { "Breathe",      &c_InputEffectEngine::effectBreathe,    "t_breathe",      1,    0,    0,    0,  "T8"     },
        { "Random",       &c_InputEffectEngine::effectRandom,     "t_random",       0,    0,    0,    0,  "T9"     },
        { "Transition",   &c_InputEffectEngine::effectTransition, "t_Transition",   0,    0,    0,    0,  "T10"    },
        { "Marquee",      &c_InputEffectEngine::effectMarquee,    "t_Marquee",      0,    0,    0,    0,  "T11"    }
};

static std::vector<c_InputEffectEngine::dCRGB> TransitionColorTable =
{
	{ 85,  85,  85},
	{128, 128,   0},
	{128,   0, 128},
	{  0, 128, 128},
	{ 28, 128, 100},
	{128, 100,  28},
	{100,  28, 128},
	{ 40, 175,  40},
	{175,  40,  40},
	{ 40,  40, 175},
	{191,  64,   0},
	{ 64,   0, 191},
	{  0, 191,  64},
	{128,  64,  64},
	{ 64, 128,  64},
	{ 64,  64, 128},
	{ 80, 144,  32},
	{144,  32,  80},
	{ 32,  80, 144},
	{100, 100,  55},
	{ 55, 100, 100},
	{100, 100,  55},
};

static std::vector<c_InputEffectEngine::MarqueeGroup_t> MarqueueGroupTable =
{
    {5, {255, 0, 0}, 100, 100},
    {5, {255, 255, 255}, 100, 0},
}; // MarqueueGroupTable

//-----------------------------------------------------------------------------
c_InputEffectEngine::c_InputEffectEngine (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                          c_InputMgr::e_InputType       NewChannelType,
                                          uint32_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)
{
    // DEBUG_START;
    // set a default effect
    ActiveEffect = &ListOfEffects[0];

    SetBufferInfo (BufferSize);

    TransitionInfo.TargetColorIterator = TransitionColorTable.begin();

    // DEBUG_END;
} // c_InputEffectEngine

//-----------------------------------------------------------------------------
c_InputEffectEngine::c_InputEffectEngine () :
    c_InputCommon (c_InputMgr::e_InputChannelIds::InputPrimaryChannelId,
                   c_InputMgr::e_InputType::InputType_Effects, 0)
{
    // DEBUG_START;
    // set a default effect
    ActiveEffect = &ListOfEffects[0];

    SetBufferInfo (0);

    TransitionInfo.TargetColorIterator = TransitionColorTable.begin();

    // DEBUG_END;

} // c_InputEffectEngine

//-----------------------------------------------------------------------------
c_InputEffectEngine::~c_InputEffectEngine ()
{

} // ~c_InputEffectEngine

//-----------------------------------------------------------------------------
void c_InputEffectEngine::Begin ()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        return;
    }
    HasBeenInitialized = true;

    validateConfiguration ();
    // DEBUG_V ("");

    // DEBUG_END;
} // Begin

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;
    char HexColor[8];
    ESP_ERROR_CHECK(saferRgbToHtmlColorString(HexColor, EffectColor.r, EffectColor.g, EffectColor.b));
    // DEBUG_V ("");

    JsonWrite(jsonConfig, CN_currenteffect,      ActiveEffect->name);
    JsonWrite(jsonConfig, CN_EffectSpeed,        EffectSpeed);
    JsonWrite(jsonConfig, CN_EffectReverse,      EffectReverse);
    JsonWrite(jsonConfig, CN_EffectMirror,       EffectMirror);
    JsonWrite(jsonConfig, CN_EffectAllLeds,      EffectAllLeds);
    JsonWrite(jsonConfig, CN_EffectBrightness,   uint32_t(EffectBrightness * 100.0));
    JsonWrite(jsonConfig, CN_EffectWhiteChannel, EffectWhiteChannel);
    JsonWrite(jsonConfig, CN_EffectColor,        HexColor);
    JsonWrite(jsonConfig, CN_pixel_count,        effectMarqueePixelAdvanceCount);

    JsonWrite(jsonConfig, "FlashEnable",   FlashInfo.Enable);
    JsonWrite(jsonConfig, "FlashMinInt",   FlashInfo.MinIntensity);
    JsonWrite(jsonConfig, "FlashMaxInt",   FlashInfo.MaxIntensity);
    JsonWrite(jsonConfig, "FlashMinDelay", FlashInfo.MinDelayMS);
    JsonWrite(jsonConfig, "FlashMaxDelay", FlashInfo.MaxDelayMS);
    JsonWrite(jsonConfig, "FlashMinDur",   FlashInfo.MinDurationMS);
    JsonWrite(jsonConfig, "FlashMaxDur",   FlashInfo.MaxDurationMS);
    JsonWrite(jsonConfig, "TransCount",    TransitionInfo.StepsToTarget);
    JsonWrite(jsonConfig, "TransDelay",    TransitionInfo.TimeAtTargetMs);

    // DEBUG_V ("");

    JsonArray EffectsArray = jsonConfig[(char*)CN_effects].to<JsonArray> ();
    // DEBUG_V ("");

    for (EffectDescriptor_t currentEffect : ListOfEffects)
    {
        // DEBUG_V ("");
        JsonObject currentJsonEntry = EffectsArray.add<JsonObject> ();
        JsonWrite(currentJsonEntry, CN_name, currentEffect.name);
    }

    JsonArray TransitionsArray = jsonConfig[(char*)CN_transitions].to<JsonArray> ();
    for (auto currentTransition : TransitionColorTable)
    {
        // DEBUG_V ("");
        JsonObject currentJsonEntry = TransitionsArray.add<JsonObject> ();
        JsonWrite(currentJsonEntry, CN_r, currentTransition.r);
        JsonWrite(currentJsonEntry, CN_g, currentTransition.g);
        JsonWrite(currentJsonEntry, CN_b, currentTransition.b);
    }

    JsonArray MarqueeGroupArray = jsonConfig[(char*)CN_MarqueeGroups].to<JsonArray> ();
    for(auto CurrentMarqueeGroup : MarqueueGroupTable)
    {
        JsonObject currentJsonEntry = MarqueeGroupArray.add<JsonObject> ();
        JsonObject currentJsonEntryColor = currentJsonEntry[(char*)CN_color].to<JsonObject> ();
        JsonWrite(currentJsonEntryColor, CN_r, CurrentMarqueeGroup.Color.r);
        JsonWrite(currentJsonEntryColor, CN_g, CurrentMarqueeGroup.Color.g);
        JsonWrite(currentJsonEntryColor, CN_b, CurrentMarqueeGroup.Color.b);
        JsonWrite(currentJsonEntry, CN_brightness,    CurrentMarqueeGroup.StartingIntensity);
        JsonWrite(currentJsonEntry, CN_pixel_count,   CurrentMarqueeGroup.NumPixelsInGroup);
        JsonWrite(currentJsonEntry, CN_brightnessEnd, CurrentMarqueeGroup.EndingIntensity);
    }

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetMqttEffectList (JsonObject& jsonConfig)
{
    // DEBUG_START;
    JsonArray EffectsArray = jsonConfig[(char*)CN_effect_list].to<JsonArray> ();

    for (EffectDescriptor_t currentEffect : ListOfEffects)
    {
        EffectsArray.add (currentEffect.name);
    }
    // DEBUG_END;
} // GetMqttEffectList

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetMqttConfig (MQTTConfiguration_t & mqttConfig)
{
    // DEBUG_START;

    mqttConfig.effect       = ActiveEffect->name;
    mqttConfig.mirror       = EffectMirror;
    mqttConfig.allLeds      = EffectAllLeds;
    mqttConfig.brightness   = uint8_t(EffectBrightness * 255.0);
    mqttConfig.whiteChannel = EffectWhiteChannel;

    mqttConfig.color.r = EffectColor.r;
    mqttConfig.color.g = EffectColor.g;
    mqttConfig.color.b = EffectColor.b;

    // DEBUG_END;

} // GetMqttConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject Status = jsonStatus[(char*)CN_effects].to<JsonObject> ();
    JsonWrite(Status, CN_currenteffect, ActiveEffect->name);
    JsonWrite(Status, CN_id,            InputChannelId);

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputEffectEngine::NextEffect ()
{
    // DEBUG_START;

    // DEBUG_V ("Find the current effect");
    uint32_t CurrentEffectIndex = 0;
    for (const EffectDescriptor_t currentEffect : ListOfEffects)
    {
        // DEBUG_V (String ("currentEffect.name: ") + currentEffect.name);
        if (ActiveEffect->name == currentEffect.name)
        {
            // DEBUG_V (String ("currentEffect.name: ") + currentEffect.name);
            break;
        }

        ++CurrentEffectIndex;
    }

    // we now have the index of the current effect
    ++CurrentEffectIndex;
    if (sizeof(ListOfEffects)/sizeof(ListOfEffects[0]) <= CurrentEffectIndex)
    {
        // DEBUG_V ("Wrap to first effect");
        CurrentEffectIndex = 0;
    }

    // DEBUG_V (String ("CurrentEffectIndex: ") + String(CurrentEffectIndex));
    setEffect (ListOfEffects[CurrentEffectIndex].name);
    logcon (String (F ("Setting new effect: ")) + ActiveEffect->name);
    // DEBUG_V (String ("ActiveEffect->name: ") + ActiveEffect->name);

    // DEBUG_END;
} // NextEffect

//-----------------------------------------------------------------------------
void c_InputEffectEngine::PollFlash ()
{
    do // once
    {
        if(!FlashInfo.Enable)
        {
            // not doing random flashing
            break;
        }
        //  // DEBUG_V("Flash is enabled");

        if(!FlashInfo.delaytimer.IsExpired())
        {
            // not time to flash yet
            break;
        }

        // is the flash done?
        if(FlashInfo.durationtimer.IsExpired())
        {
            // set up the next flash
            uint32_t NextDelay = random(FlashInfo.MinDelayMS, FlashInfo.MaxDelayMS);
            uint32_t NextDuration = random(FlashInfo.MinDurationMS, FlashInfo.MaxDurationMS);

            FlashInfo.delaytimer.StartTimer(NextDelay, false);
            FlashInfo.durationtimer.StartTimer(NextDelay + NextDuration, false);

            // force the effect to overwrite the buffer
            EffectDelayTimer.CancelTimer();
            // DEBUG_V(String("         now: ") + String(now));
            // DEBUG_V(String("   NextDelay: ") + String(NextDelay));
            // DEBUG_V(String("NextDuration: ") + String(NextDuration));

            // dont overwrite the buffer
            break;
        }

        uint8_t intensity = uint8_t(map(random( FlashInfo.MinIntensity, FlashInfo.MaxIntensity),0,100,0,255));
        CRGB color;
        color.r = intensity;
        color.g = intensity;
        color.b = intensity;
        setAll(color);
    } while(false);

} // PollFlash

//-----------------------------------------------------------------------------
void c_InputEffectEngine::Process ()
{
    // DEBUG_START;
    // DEBUG_V (String ("HasBeenInitialized: ") + HasBeenInitialized);
    // DEBUG_V (String ("PixelCount: ") + PixelCount);

    do // once
    {
        if (!HasBeenInitialized)
        {
            break;
        }
        // DEBUG_V ("Init OK");

        if (0 == PixelCount)
        {
            break;
        }
        // DEBUG_V ("Pixel Count OK");

        if(!EffectDelayTimer.IsExpired())
        {
            PollFlash();
            break;
        }

        // timer has expired
        uint32_t wait = (this->*ActiveEffect->func)();
        uint32_t NewEffectWait = max ((int)wait, MIN_EFFECT_DELAY);
        if(NewEffectWait != EffectWait)
        {
            EffectWait = NewEffectWait;
            // DEBUG_V ("Update timer");
            EffectDelayTimer.StartTimer(EffectWait, true);
        }
        EffectCounter++;
        InputMgr.RestartBlankTimer (GetInputChannelId ());

        PollFlash();

    } while (false);

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputEffectEngine::Poll ()
{
    // // DEBUG_START;

    // // DEBUG_V (String ("HasBeenInitialized: ") + HasBeenInitialized);
    // // DEBUG_V (String ("PixelCount: ") + PixelCount);
#ifdef ARDUINO_ARCH_ESP32
    do // once
    {
        if (!HasBeenInitialized || Disabled)
        {
            break;
        }
        // // DEBUG_V ("Init OK");

        if ((0 == PixelCount) || (StayDark))
        {
            break;
        }
        // // DEBUG_V ("Pixel Count OK");

        // do we have an active effect?
        if(ActiveEffect == nullptr)
        {
            break;
        }
        // // DEBUG_V("Have Active effect");
        if(ActiveEffect->func == nullptr)
        {
            break;
        }

        // // DEBUG_V("has the flash timer expired?");
        if(!EffectDelayTimer.IsExpired())
        {
            // // DEBUG_V("Flash Timer has NOT expired");
            PollFlash();
            break;
        }

        // // DEBUG_V("timer has expired");
        uint32_t wait = (this->*ActiveEffect->func)();
        uint32_t NewEffectWait = max ((int)wait, MIN_EFFECT_DELAY);
        if(NewEffectWait != EffectWait)
        {
            EffectWait = NewEffectWait;
            // // DEBUG_V ("Update timer");
            EffectDelayTimer.StartTimer(EffectWait, true);
        }
        // // DEBUG_V("Update Blank timer");
        EffectCounter++;
        InputMgr.RestartBlankTimer (GetInputChannelId ());

        // // DEBUG_V("Check Flash operation");
        PollFlash();

    } while (false);

    // // DEBUG_END;
#endif // def ARDUINO_ARCH_ESP32

} // process

//----------------------------------------------------------------------------
void c_InputEffectEngine::ProcessButtonActions(c_ExternalInput::InputValue_t value)
{
    // DEBUG_START;

    if(c_ExternalInput::InputValue_t::longOn == value)
    {
        // DEBUG_V("flip the dark flag");
        StayDark = !StayDark;
        // DEBUG_V(String("StayDark: ") + String(StayDark));

    }
    else if(c_ExternalInput::InputValue_t::shortOn == value)
    {
        // DEBUG_V("Move to the next effect");
        NextEffect();
    }
    else if(c_ExternalInput::InputValue_t::off == value)
    {
        // DEBUG_V("Got input Off notification");
    }

    // DEBUG_END;
} // ProcessButtonActions

//-----------------------------------------------------------------------------
void c_InputEffectEngine::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;

    // DEBUG_V (String ("BufferSize: ") + String (BufferSize));
    ChannelsPerPixel = (true == EffectWhiteChannel) ? 4 : 3;
    PixelCount = InputDataBufferSize / ChannelsPerPixel;

    PixelOffset = PixelCount & 0x0001; // handle odd number of pixels

    MirroredPixelCount = PixelCount;
    if (EffectMirror)
    {
        MirroredPixelCount = (PixelCount / 2) + PixelOffset;
    }

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputEffectEngine::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;
    String effectName;
    String effectColor;

    // PrettyPrint(jsonConfig, "Effects");

    setFromJSON (EffectSpeed,        jsonConfig, CN_EffectSpeed);
    setFromJSON (EffectReverse,      jsonConfig, CN_EffectReverse);
    setFromJSON (EffectMirror,       jsonConfig, CN_EffectMirror);
    setFromJSON (EffectAllLeds,      jsonConfig, CN_EffectAllLeds);
    setFromJSON (EffectBrightness,   jsonConfig, CN_EffectBrightness);
    setFromJSON (EffectWhiteChannel, jsonConfig, CN_EffectWhiteChannel);
    setFromJSON (effectName,         jsonConfig, CN_currenteffect);
    setFromJSON (effectColor,        jsonConfig, CN_EffectColor);
    // DEBUG_V (String ("effectColor: ") + effectColor);
    setFromJSON (effectMarqueePixelAdvanceCount, jsonConfig, CN_pixel_count);

    setFromJSON (FlashInfo.Enable,             jsonConfig, "FlashEnable");
    setFromJSON (FlashInfo.MinIntensity,       jsonConfig, "FlashMinInt");
    setFromJSON (FlashInfo.MaxIntensity,       jsonConfig, "FlashMaxInt");
    setFromJSON (FlashInfo.MinDelayMS,         jsonConfig, "FlashMinDelay");
    setFromJSON (FlashInfo.MaxDelayMS,         jsonConfig, "FlashMaxDelay");
    setFromJSON (FlashInfo.MinDurationMS,      jsonConfig, "FlashMinDur");
    setFromJSON (FlashInfo.MaxDurationMS,      jsonConfig, "FlashMaxDur");

    setFromJSON (TransitionInfo.StepsToTarget,  jsonConfig, "TransCount");
    setFromJSON (TransitionInfo.TimeAtTargetMs, jsonConfig, "TransDelay");
    // DEBUG_V(String(" TransitionInfo.StepsToTarget: ") + String(TransitionInfo.StepsToTarget));
    // DEBUG_V(String("TransitionInfo.TimeAtTargetMs: ") + String(TransitionInfo.TimeAtTargetMs));

    // avoid divide by zero errors later in the processing.
    TransitionInfo.StepsToTarget = max(uint32_t(1), TransitionInfo.StepsToTarget);
    // Pretend we reached the currnt color.
    TransitionInfo.CurrentColor = *TransitionInfo.TargetColorIterator;

    // apply minimum transition hold time
    TransitionInfo.TimeAtTargetMs = max(uint32_t(1), TransitionInfo.TimeAtTargetMs);
    // DEBUG_V(String(" TransitionInfo.StepsToTarget: ") + String(TransitionInfo.StepsToTarget));
    // DEBUG_V(String("TransitionInfo.TimeAtTargetMs: ") + String(TransitionInfo.TimeAtTargetMs));

    // make sure max is really max
    if(FlashInfo.MinIntensity >= FlashInfo.MaxIntensity)
    {
        FlashInfo.MinIntensity = FlashInfo.MaxIntensity;
    }

    JsonArray TransitionsArray = jsonConfig[(char*)CN_transitions];
    if(TransitionsArray)
    {
        TransitionColorTable.clear();

        for (auto Transition : TransitionsArray)
        {
            // DEBUG_V ("");
            dCRGB NewColorTarget;
            JsonObject currentTransition = Transition.as<JsonObject>();
            setFromJSON (NewColorTarget.r, currentTransition, "r");
            setFromJSON (NewColorTarget.g, currentTransition, "g");
            setFromJSON (NewColorTarget.b, currentTransition, "b");
            // DEBUG_V (String("NewColorTarget.r: ") + String(NewColorTarget.r));
            // DEBUG_V (String("NewColorTarget.g: ") + String(NewColorTarget.g));
            // DEBUG_V (String("NewColorTarget.b: ") + String(NewColorTarget.b));

            TransitionColorTable.push_back(NewColorTarget);
        }
    }

    JsonArray MarqueeGroupArray = jsonConfig[(char*)CN_MarqueeGroups];
    if(MarqueeGroupArray)
    {
        MarqueueGroupTable.clear();

        for (auto _MarqueeGroup : MarqueeGroupArray)
        {
            MarqueeGroup_t NewGroup;
            JsonObject currentMarqueeGroup = _MarqueeGroup.as<JsonObject>();
            // DEBUG_V ("");
            JsonObject GroupColor = currentMarqueeGroup[(char*)CN_color];
            setFromJSON (NewGroup.Color.r, GroupColor, "r");
            setFromJSON (NewGroup.Color.g, GroupColor, "g");
            setFromJSON (NewGroup.Color.b, GroupColor, "b");
            // DEBUG_V (String("NewGroup.Color.r: ") + String(NewGroup.Color.r));
            // DEBUG_V (String("NewGroup.Color.g: ") + String(NewGroup.Color.g));
            // DEBUG_V (String("NewGroup.Color.b: ") + String(NewGroup.Color.b));

            setFromJSON (NewGroup.NumPixelsInGroup, currentMarqueeGroup, CN_pixel_count);
            setFromJSON (NewGroup.StartingIntensity, currentMarqueeGroup, CN_brightness);
            setFromJSON (NewGroup.EndingIntensity, currentMarqueeGroup, CN_brightnessEnd);

            MarqueueGroupTable.push_back(NewGroup);
        }
    }

    EffectBrightness /= 100.0;

    SetBufferInfo (InputDataBufferSize);

    setColor (effectColor);
    validateConfiguration ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    setEffect (effectName);

    // DEBUG_V (String ("IsInputChannelActive: ") + String (IsInputChannelActive));

    // DEBUG_END;
    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::SetMqttConfig (MQTTConfiguration_t& mqttConfig)
{
    // DEBUG_START;
    String effectName;

    effectName = mqttConfig.effect;
    EffectMirror = mqttConfig.mirror;
    EffectAllLeds = mqttConfig.allLeds;
    EffectWhiteChannel = mqttConfig.whiteChannel;

    uint16_t tempBrightness = uint8_t (EffectBrightness * 255.0);
    tempBrightness = mqttConfig.brightness;
    EffectBrightness = float (tempBrightness) / 255.0;

    EffectColor.r = mqttConfig.color.r;
    EffectColor.g = mqttConfig.color.g;
    EffectColor.b = mqttConfig.color.b;

    SetBufferInfo (InputDataBufferSize);

    validateConfiguration ();

    setEffect (effectName);

    // DEBUG_END;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::validateConfiguration ()
{
    // DEBUG_START;

    setBrightness (EffectBrightness);
    setSpeed (EffectSpeed);
    setDelay (EffectDelay);

    // DEBUG_END;

} // validateConfiguration

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setBrightness (float brightness)
{
    // DEBUG_START;

    EffectBrightness = brightness;
    if (EffectBrightness > 1.0) { EffectBrightness = 1.0; }
    if (EffectBrightness < 0.0) { EffectBrightness = 0.0; }

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
// Yukky maths here. Input speeds from 1..10 get mapped to 17782..100
void c_InputEffectEngine::setSpeed (uint16_t speed)
{
    EffectSpeed = speed;
    // DEBUG_V(String("EffectSpeed: ") + String(speed));
    setDelay (pow (10, (10 - speed) / 4.0 + 2));
}

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setDelay (uint16_t delay)
{
    // DEBUG_START;

    EffectDelay = delay;
    if (EffectDelay < MIN_EFFECT_DELAY)
    {
        EffectDelay = MIN_EFFECT_DELAY;
    }
    // DEBUG_V(String("EffectDelay: ") + String(EffectDelay));

    // DEBUG_END;
} // setDelay

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setEffect (const String & effectName)
{
    // DEBUG_START;

    int EffectIndex = 0;
    for (EffectDescriptor_t currentEffect : ListOfEffects)
    {
        if (effectName.equalsIgnoreCase (currentEffect.name))
        {
            // DEBUG_V ("Found desired effect");
            if (!ActiveEffect->name.equalsIgnoreCase (currentEffect.name))
            {
                // DEBUG_V ("Starting New Effect");
                ActiveEffect = &ListOfEffects[EffectIndex];
                EffectDelayTimer.StartTimer(EffectDelay, true);
                EffectWait = MIN_EFFECT_DELAY;
                EffectCounter = 0;
                EffectStep = 0;
            }
            break;
        }
        EffectIndex++;
    } // end for each effect

    // DEBUG_END;

} // setEffect

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setColor (String & NewColor)
{
    // DEBUG_START;

    // DEBUG_V ("NewColor: " + NewColor);

    // Parse the color string into rgb values

    uint32_t intValue = strtoul (NewColor.substring (1).c_str (), nullptr, 16);
    // DEBUG_V (String ("intValue: ") + String (intValue, 16));

    EffectColor.r = uint8_t ((intValue >> 16) & 0xFF);
    EffectColor.g = uint8_t ((intValue >> 8) & 0xFF);
    EffectColor.b = uint8_t ((intValue >> 0) & 0xFF);

    // DEBUG_END;

} // setColor

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setPixel (uint16_t pixelId, CRGB color)
{
    // DEBUG_START;

    // DEBUG_V (String ("IsInputChannelActive: ") + String(IsInputChannelActive));
    // DEBUG_V (String ("pixelId: ") + pixelId);
    // DEBUG_V (String ("PixelCount: ") + PixelCount);

    if ((true == IsInputChannelActive) && (pixelId < PixelCount))
    {
        uint8_t PixelBuffer[sizeof(CRGB)+1];

        // DEBUG_V(String("ChannelsPerPixel * pixelId: 0x") + String(uint(ChannelsPerPixel * pixelId), HEX));
        // DEBUG_V (String ("EffectBrightness: ") + String (EffectBrightness));
        // DEBUG_V (String ("color.r: ") + String (color.r));
        // DEBUG_V (String ("color.g: ") + String (color.g));
        // DEBUG_V (String ("color.b: ") + String (color.b));

        PixelBuffer[0] = color.r * EffectBrightness;
        PixelBuffer[1] = color.g * EffectBrightness;
        PixelBuffer[2] = color.b * EffectBrightness;
        if (4 == ChannelsPerPixel)
        {
            PixelBuffer[3] = 0; // no white data
        }
        OutputMgr.WriteChannelData(pixelId * ChannelsPerPixel, ChannelsPerPixel, PixelBuffer);
    }

    // DEBUG_END;

} // setPixel

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetPixel (uint16_t pixelId, CRGB & out)
{
    // DEBUG_START;

    // DEBUG_V (String ("IsInputChannelActive: ") + String(IsInputChannelActive));
    // DEBUG_V (String ("pixelId: ") + pixelId);
    // DEBUG_V (String ("PixelCount: ") + PixelCount);

    if (pixelId < PixelCount)
    {
        byte PixelData[sizeof(CRGB)];
        OutputMgr.ReadChannelData(uint32_t(ChannelsPerPixel * pixelId), sizeof(PixelData), PixelData);

        out.r = PixelData[0];
        out.g = PixelData[1];
        out.b = PixelData[2];
    }

    // DEBUG_END;

} // getPixel

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setRange (uint16_t FirstPixelId, uint16_t NumberOfPixels, CRGB color)
{
    for (uint16_t i = FirstPixelId; i < min(uint32_t (FirstPixelId + NumberOfPixels), PixelCount); i++)
    {
        setPixel (i, color);
    }
} // setRange

//-----------------------------------------------------------------------------
void c_InputEffectEngine::clearRange (uint16_t FirstPixelId, uint16_t NumberOfPixels)
{
    for (uint16_t i = FirstPixelId; i < min (uint32_t (FirstPixelId + NumberOfPixels), PixelCount); i++)
    {
        setPixel (i, { 0, 0, 0 });
    }
}

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setAll (CRGB color)
{
    setRange (0, PixelCount, color);
} // setAll

//-----------------------------------------------------------------------------
void c_InputEffectEngine::clearAll ()
{
    clearRange (0, PixelCount);
} // clearAll

//-----------------------------------------------------------------------------
c_InputEffectEngine::CRGB c_InputEffectEngine::colorWheel (uint8_t pos)
{
    CRGB Response = { 0, 0, 0 };

    pos = 255 - pos;
    if (pos < 85)
    {
        Response = { uint8_t (255 - pos * 3), 0, uint8_t (pos * 3) };
    }
    else if (pos < 170)
    {
        pos -= 85;
        Response = { 0, uint8_t (pos * 3), uint8_t (255 - pos * 3) };
    }
    else
    {
        pos -= 170;
        Response = { uint8_t (pos * 3), uint8_t (255 - pos * 3), 0 };
    }

    return Response;
} // colorWheel

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectSolidColor ()
{
    // DEBUG_START;

    // DEBUG_V (String ("PixelCount: ") + PixelCount);

    setAll (EffectColor);

    // DEBUG_END;
    return 32;
} // effectSolidColor

//-----------------------------------------------------------------------------
void c_InputEffectEngine::outputEffectColor (uint16_t pixelId, CRGB outputColor)
{
    // DEBUG_START;

    uint16_t NumPixels = MirroredPixelCount;

    if (EffectReverse)
    {
        pixelId = (NumPixels - 1) - pixelId;
    }

    if (EffectMirror)
    {
        setPixel ((NumPixels - 1) - pixelId, outputColor);
        setPixel (((NumPixels)+pixelId) - PixelOffset, outputColor);
    }
    else
    {
        setPixel (pixelId, outputColor);
    }

    // DEBUG_END;
} // outputEffectColor

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectChase ()
{
    // DEBUG_START;

    // remove the current output data
    clearAll ();

    // calculate only half the pixels if mirroring
    uint16_t lc = MirroredPixelCount;

    // Prevent errors if we come from another effect with more steps
    // or switch from the upper half of non-mirror to mirror mode
    EffectStep = EffectStep % lc;

    outputEffectColor (EffectStep, EffectColor);

    // EffectStep = (1 + EffectStep) % lc;
    ++EffectStep;

    // DEBUG_END;
    return EffectDelay / 32;
} // effectChase

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectRainbow ()
{
    // Effect Step will be in the range zero through the number of pixels and is used
    // to set the starting point for the colors

    // DEBUG_START;
    // calculate only half the pixels if mirroring
    uint16_t NumberOfPixelsToOutput = MirroredPixelCount;

    // Next step or wrap
    if (++EffectStep >= NumberOfPixelsToOutput) { EffectStep = 0; }

    // DEBUG_V (String ("MirroredPixelCount: ") + String (MirroredPixelCount));
    // DEBUG_V (String ("        EffectStep: ") + String (EffectStep));

    for (uint16_t CurrentPixelId = 0; CurrentPixelId < NumberOfPixelsToOutput; CurrentPixelId++)
    {
        uint32_t hue = EffectStep;
        if (!EffectAllLeds)
        {
            hue = CurrentPixelId + EffectStep;
            if (hue > NumberOfPixelsToOutput) { hue -= NumberOfPixelsToOutput; }
        }
        hue = map (hue, 0, NumberOfPixelsToOutput, 0, 359);
        CRGB color = hsv2rgb ({ double (hue), 1.0, 1.0 });

        outputEffectColor ((NumberOfPixelsToOutput - CurrentPixelId) - 1, color);
        // outputEffectColor (CurrentPixelId, color);
    }

    // DEBUG_END;
    return (EffectDelay / 256);

} // effectRainbow

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectRandom ()
{
    // Effect Step will be in the range zero through the number of pixels and is used
    // to set the starting point for the colors

    // DEBUG_START;
    // calculate only half the pixels if mirroring
    uint16_t NumberOfPixelsToOutput = MirroredPixelCount;

    // DEBUG_V (String ("MirroredPixelCount: ") + String (MirroredPixelCount));

    for (uint16_t CurrentPixelId = 0; CurrentPixelId < NumberOfPixelsToOutput; CurrentPixelId++)
    {
        CRGB RgbColor;
        GetPixel (CurrentPixelId, RgbColor);
        // DEBUG_V (String ("    RgbColor.r: ") + String (RgbColor.r));
        // DEBUG_V (String ("    RgbColor.g: ") + String (RgbColor.g));
        // DEBUG_V (String ("    RgbColor.b: ") + String (RgbColor.b));

        dCHSV HsvColor = rgb2hsv(RgbColor);
        // DEBUG_V (String ("CurrentPixelId: ") + String (CurrentPixelId));
        // DEBUG_V (String ("         value: ") + String (HsvColor.v));
        // DEBUG_V (String ("    saturation: ") + String (HsvColor.s));

        // is a new color needed
        if (HsvColor.v > 0.01)
        {
            // DEBUG_V ("adjust existing color value");
            HsvColor.v -= 0.01;
            // DEBUG_V (String ("         value: ") + String (HsvColor.v));
        }
        else
        {
            // DEBUG_V ("set up a new color");
            HsvColor.h = double (random (359));
            HsvColor.s = double (random (50, 100)) / 100;
            HsvColor.v = double (random (50, 100)) / 100;

            // RgbColor = hsv2rgb (HsvColor);
            // DEBUG_V (String ("           hue: ") + String (HsvColor.h));
            // DEBUG_V (String ("    saturation: ") + String (HsvColor.s));
            // DEBUG_V (String ("         value: ") + String (HsvColor.v));
            // DEBUG_V (String ("    RgbColor.r: ") + String (RgbColor.r));
            // DEBUG_V (String ("    RgbColor.g: ") + String (RgbColor.g));
            // DEBUG_V (String ("    RgbColor.b: ") + String (RgbColor.b));
        }

        RgbColor = hsv2rgb (HsvColor);
        outputEffectColor (CurrentPixelId, RgbColor);
    }

    // DEBUG_END;
    return (EffectDelay / 500);

} // effectRandom

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectTransition ()
{
    /*
        All pixels will be changed to the next color in a list of colors
    */

    // DEBUG_START;
    // DEBUG_V(String("TransitionTargetColorId: ") + String(TransitionTargetColorId));

    if(0 != TransitionInfo.HoldStartTimeMs)
    {
        uint32_t timeSinceTimerStarted = abs(int32_t(millis()) - int32_t(TransitionInfo.HoldStartTimeMs));
        // DEBUG_V(String("        timeSinceTimerStarted: ") + String(timeSinceTimerStarted));
        // DEBUG_V(String("TransitionInfo.TimeAtTargetMs: ") + String(TransitionInfo.TimeAtTargetMs));
        if(TransitionInfo.TimeAtTargetMs < timeSinceTimerStarted)
        {
            // DEBUG_V("Transition Timer Has Expired");
            TransitionInfo.HoldStartTimeMs = 0;
        }
    }
    else if(ColorHasReachedTarget())
    {
        // DEBUG_V("need to calculate a new target color");

        // remove any calculation errors
        TransitionInfo.CurrentColor = *TransitionInfo.TargetColorIterator;

        ++TransitionInfo.TargetColorIterator;

        // wrap the index
        if(TransitionInfo.TargetColorIterator == TransitionColorTable.end())
        {
            // DEBUG_V("Wrap Transition iterator");
            TransitionInfo.TargetColorIterator = TransitionColorTable.begin();
        }

        CalculateTransitionStepValue (TransitionInfo.TargetColorIterator->r, TransitionInfo.CurrentColor.r, TransitionInfo.StepValue.r);
        CalculateTransitionStepValue (TransitionInfo.TargetColorIterator->g, TransitionInfo.CurrentColor.g, TransitionInfo.StepValue.g);
        CalculateTransitionStepValue (TransitionInfo.TargetColorIterator->b, TransitionInfo.CurrentColor.b, TransitionInfo.StepValue.b);

        // start timer to hold color
        TransitionInfo.HoldStartTimeMs = millis();
        // DEBUG_V("Transition Timer Has Been started");

        // DEBUG_V(String("   TransitionInfo.HoldStartTimeMs: ") + String(TransitionInfo.HoldStartTimeMs));
        // DEBUG_V(String("   TransitionInfo.StepValue.r: ") + String(TransitionInfo.StepValue.r));
        // DEBUG_V(String("   TransitionInfo.StepValue.g: ") + String(TransitionInfo.StepValue.g));
        // DEBUG_V(String("   TransitionInfo.StepValue.b: ") + String(TransitionInfo.StepValue.b));
        // DEBUG_V(String("           TargetColor.r: ") + String(TransitionInfo.TargetColorIterator->r));
        // DEBUG_V(String("           TargetColor.g: ") + String(TransitionInfo.TargetColorIterator->g));
        // DEBUG_V(String("           TargetColor.b: ") + String(TransitionInfo.TargetColorIterator->b));
        // DEBUG_V(String("TransitionInfo.CurrentColor.r: ") + String(TransitionInfo.CurrentColor.r));
        // DEBUG_V(String("TransitionInfo.CurrentColor.g: ") + String(TransitionInfo.CurrentColor.g));
        // DEBUG_V(String("TransitionInfo.CurrentColor.b: ") + String(TransitionInfo.CurrentColor.b));
    }
    else
    {
        // DEBUG_V("need to calculate next transition color");

        ConditionalIncrementColor(TransitionInfo.TargetColorIterator->r, TransitionInfo.CurrentColor.r, TransitionInfo.StepValue.r);
        ConditionalIncrementColor(TransitionInfo.TargetColorIterator->g, TransitionInfo.CurrentColor.g, TransitionInfo.StepValue.g);
        ConditionalIncrementColor(TransitionInfo.TargetColorIterator->b, TransitionInfo.CurrentColor.b, TransitionInfo.StepValue.b);
    }

    CRGB TempColor;
    TempColor.r = uint8_t(TransitionInfo.CurrentColor.r);
    TempColor.g = uint8_t(TransitionInfo.CurrentColor.g);
    TempColor.b = uint8_t(TransitionInfo.CurrentColor.b);

    // DEBUG_V(String("r: ") + String(TempColor.r));
    // DEBUG_V(String("g: ") + String(TempColor.g));
    // DEBUG_V(String("b: ") + String(TempColor.b));

    setAll(TempColor);

    // DEBUG_END;
    return (EffectDelay / 10);
//    return 1;
} // effectTransition

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectMarquee ()
{
    // DEBUG_START;
    /*
        Chase groups of pixels
        Each group specifies a color and a number of pixels in the group
        seperate number of pixels to advance for each iteration


        Iterate backwards through the array of pixels. 
        Output data for each entry in the array of groups
        Advance the next output pixel forward
        wait
    */

    // DEBUG_V(String("MarqueeTargetColorId: ") + String(MarqueeTargetColorId));

    uint32_t CurrentMarqueePixelLocation = effectMarqueePixelLocation;
    uint32_t NumPixelsToProcess = PixelCount;
    do
    {
        // iterate through the groups until we have processed all of the pixels.
        for(auto CurrentGroup : MarqueueGroupTable)
        {
            uint32_t groupPixelCount = CurrentGroup.NumPixelsInGroup;
            double CurrentBrightness = (EffectReverse) ? CurrentGroup.EndingIntensity : CurrentGroup.StartingIntensity;
            double BrightnessInterval = (double(CurrentGroup.StartingIntensity) - double(CurrentGroup.EndingIntensity))/double(groupPixelCount);

            // now adjust for 100% = 1
            CurrentBrightness /= 100;
            BrightnessInterval /= 100;

            // for each pixel in the group
            for(; (0 != groupPixelCount) && (NumPixelsToProcess); --groupPixelCount, --NumPixelsToProcess)
            {
                CRGB color = CurrentGroup.Color;
                color.r = uint8_t(double(color.r) * CurrentBrightness);
                color.g = uint8_t(double(color.g) * CurrentBrightness);
                color.b = uint8_t(double(color.b) * CurrentBrightness);

                // output the current value
                outputEffectColor (CurrentMarqueePixelLocation, color);

                // advance to the next pixel
                if(EffectReverse)
                {
                    // set the next brightness
                    CurrentBrightness += BrightnessInterval;

                    ++CurrentMarqueePixelLocation;
                    if(PixelCount <= CurrentMarqueePixelLocation)
                    {
                        // wrap bottom of the buffer
                        CurrentMarqueePixelLocation = 0;
                    }
                }
                else // forward
                {
                    // set the next brightness
                    CurrentBrightness -= BrightnessInterval;

                    if(0 == CurrentMarqueePixelLocation)
                    {
                        // wrap one past the top of the buffer
                        CurrentMarqueePixelLocation = PixelCount;
                    }
                    --CurrentMarqueePixelLocation;
                }
            }

            // did we stop due to pixel exhaustion
            if(0 == NumPixelsToProcess)
            {
                break;
            }
        }

    } while (NumPixelsToProcess);

    // advance to the next starting location
    effectMarqueePixelLocation += effectMarqueePixelAdvanceCount;
    if(effectMarqueePixelLocation >= PixelCount)
    {
        // wrap around
        effectMarqueePixelLocation-= PixelCount;
    }

    // DEBUG_END;
    return (EffectDelay / 10);
//    return 1;
} // effectTransition

//-----------------------------------------------------------------------------
void c_InputEffectEngine::CalculateTransitionStepValue(double tc, double cc, double & step)
{
    // DEBUG_START;
    step = (tc - cc) / double(TransitionInfo.StepsToTarget);

    #define MinStepValue (1.0 / double(TransitionInfo.StepsToTarget))
    if(MinStepValue > fabs(step))
    {
        if(step < 0.0)
        {
            step = 0 - MinStepValue;
        }
        else
        {
            step = MinStepValue;
        }
    }

    // DEBUG_V(String("   tc: ") + String(tc));
    // DEBUG_V(String("   cc: ") + String(cc));
    // DEBUG_V(String(" step: ") + String(step));
    // DEBUG_V(String("steps: ") + String(TransitionInfo.StepsToTarget));

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_InputEffectEngine::ConditionalIncrementColor(double tc, double & cc, double step)
{
    // DEBUG_START;

    double originalDiff = fabs(tc-cc);

    if(!ColorHasReachedTarget(tc, cc, step))
    {
        cc = min((cc + step), 255.0);
        cc = max(0.0, cc);
    }

    double NewDiff = fabs(tc-cc);
    if(NewDiff > originalDiff)
    {
        // DEBUG_V("Diff error. Diff is growing instead of shrinking");
        cc = tc;
    }

    // DEBUG_V(String("  tc: ") + String(tc));
    // DEBUG_V(String("  cc: ") + String(cc));
    // DEBUG_V(String("step: ") + String(step));

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
bool c_InputEffectEngine::ColorHasReachedTarget(double tc, double cc, double step)
{
    // DEBUG_START;

    bool response = false;

    double diff = fabs(tc - cc);

    if(diff <= fabs(2.0 * step))
    {
        // DEBUG_V("Single Color has reached target")
        response = true;
    }
/*
    else
    {
        // DEBUG_V(String("  tc: ") + String(tc, 8));
        // DEBUG_V(String("  cc: ") + String(cc, 8));
        // DEBUG_V(String("step: ") + String(step, 8));
        // DEBUG_V(String("diff: ") + String(diff, 8));
    }
*/
    // DEBUG_END;
    return response;

} // ColorHasReachedTarget

//-----------------------------------------------------------------------------
bool c_InputEffectEngine::ColorHasReachedTarget()
{
    // DEBUG_START;

    bool response = ( ColorHasReachedTarget(TransitionInfo.TargetColorIterator->r, TransitionInfo.CurrentColor.r, TransitionInfo.StepValue.r) &&
                      ColorHasReachedTarget(TransitionInfo.TargetColorIterator->g, TransitionInfo.CurrentColor.g, TransitionInfo.StepValue.g) &&
                      ColorHasReachedTarget(TransitionInfo.TargetColorIterator->b, TransitionInfo.CurrentColor.b, TransitionInfo.StepValue.b));

    if(response)
    {
        // DEBUG_V("Color has reached target");
    }

    // DEBUG_END;
    return response;

} // ColorHasReachedTarget

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectBlink ()
{
    // DEBUG_START;
    // The Blink effect uses two "time slots": on, off
    // Using default delay, a complete sequence takes 2s.
    if (EffectStep & 0x1)
    {
        clearAll ();
    }
    else
    {
        setAll (EffectColor);
    }

    ++EffectStep;

    // DEBUG_END;
    return EffectDelay / 1;
} // effectBlink

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectFlash ()
{
    // DEBUG_START;
    // The Flash effect uses 6 "time slots": on, off, on, off, off, off
    // Using default delay, a complete sequence takes 2s.
    // Prevent errors if we come from another effect with more steps
    EffectStep = EffectStep % 6;

    switch (EffectStep)
    {
    case 0:
    case 2:
    {
        setAll (EffectColor);
        break;
    }

    default:
    {
        clearAll ();
        break;
    }
    }

    EffectStep = (1 + EffectStep) % 6;
    // DEBUG_END;
    return EffectDelay / 3;
} // effectFlash

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectFireFlicker ()
{
    // DEBUG_START;
    byte rev_intensity = 6; // more=less intensive, less=more intensive
    byte lum = max (EffectColor.r, max (EffectColor.g, EffectColor.b)) / rev_intensity;

    for (uint16_t i = 0; i < PixelCount; i++)
    {
        uint8_t flicker = random (lum);
        setPixel (i, CRGB{ uint8_t (max (EffectColor.r - flicker, 0)),
                           uint8_t (max (EffectColor.g - flicker, 0)),
                           uint8_t (max (EffectColor.b - flicker, 0)) });
    }
    EffectStep = (1 + EffectStep) % PixelCount;
    // DEBUG_END;
    return EffectDelay / 10;
} // effectFireFlicker

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectLightning ()
{
    // DEBUG_START;
    static byte maxFlashes;
    static int timeslot = EffectDelay / 1000; // 1ms
    int flashPause = 10; // 10ms
    uint16_t ledStart = random (PixelCount);
    uint16_t ledLen = random (1, PixelCount - ledStart);
    uint32_t intensity; // flash intensity

    if (EffectStep % 2)
    {
        // odd steps = clear
        clearAll ();
        if (EffectStep == 1)
        {
            // pause after 1st flash is longer
            flashPause = 130;
        }
        else
        {
            flashPause = random (50, 151); // pause between flashes 50-150ms
        }
    }
    else
    {
        // even steps = flashes
        if (EffectStep == 0)
        {
            // FirstPixelId flash (weaker and longer pause)
            maxFlashes = random (3, 8); // 2-6 follow-up flashes
            intensity = random (128);
        }
        else
        {
            // follow-up flashes (stronger)
            intensity = random (128, 256); // next flashes are stronger
        }

        CRGB temprgb = { uint8_t (uint32_t (EffectColor.r) * intensity / 256),
                         uint8_t (uint32_t (EffectColor.g) * intensity / 256),
                         uint8_t (uint32_t (EffectColor.b) * intensity / 256) };
        setRange (ledStart, ledLen, temprgb);
        flashPause = random (4, 21); // flash duration 4-20ms
    }

    EffectStep++;

    if (EffectStep >= maxFlashes * 2)
    {
        EffectStep = 0;
        flashPause = random (100, 5001); // between 0.1 and 5s
    }
    // DEBUG_END;
    return timeslot * flashPause;
}

//-----------------------------------------------------------------------------
uint16_t c_InputEffectEngine::effectBreathe ()
{
    /*
     * Subtle "breathing" effect, works best with gamma correction on.
     *
     * The average resting respiratory rate of an adult is 12–18 breaths/minute.
     * We use 12 breaths/minute = 5.0s/breath at the default EffectDelay.
     * The tidal volume (~0.5l) is much less than the total lung capacity,
     * so we vary only between 75% and 100% of the set brightness.
     *
     * Per default, this is subtle enough to use with a flood, spot, ceiling or
     * even bedside light. If you want more variation, use the values given
     * below for a 33%/67% variation.
     *
     * In the calculation, we use some constants to make it faster:
     * 0.367879441 is: 1/e
     * 0.106364766 is: 0.25/(e-1/e)  [25% brightness variation, use 0.140401491 for 33%]
     * 0.75 is the offset [75% min brightness, use 0.67 for 67%]
     *
     * See also https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
     * for a nice explanation of the math.
     */
     // sin() is in radians, so 2*PI rad is a full period; compiler should optimize.
    // DEBUG_START;
    float val = (exp (sin (float(millis ()) / (float(EffectDelay) * 5.0) * 2.0 * PI)) - 0.367879441) * 0.106364766 + 0.75;
    setAll ({ uint8_t (EffectColor.r * val),
              uint8_t (EffectColor.g * val),
              uint8_t (EffectColor.b * val) });
    // DEBUG_END;
    return EffectDelay / 40; // update every 25ms
}

//-----------------------------------------------------------------------------
// dCHSV hue 0->360 sat 0->1.0 val 0->1.0
c_InputEffectEngine::dCHSV c_InputEffectEngine::rgb2hsv (CRGB in_int)
{
    dCHSV       out;
    dCRGB       in = { double(in_int.r) / double(255.0), double(in_int.g) / double(255.0), double(in_int.b) / double(255.0) };
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min < in.b ? min : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max > in.b ? max : in.b;

    out.v = max;
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if (max > 0.0) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    }
    else {
        // if max is 0, then r = g = b = 0
        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if (in.r >= max)                           // > is bogus, just keeps compilor happy
        out.h = (in.g - in.b) / delta;        // between yellow & magenta
    else
        if (in.g >= max)
            out.h = 2.0 + (in.b - in.r) / delta;  // between cyan & yellow
        else
            out.h = 4.0 + (in.r - in.g) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if (out.h < 0.0)
        out.h += 360.0;

    return out;
}

//-----------------------------------------------------------------------------
// dCHSV hue 0->360 sat 0->1.0 val 0->1.0
c_InputEffectEngine::CRGB c_InputEffectEngine::hsv2rgb (dCHSV in)
{
    double      hh, p, q, t, ff;
    long        i;
    dCRGB       out;
    CRGB out_int = { 0,0,0 };

    if (in.s <= 0.0)       // < is bogus, just shuts up warnings
    {
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
    }
    else
    {
        hh = in.h;
        if (hh >= 360.0) hh = 0.0;
        hh /= 60.0;
        i = (long)hh;
        ff = hh - i;
        p = in.v * (1.0 - in.s);
        q = in.v * (1.0 - (in.s * ff));
        t = in.v * (1.0 - (in.s * (1.0 - ff)));

        switch (i)
        {
            case 0:
                out.r = in.v;
                out.g = t;
                out.b = p;
                break;

            case 1:
                out.r = q;
                out.g = in.v;
                out.b = p;
                break;

            case 2:
                out.r = p;
                out.g = in.v;
                out.b = t;
                break;

            case 3:
                out.r = p;
                out.g = q;
                out.b = in.v;
                break;

            case 4:
                out.r = t;
                out.g = p;
                out.b = in.v;
                break;

            case 5:
            default:
                out.r = in.v;
                out.g = p;
                out.b = q;
                break;
        }
    }

    out_int.r = min (uint16_t (255), uint16_t (255 * out.r));
    out_int.g = min (uint16_t (255), uint16_t (255 * out.g));
    out_int.b = min (uint16_t (255), uint16_t (255 * out.b));

    return out_int;
}
