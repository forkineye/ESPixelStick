#pragma once
/*
* InputEffectEngine.cpp - Input Management class
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

#include <Arduino.h>
#include "../ESPixelStick.h"
#include "../FileIO.h"
#include "InputCommon.hpp"

class c_InputEffectEngine : public c_InputCommon
{
public:
    c_InputEffectEngine (c_InputMgr::e_InputChannelIds NewInputChannelId,
                         c_InputMgr::e_InputType       NewChannelType,
                         uint8_t                     * BufferStart,
                         uint16_t                      BufferSize);
    ~c_InputEffectEngine ();

    // functions to be provided by the derived class
    void Begin ();                             ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    bool SetMqttConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetMqttConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetMqttEffectList (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject& jsonStatus);
    void Process ();                           ///< Call from loop(),  renders Input data
    void GetDriverName (String  & sDriverName) { sDriverName = "Effects"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint8_t * BufferStart, uint16_t BufferSize);

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

    typedef uint16_t (c_InputEffectEngine::* EffectFunc)(void);
    typedef struct EffectDescriptor_s
    {
        String      name;
        EffectFunc  func;
        const char* htmlid;
        bool        hasColor;
        bool        hasMirror;
        bool        hasReverse;
        bool        hasAllLeds;
        String      wsTCode;
    } EffectDescriptor_t;

private:

    void validateConfiguration ();

    bool HasBeenInitialized = false;

    // CRGB red, green, blue 0->255
    struct CRGB
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    // dCRGB red, green, blue 0->1.0
    struct dCRGB {
        double r;
        double g;
        double b;
    };

    // dCHSV hue 0->360 sat 0->1.0 val 0->1.0
    struct dCHSV {
        double h;
        double s;
        double v;
    };


#define MIN_EFFECT_DELAY 10
#define MAX_EFFECT_DELAY 65535
#define DEFAULT_EFFECT_DELAY 1000

    using timeType = decltype(millis());


    uint32_t EffectWait            = 32;              /* How long to wait for the effect to run again */
    timeType EffectLastRun         = 0;               /* When did the effect last run ? in millis() */
    uint32_t EffectCounter         = 0;               /* Counter for the number of calls to the active effect */
    uint16_t EffectSpeed           = 6;               /* Externally controlled effect speed 1..10 */
    uint16_t EffectDelay           = DEFAULT_EFFECT_DELAY; /* Internal representation of speed */
    bool EffectReverse             = false;           /* Externally controlled effect reverse option */
    bool EffectMirror              = false;           /* Externally controlled effect mirroring (start at center) */
    bool EffectAllLeds             = false;           /* Externally controlled effect all leds = 1st led */
    float EffectBrightness         = 1.0;             /* Externally controlled effect brightness [0, 255] */
    CRGB EffectColor               = { 183, 0, 255 }; /* Externally controlled effect color */

    uint32_t EffectStep            = 0;            /* Shared mutable effect step counter */
    uint16_t PixelCount            = 0;            /* Number of RGB leds (not channels) */

    void setPixel(uint16_t idx,  CRGB color);
    void setRange(uint16_t first, uint16_t len, CRGB color);
    void clearRange(uint16_t first, uint16_t len);
    void setAll(CRGB color);

    CRGB colorWheel(uint8_t pos);
    dCHSV rgb2hsv(CRGB in);
    CRGB hsv2rgb(dCHSV in);

    void setColor (String& NewColor);
    void setEffect (const String & effectName);
    void setBrightness (float brightness);
    void setSpeed (uint16_t speed);
    void setDelay (uint16_t delay);

    void clearAll ();

    const EffectDescriptor_t * ActiveEffect = nullptr;

};
