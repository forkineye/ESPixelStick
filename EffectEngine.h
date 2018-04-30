#ifndef EFFECTENGINE_H_
#define EFFECTENGINE_H_

#define MIN_EFFECT_DELAY 10
#define MAX_EFFECT_DELAY 65535
#define DEFAULT_EFFECT_SPEED 1000

#if defined(ESPS_MODE_PIXEL)
    #define DRIVER PixelDriver
#elif defined(ESPS_MODE_SERIAL)
    #define DRIVER SerialDriver
#endif

class EffectEngine;
struct CRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/*
* EffectFunc is the signiture used for all effects. Returns
* the desired delay before the effect should trigger again
*/
typedef uint16_t (EffectEngine::*EffectFunc)(void);
struct EffectDesc {
    const char* name;
    EffectFunc  func;
};

class EffectEngine {

private:

    const EffectDesc* _activeEffect = nullptr;      /* Pointer to the active effect descriptor */
    uint32_t _effectTimeout         = 0;            /* Time after which the effect will run again */
    uint32_t _effectCounter         = 0;            /* Counter for the number of calls to the active effect */
    uint16_t _effectSpeed           = 1024;         /* Externally controlled effect speed [MIN_EFFECT_DELAY, MAX_EFFECT_DELAY]*/
    uint8_t _effectBrightness       = 255;          /* Externally controlled effect brightness [0, 255] */
    CRGB _effectColor               = { };          /* Externally controlled effect color */

    uint32_t _effectStep            = 0;            /* Shared mutable effect step counter */

    bool _initialized               = false;        /* Boolean indicating if the engine is initialzied */
    DRIVER* _ledDriver              = nullptr;      /* Pointer to the active LED driver */
    uint16_t _ledCount              = 0;            /* Number of RGB leds (not channels) */

public:

    void begin(DRIVER* ledDriver, uint16_t ledCount);
    void run();

    const char* getEffect()                 { return _activeEffect ? _activeEffect->name : nullptr; }
    uint32_t getBrightness()                { return _effectBrightness; }
    uint16_t getSpeed()                     { return _effectSpeed; }
    CRGB getColor()                         { return _effectColor; }
    
    void setEffect(const char* effectName);
    void setBrightness(uint8_t brightness)  { _effectBrightness = brightness; }
    void setSpeed(uint16_t speed)           { _effectSpeed = speed; }
    void setColor(CRGB color)               { _effectColor = color; }

public:

    CRGB colorWheel(uint8_t pos);

    uint16_t theaterChase(CRGB color1, CRGB color2);

public:

    // Effect functions
    uint16_t effectSolidColor();
    uint16_t effectRainbowCycle();
    uint16_t effectChase();

    uint16_t effectRainbowChase();
    uint16_t effect0();
    uint16_t effect1();

private:

    void setPixel(uint16_t idx,  CRGB color);
    void setAll(CRGB color);
    void clearAll();
};

#endif
