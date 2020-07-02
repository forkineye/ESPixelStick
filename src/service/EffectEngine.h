#pragma once

#define MIN_EFFECT_DELAY 10
#define MAX_EFFECT_DELAY 65535
#define DEFAULT_EFFECT_DELAY 1000

//TODO: Make more generic
//#if defined(ESPS_MODE_PIXEL)
//    #define DRIVER PixelDriver
//#elif defined(ESPS_MODE_SERIAL)
//    #define DRIVER SerialDriver
//#endif


/*
Code ripped from main sketch

config_t:
    // Effects
    String effect_name;
    CRGB effect_color;
    float effect_brightness;
    uint16_t effect_speed;	     ///< 1..10 for web UI and MQTT
    bool effect_reverse;
    bool effect_mirror;
    bool effect_allleds;
    bool effect_startenabled;
    bool effect_idleenabled;
    uint16_t effect_idletimeout;

setup():
#if defined (ESPS_MODE_PIXEL)
    pixels.setPin(DATA_PIN);
    updateConfig();
    // Do one effects cycle as early as possible
    if (config.ds == DataSource::WEB) {
        effects.run();
    }
    // set the effect idle timer
    idleTicker.attach(config.effect_idletimeout, idleTimeout);
    pixels.show();
#else
    updateConfig();
    // Do one effects cycle as early as possible
    if (config.ds == DataSource::WEB) {
        effects.run();
    }
    // set the effect idle timer
    idleTicker.attach(config.effect_idletimeout, idleTimeout);

    serial.show();
#endif

validateConfig():
    if (config.effect_speed < 1)
        config.effect_speed = 1;
    if (config.effect_speed > 10)
        config.effect_speed = 10;

    if (config.effect_brightness > 1.0)
        config.effect_brightness = 1.0;
    if (config.effect_brightness < 0.0)
        config.effect_brightness = 0.0;

    if (config.effect_idletimeout == 0) {
        config.effect_idletimeout = 10;
        config.effect_idleenabled = false;
    }

    if (config.effect_startenabled) {
        if (effects.isValidEffect(config.effect_name)) {
            effects.setEffect(config.effect_name);

            if ( !config.effect_name.equalsIgnoreCase("disabled")
              && !config.effect_name.equalsIgnoreCase("view") ) {
                config.ds = DataSource::WEB;
            }

        }
    }

loadConfig():
    effects.setFromDefaults();
    effects.setFromConfig();

serializeConfig():
    // Effects
    JsonObject _effects = json.createNestedObject(F("effects"));
    _effects["name"] = config.effect_name;

    _effects["mirror"] = config.effect_mirror;
    _effects["allleds"] = config.effect_allleds;
    _effects["reverse"] = config.effect_reverse;
    _effects["speed"] = config.effect_speed;
    _effects["brightness"] = config.effect_brightness;

    _effects["r"] = config.effect_color.r;
    _effects["g"] = config.effect_color.g;
    _effects["b"] = config.effect_color.b;

    _effects["brightness"] = config.effect_brightness;
    _effects["startenabled"] = config.effect_startenabled;
    _effects["idleenabled"] = config.effect_idleenabled;
    _effects["idletimeout"] = config.effect_idletimeout;



void dsEffectConfig(const JsonObject &json) {
    // Effects
    if (json.containsKey("effects")) {
        JsonObject effectsJson = json["effects"];
        config.effect_name = effectsJson["name"].as<String>();
        config.effect_mirror = effectsJson["mirror"];
        config.effect_allleds = effectsJson["allleds"];
        config.effect_reverse = effectsJson["reverse"];
        if (effectsJson.containsKey("speed"))
            config.effect_speed = effectsJson["speed"];
        config.effect_color = { effectsJson["r"], effectsJson["g"], effectsJson["b"] };
        if (effectsJson.containsKey("brightness"))
            config.effect_brightness = effectsJson["brightness"];
        config.effect_startenabled = effectsJson["startenabled"];
        config.effect_idleenabled = effectsJson["idleenabled"];
        config.effect_idletimeout = effectsJson["idletimeout"];
    }
    else
    {
        LOG_PORT.println("No effect settings found.");
    }
}

void idleTimeout() {
   idleTicker.attach(config.effect_idletimeout, idleTimeout);
    if ( (config.effect_idleenabled) && (config.ds == DataSource::E131 || config.ds == DataSource::ZCPP) ) {
        config.ds = DataSource::IDLEWEB;
        effects.setFromConfig();
    }
}

loop():
idleTicker.attach(config.effect_idletimeout, idleTimeout);
if ((config.ds == DataSource::WEB)
    || (config.ds == DataSource::IDLEWEB)
    || (config.ds == DataSource::ZCPP)
    || (config.ds == DataSource::DDP)
    || (config.ds == DataSource::MQTT)) {
        effects.run();
}


*/


class EffectEngine;
// CRGB red, green, blue 0->255
struct CRGB {
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

/*
* EffectFunc is the signature used for all effects. Returns
* the desired delay before the effect should trigger again
*/
typedef uint16_t (EffectEngine::*EffectFunc)(void);
struct EffectDesc {
    String      name;
    EffectFunc  func;
    const char* htmlid;
    bool        hasColor;
    bool        hasMirror;
    bool        hasReverse;
    bool        hasAllLeds;
    String      wsTCode;
};

class EffectEngine {

private:
    using timeType = decltype(millis());

    const EffectDesc* _activeEffect = nullptr;      /* Pointer to the active effect descriptor */
    uint32_t _effectWait            = 0;            /* How long to wait for the effect to run again */
    timeType _effectLastRun         = 0;            /* When did the effect last run ? in millis() */
    uint32_t _effectCounter         = 0;            /* Counter for the number of calls to the active effect */
    uint16_t _effectSpeed           = 6;            /* Externally controlled effect speed 1..10 */
    uint16_t _effectDelay           = 1000;         /* Internal representation of speed */
    bool _effectReverse             = false;        /* Externally controlled effect reverse option */
    bool _effectMirror              = false;        /* Externally controlled effect mirroring (start at center) */
    bool _effectAllLeds             = false;        /* Externally controlled effect all leds = 1st led */
    float _effectBrightness         = 1.0;          /* Externally controlled effect brightness [0, 255] */
    CRGB _effectColor               = {0,0,0};      /* Externally controlled effect color */

    uint32_t _effectStep            = 0;            /* Shared mutable effect step counter */

    bool _initialized               = false;        /* Boolean indicating if the engine is initialzied */
//    DRIVER* _ledDriver              = nullptr;      /* Pointer to the active LED driver */
    uint16_t _ledCount              = 0;            /* Number of RGB leds (not channels) */

public:
    EffectEngine();

//    void begin(DRIVER* ledDriver, uint16_t ledCount);
    void run();

    String getEffect()                      { return _activeEffect ? _activeEffect->name : ""; }
    bool getReverse()                       { return _effectReverse; }
    bool getMirror()                        { return _effectMirror; }
    bool getAllLeds()                       { return _effectAllLeds; }
    float getBrightness()                   { return _effectBrightness; }
    uint16_t getDelay()                     { return _effectDelay; }
    uint16_t getSpeed()                     { return _effectSpeed; }
    CRGB getColor()                         { return _effectColor; }

    int getEffectCount();
    const EffectDesc* getEffectInfo(unsigned a);
    const EffectDesc* getEffectInfo(String s);
    void setFromConfig();
    void setFromDefaults();

    bool isValidEffect(const String effectName);
    void setEffect(const String effectName);
    void setReverse(bool reverse)           { _effectReverse = reverse; }
    void setMirror(bool mirror)             { _effectMirror = mirror; }
    void setAllLeds(bool allleds)           { _effectAllLeds = allleds; }
    void setBrightness(float brightness);
    void setSpeed(uint16_t speed);
    void setDelay(uint16_t delay);
    void setColor(CRGB color)               { _effectColor = color; }

    // Effect functions
    uint16_t effectSolidColor();
    uint16_t effectRainbow();
    uint16_t effectChase();
    uint16_t effectBlink();
    uint16_t effectFlash();
    uint16_t effectFireFlicker();
    uint16_t effectLightning();
    uint16_t effectBreathe();
    uint16_t effectNull();
    void clearAll();

private:

    void setPixel(uint16_t idx,  CRGB color);
    void setRange(uint16_t first, uint16_t len, CRGB color);
    void clearRange(uint16_t first, uint16_t len);
    void setAll(CRGB color);

    CRGB colorWheel(uint8_t pos);
    dCHSV rgb2hsv(CRGB in);
    CRGB hsv2rgb(dCHSV in);
};
