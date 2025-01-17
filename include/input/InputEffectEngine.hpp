#pragma once
/*
* InputEffectEngine.cpp - Input Management class
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

#include <vector>
#include "InputCommon.hpp"

class c_InputEffectEngine : public c_InputCommon
{
public:
    c_InputEffectEngine (c_InputMgr::e_InputChannelIds NewInputChannelId,
                         c_InputMgr::e_InputType       NewChannelType,
                         uint32_t                        BufferSize);
    virtual ~c_InputEffectEngine ();

    c_InputEffectEngine ();

    // dCRGB red, green, blue 0->1.0
    struct dCRGB {
        double r;
        double g;
        double b;
        dCRGB operator=(dCRGB a)
        {
            r = a.r;
            g = a.g;
            b = a.b;
            return a;
        }
    };

    // CRGB red, green, blue 0->255
    struct CRGB
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    // dCHSV hue 0->360 sat 0->1.0 val 0->1.0
    struct dCHSV
    {
        double h;
        double s;
        double v;
    };

    typedef uint16_t (c_InputEffectEngine::* EffectFunc)(void);

    struct EffectDescriptor_t
    {
        String      name;
        EffectFunc  func;
        const char* htmlid;
        bool        hasColor;
        bool        hasMirror;
        bool        hasReverse;
        bool        hasAllLeds;
        bool        hasWhiteChannel;
        String      wsTCode;
    };

    struct MQTTConfiguration_t
    {
        String  effect;
        bool    mirror;
        bool    allLeds;
        uint8_t brightness;
        bool    whiteChannel;
        CRGB    color;
    };

    struct MarqueeGroup_t
    {
       uint32_t NumPixelsInGroup;
       CRGB     Color;
       uint8_t  StartingIntensity;
       uint8_t  EndingIntensity;
    };

    // functions to be provided by the derived class
    void Begin ();                             ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    void SetMqttConfig (MQTTConfiguration_t& mqttConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetMqttConfig (MQTTConfiguration_t& mqttConfig);   ///< Get the current config used by the driver
    void GetMqttEffectList (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject& jsonStatus);
    void Process   ();
    void Poll ();                              ///< Call from loop(),  renders Input data
    void GetDriverName (String  & sDriverName) { sDriverName = "Effects"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint32_t BufferSize);
    void NextEffect ();
    void ProcessButtonActions(c_ExternalInput::InputValue_t value);

    // Effect functions
    uint16_t effectSolidColor ();
    uint16_t effectRainbow ();
    uint16_t effectChase ();
    uint16_t effectBlink ();
    uint16_t effectFlash ();
    uint16_t effectFireFlicker ();
    uint16_t effectLightning ();
    uint16_t effectBreathe ();
    uint16_t effectNull ();
    uint16_t effectRandom ();
    uint16_t effectTransition ();
    uint16_t effectMarquee ();

private:
#define EFFECTS_TASK_PRIORITY 5

    void validateConfiguration ();

    bool HasBeenInitialized = false;

#define MIN_EFFECT_DELAY 10

    using timeType = decltype(millis());

    uint32_t EffectWait            = 0;               /* How long to wait for the effect to run again */
    uint32_t EffectCounter         = 0;               /* Counter for the number of calls to the active effect */
    uint32_t EffectSpeed           = 6;               /* Externally controlled effect speed 1..10 */
    uint32_t EffectDelay           = 0;               /* Internal representation of speed */
    bool EffectReverse             = false;           /* Externally controlled effect reverse option */
    bool EffectMirror              = false;           /* Externally controlled effect mirroring (start at center) */
    bool EffectAllLeds             = false;           /* Externally controlled effect all leds = 1st led */
    bool EffectWhiteChannel        = false;
    float EffectBrightness         = 1.0;             /* Externally controlled effect brightness [0, 255] */
    CRGB EffectColor               = { 183, 0, 255 }; /* Externally controlled effect color */
    bool StayDark                  = false;
    bool Disabled                  = false;

    uint32_t effectMarqueePixelAdvanceCount = 1;
    uint32_t effectMarqueePixelLocation = 0;

    uint32_t   EffectStep            = 0;            /* Shared mutable effect step counter */
    uint32_t   PixelCount            = 0;            /* Number of RGB leds (not channels) */
    uint32_t   MirroredPixelCount    = 0;            /* Number of RGB leds (not channels) */
    uint8_t    ChannelsPerPixel      = 3;
    uint32_t   PixelOffset           = 0;
    FastTimer  EffectDelayTimer;

    void setPixel(uint16_t idx,  CRGB color);
    void GetPixel (uint16_t pixelId, CRGB & out);
    void setRange(uint16_t first, uint16_t len, CRGB color);
    void clearRange(uint16_t first, uint16_t len);
    void setAll(CRGB color);
    void outputEffectColor (uint16_t pixelId, CRGB outputColor);

    CRGB colorWheel(uint8_t pos);
    dCHSV rgb2hsv(CRGB in);
    CRGB hsv2rgb(dCHSV in);

    void setColor (String& NewColor);
    void setEffect (const String & effectName);
    void setBrightness (float brightness);
    void setSpeed (uint16_t speed);
    void setDelay (uint16_t delay);
    void PollFlash();

    void clearAll ();

    const EffectDescriptor_t * ActiveEffect = nullptr;

    struct Transition_t
    {
        dCRGB       CurrentColor    = {0.0, 0.0, 0.0};
        dCRGB       StepValue       = {2.0, 2.0, 2.0};
        uint32_t    StepsToTarget   = 300; // number of NumStepsToTarget
        uint32_t    TimeAtTargetMs  = 100; // number of milli seconds to stay at the target color.
        uint32_t    HoldStartTimeMs = 0; // time at which transition hold time was started. 0 == off
        std::vector<c_InputEffectEngine::dCRGB>::iterator TargetColorIterator;
    } TransitionInfo;

    bool ColorHasReachedTarget ();
    bool ColorHasReachedTarget (double tc, double cc, double step);
    void ConditionalIncrementColor(double tc, double & cc, double step);
    void CalculateTransitionStepValue(double tc, double cc, double & step);

    struct FlashInfo_t
    {
        bool     Enable           = false;
		uint32_t MinIntensity     = 100;
		uint32_t MaxIntensity     = 100;
		uint32_t MinDelayMS       = 100;
		uint32_t MaxDelayMS       = 5000;
		uint32_t MinDurationMS    = 25;
		uint32_t MaxDurationMS    = 50;
        FastTimer delaytimer;
        FastTimer durationtimer;
    } FlashInfo;

};
