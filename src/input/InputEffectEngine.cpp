/*
* InputEffectEngine.cpp - Code to wrap ESPAsyncE131 for input
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
#include "InputEffectEngine.hpp"

//-----------------------------------------------------------------------------
// Local Structure and Data Definitions
//-----------------------------------------------------------------------------

// List of all the supported effects and their names
static const c_InputEffectEngine::EffectDescriptor_t ListOfEffects[] =
{//                                                                   Mirror     AllLeds
    //    name;                             func;              htmlid;      Color;     Reverse     wsTCode

        // { "Disabled",     nullptr,                             "t_disabled",     1,    1,    1,    1,  "T0"     },
        { "Solid",        &c_InputEffectEngine::effectSolidColor, "t_static",       1,    0,    0,    0,  "T1"     },
        { "Blink",        &c_InputEffectEngine::effectBlink,      "t_blink",        1,    0,    0,    0,  "T2"     },
        { "Flash",        &c_InputEffectEngine::effectFlash,      "t_flash",        1,    0,    0,    0,  "T3"     },
        { "Rainbow",      &c_InputEffectEngine::effectRainbow,    "t_rainbow",      0,    1,    1,    1,  "T5"     },
        { "Chase",        &c_InputEffectEngine::effectChase,      "t_chase",        1,    1,    1,    0,  "T4"     },
        { "Fire flicker", &c_InputEffectEngine::effectFireFlicker,"t_fireflicker",  1,    0,    0,    0,  "T6"     },
        { "Lightning",    &c_InputEffectEngine::effectLightning,  "t_lightning",    1,    0,    0,    0,  "T7"     },
        { "Breathe",      &c_InputEffectEngine::effectBreathe,    "t_breathe",      1,    0,    0,    0,  "T8"     }
};

//-----------------------------------------------------------------------------
c_InputEffectEngine::c_InputEffectEngine (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                          c_InputMgr::e_InputType       NewChannelType,
                                          uint8_t                     * BufferStart,
                                          uint16_t                      BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferStart, BufferSize)
{
    // DEBUG_START;
    // set a default effect
    ActiveEffect = &ListOfEffects[0];

    SetBufferInfo (BufferStart, BufferSize);

    // DEBUG_END;
} // c_InputEffectEngine


//-----------------------------------------------------------------------------
c_InputEffectEngine::c_InputEffectEngine () :
    c_InputCommon (c_InputMgr::e_InputChannelIds::InputChannelId_1, 
        c_InputMgr::e_InputType::InputType_Effects, 
        nullptr, 0)
{
    // DEBUG_START;
    // set a default effect
    ActiveEffect = &ListOfEffects[0];

    SetBufferInfo (nullptr, 0);

    // DEBUG_END;

} // c_InputEffectEngine
//-----------------------------------------------------------------------------
c_InputEffectEngine::~c_InputEffectEngine ()
{
    if (nullptr != InputDataBuffer)
    {
        memset ((void*)InputDataBuffer, 0x0, InputDataBufferSize);
    }
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
    char HexColor[] = "#000000 ";
    sprintf (HexColor, "#%02x%02x%02x", EffectColor.r, EffectColor.g, EffectColor.b);
    // DEBUG_V ("");

    jsonConfig[CN_currenteffect]      = ActiveEffect->name;
    jsonConfig[CN_EffectSpeed]        = EffectSpeed;
    jsonConfig[CN_EffectReverse]      = EffectReverse;
    jsonConfig[CN_EffectMirror]       = EffectMirror;
    jsonConfig[CN_EffectAllLeds]      = EffectAllLeds;
    jsonConfig[CN_EffectBrightness]   = uint32_t(EffectBrightness * 100.0);
    jsonConfig[CN_EffectBlankTime]    = EffectBlankTime;
    jsonConfig[CN_EffectWhiteChannel] = EffectWhiteChannel;
    jsonConfig[CN_EffectColor]        = HexColor;
    // DEBUG_V ("");

    JsonArray EffectsArray = jsonConfig.createNestedArray (CN_effects);
    // DEBUG_V ("");

    for (EffectDescriptor_t currentEffect : ListOfEffects)
    {
        // DEBUG_V ("");
        JsonObject currentJsonEntry = EffectsArray.createNestedObject ();
        currentJsonEntry[CN_name] = currentEffect.name;
    }
    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetMqttEffectList (JsonObject& jsonConfig)
{
    // DEBUG_START;
    JsonArray EffectsArray = jsonConfig.createNestedArray (CN_effect_list);

    for (EffectDescriptor_t currentEffect : ListOfEffects)
    {
        EffectsArray.add(currentEffect.name);
    }
    // DEBUG_END;
} // GetMqttEffectList

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetMqttConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[CN_effect]             = ActiveEffect->name;
    // jsonConfig[CN_speed]      = EffectSpeed;
    // jsonConfig[CN_reverse]    = EffectReverse;
    jsonConfig[CN_mirror]             = EffectMirror;
    jsonConfig[CN_allleds]            = EffectAllLeds;
    jsonConfig[CN_brightness]         = uint16_t(EffectBrightness * 255.0);
    jsonConfig[CN_blanktime]          = EffectBlankTime;
    jsonConfig[CN_EffectWhiteChannel] = EffectWhiteChannel;

    // color needs a bit of reprocessing
    JsonObject color = jsonConfig.createNestedObject (CN_color);
    color[CN_r] = EffectColor.r;
    color[CN_g] = EffectColor.g;
    color[CN_b] = EffectColor.b;

    // DEBUG_END;

} // GetMqttConfig

//-----------------------------------------------------------------------------
void c_InputEffectEngine::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject Status = jsonStatus.createNestedObject (F ("effects"));
    Status[CN_currenteffect] = ActiveEffect->name;
    Status[CN_id]            = InputChannelId;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputEffectEngine::NextEffect ()
{
    // DEBUG_START;

    // DEBUG_V ("Find the current effect");
    int CurrentEffectIndex = 0;
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
    if (String ("Breathe") == ActiveEffect->name)
    {
        // DEBUG_V ("Wrap to first effect");
        CurrentEffectIndex = 0;
    }

    // DEBUG_V (String ("CurrentEffectIndex: ") + String(CurrentEffectIndex));
    setEffect (ListOfEffects[CurrentEffectIndex].name);
    LOG_PORT.println (String (F ("Setting new effect: ")) + ActiveEffect->name);
    // DEBUG_V (String ("ActiveEffect->name: ") + ActiveEffect->name);

    // DEBUG_END;
} // NextEffect

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

        if (millis () < EffectBlankEnd)
        {
            break;
        }
        // DEBUG_V ("");

        if (millis () < (EffectLastRun + EffectWait))
        {
            break;
        }

        // DEBUG_V ("Update output");
        EffectLastRun = millis ();
        uint16_t wait = (this->*ActiveEffect->func)();
        EffectWait = max ((int)wait, MIN_EFFECT_DELAY);
        EffectCounter++;

    } while (false);

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputEffectEngine::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    // DEBUG_START;

    InputDataBuffer     = BufferStart;
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
boolean c_InputEffectEngine::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;
    String effectName;
    String effectColor;

    setFromJSON (EffectSpeed,        jsonConfig, CN_EffectSpeed);
    setFromJSON (EffectReverse,      jsonConfig, CN_EffectReverse);
    setFromJSON (EffectMirror,       jsonConfig, CN_EffectMirror);
    setFromJSON (EffectAllLeds,      jsonConfig, CN_EffectAllLeds);
    setFromJSON (EffectBrightness,   jsonConfig, CN_EffectBrightness);
    setFromJSON (EffectBlankTime,    jsonConfig, CN_EffectBlankTime);
    setFromJSON (EffectWhiteChannel, jsonConfig, CN_EffectWhiteChannel);
    setFromJSON (effectName,         jsonConfig, CN_currenteffect);
    setFromJSON (effectColor,        jsonConfig, CN_EffectColor);
    // DEBUG_V (String ("effectColor: ") + effectColor);

    EffectBrightness /= 100.0;

    SetBufferInfo (InputDataBuffer, InputDataBufferSize);

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
boolean c_InputEffectEngine::SetMqttConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;
    boolean response = false;
    String effectName;

    response |= setFromJSON (EffectSpeed,        jsonConfig, CN_speed);
    response |= setFromJSON (EffectReverse,      jsonConfig, CN_reverse);
    response |= setFromJSON (EffectMirror,       jsonConfig, CN_mirror);
    response |= setFromJSON (EffectAllLeds,      jsonConfig, CN_allleds);
    response |= setFromJSON (EffectBlankTime,    jsonConfig, CN_blanktime);

    uint16_t tempBrightness = uint8_t(EffectBrightness * 255.0);
    response |= setFromJSON (tempBrightness,   jsonConfig, CN_brightness);
    EffectBrightness = float(tempBrightness) / 255.0;

    response |= setFromJSON (EffectWhiteChannel, jsonConfig, CN_EffectWhiteChannel);
    response |= setFromJSON (effectName,         jsonConfig, CN_effect);

    SetBufferInfo (InputDataBuffer, InputDataBufferSize);

    if (jsonConfig.containsKey (CN_color))
    {
        JsonObject JsonColor = jsonConfig[CN_color];
        response |= setFromJSON (EffectColor.r, JsonColor, CN_r);
        response |= setFromJSON (EffectColor.g, JsonColor, CN_g);
        response |= setFromJSON (EffectColor.b, JsonColor, CN_b);
    }

    validateConfiguration ();

    setEffect (effectName);

    // DEBUG_END;
    return response;
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
    // DEBUG_END;
} // setDelay

//-----------------------------------------------------------------------------
void c_InputEffectEngine::ResetBlankTimer ()
{
    // DEBUG_START;
    EffectBlankEnd = millis () + (EffectBlankTime * 1000);
    // DEBUG_END;

} // ResetBlankTimer

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
            if (!ActiveEffect->name.equalsIgnoreCase(currentEffect.name))
            {
                // DEBUG_V ("Starting Effect");
                ActiveEffect  = &ListOfEffects[EffectIndex];
                EffectLastRun = millis ();
                EffectWait    = MIN_EFFECT_DELAY;
                EffectCounter = 0;
                EffectStep    = 0;
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

    uint32_t intValue = strtoul(NewColor.substring (1).c_str(), nullptr, 16);
    // DEBUG_V (String ("intValue: ") + String (intValue, 16));

    EffectColor.r = uint8_t ((intValue >> 16) & 0xFF);
    EffectColor.g = uint8_t ((intValue >>  8) & 0xFF);
    EffectColor.b = uint8_t ((intValue >>  0) & 0xFF);

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
        uint8_t* pInputDataBuffer = &InputDataBuffer[ChannelsPerPixel * pixelId];

        // DEBUG_V (String ("EffectBrightness: ") + String (EffectBrightness));
        // DEBUG_V (String ("color.r: ") + String (color.r));
        // DEBUG_V (String ("color.g: ") + String (color.g));
        // DEBUG_V (String ("color.b: ") + String (color.b));

        pInputDataBuffer[0] = color.r * EffectBrightness;
        pInputDataBuffer[1] = color.g * EffectBrightness;
        pInputDataBuffer[2] = color.b * EffectBrightness;
        pInputDataBuffer[3] = 0; // no white data

        // DEBUG_V (String ("pInputDataBuffer[0]: ") + String (pInputDataBuffer[0]));
        // DEBUG_V (String ("pInputDataBuffer[1]: ") + String (pInputDataBuffer[1]));
        // DEBUG_V (String ("pInputDataBuffer[2]: ") + String (pInputDataBuffer[2]));
    }

    // DEBUG_END;

} // setPixel

//-----------------------------------------------------------------------------
void c_InputEffectEngine::setRange (uint16_t FirstPixelId, uint16_t NumberOfPixels, CRGB color)
{
    for (uint16_t i = FirstPixelId; i < min (uint16_t (FirstPixelId + NumberOfPixels), PixelCount); i++)
    {
        setPixel (i, color);
    }
} // setRange

//-----------------------------------------------------------------------------
void c_InputEffectEngine::clearRange (uint16_t FirstPixelId, uint16_t NumberOfPixels)
{
    for (uint16_t i = FirstPixelId; i < min (uint16_t (FirstPixelId + NumberOfPixels), PixelCount); i++)
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
        Response = { uint8_t(255 - pos * 3), 0, uint8_t(pos * 3) };
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
   //  DEBUG_START;

    uint16_t NumPixels = MirroredPixelCount;

    if (EffectReverse)
    {
        pixelId = NumPixels - 1 - pixelId;
    }

    if (EffectMirror)
    {
        setPixel ((NumPixels - 1)  - pixelId,                outputColor);
        setPixel (((NumPixels    ) + pixelId) - PixelOffset, outputColor);
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
    // DEBUG_START;
    // calculate only half the pixels if mirroring
    uint16_t lc = MirroredPixelCount;

    // DEBUG_V (String ("MirroredPixelCount: ") + String (MirroredPixelCount));
    // DEBUG_V (String ("        EffectStep: ") + String (EffectStep));

    for (uint16_t i = 0; i < lc; i++)
    {
        // CRGB color = colorWheel(((i * 256 / lc) + EffectStep) & 0xFF);

        double hue = 0;
        if (EffectAllLeds)
        {
            hue = EffectStep * 360.0d / 256.0;	// all same colour
        }
        else
        {
            hue = 360.0 * (((i * 256 / lc) + EffectStep) & 0xFF) / 255;
        }
        double sat = 1.0;
        double val = 1.0;
        CRGB color = hsv2rgb ({ hue, sat, val });

        outputEffectColor (i, color);
    }

    EffectStep = (1 + EffectStep) & 0xFF;

    // DEBUG_END;
    return EffectDelay / 256;
} // effectRainbow

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
        setPixel (i, CRGB{ uint8_t(max (EffectColor.r - flicker, 0)), 
                           uint8_t(max (EffectColor.g - flicker, 0)),
                           uint8_t(max (EffectColor.b - flicker, 0)) });
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
uint16_t c_InputEffectEngine::effectBreathe () {
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
    float val = (exp (sin (millis () / (EffectDelay * 5.0) * 2 * PI)) - 0.367879441) * 0.106364766 + 0.75;
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
    dCRGB       in = { in_int.r / 255.0d, in_int.g / 255.0d, in_int.b / 255.0d };
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

    if (in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        out_int = { uint8_t(255 * out.r), uint8_t(255 * out.g), uint8_t(255 * out.b) };
        return out_int;
    }
    hh = in.h;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch (i) {
    case 0:
        out.r = in.v;
        out.g = uint8_t (t);
        out.b = uint8_t (p);
        break;
    case 1:
        out.r = uint8_t (q);
        out.g = in.v;
        out.b = uint8_t (p);
        break;
    case 2:
        out.r = uint8_t (p);
        out.g = in.v;
        out.b = uint8_t (t);
        break;

    case 3:
        out.r = uint8_t (p);
        out.g = uint8_t (q);
        out.b = in.v;
        break;
    case 4:
        out.r = uint8_t (t);
        out.g = uint8_t (p);
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = uint8_t (p);
        out.b = uint8_t (q);
        break;
    }
    out_int = { uint8_t (255 * out.r), uint8_t (255 * out.g), uint8_t (255 * out.b) };
    return out_int;
}

