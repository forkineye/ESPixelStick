#include <Arduino.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "EffectEngine.h"

extern  config_t        config;

// List of all the supported effects and their names
const EffectDesc EFFECT_LIST[] = {
//                                                                          Mirror     AllLeds
//    name;             func;                             htmlid;      Color;     Reverse     wsTCode

    { "Disabled",     nullptr,                         "t_disabled",     1,    1,    1,    1,  "T0"     },
    { "Solid",        &EffectEngine::effectSolidColor, "t_static",       1,    0,    0,    0,  "T1"     },
    { "Blink",        &EffectEngine::effectBlink,      "t_blink",        1,    0,    0,    0,  "T2"     },
    { "Flash",        &EffectEngine::effectFlash,      "t_flash",        1,    0,    0,    0,  "T3"     },
    { "Rainbow",      &EffectEngine::effectRainbow,    "t_rainbow",      0,    1,    1,    1,  "T5"     },
    { "Chase",        &EffectEngine::effectChase,      "t_chase",        1,    1,    1,    0,  "T4"     },
    { "Fire flicker", &EffectEngine::effectFireFlicker,"t_fireflicker",  1,    0,    0,    0,  "T6"     },
    { "Lightning",    &EffectEngine::effectLightning,  "t_lightning",    1,    0,    0,    0,  "T7"     },
    { "Breathe",      &EffectEngine::effectBreathe,    "t_breathe",      1,    0,    0,    0,  "T8"     }
};

// Effect defaults
#define DEFAULT_EFFECT_NAME "Disabled"
#define DEFAULT_EFFECT_COLOR { 183, 0, 255 }
#define DEFAULT_EFFECT_BRIGHTNESS 1.0
#define DEFAULT_EFFECT_REVERSE false
#define DEFAULT_EFFECT_MIRROR false
#define DEFAULT_EFFECT_ALLLEDS false
#define DEFAULT_EFFECT_SPEED 6

EffectEngine::EffectEngine() {
    // Initialize with defaults
    setFromDefaults();
}

void EffectEngine::setFromDefaults() {
    config.effect_name = DEFAULT_EFFECT_NAME;
    config.effect_color = DEFAULT_EFFECT_COLOR;
    config.effect_brightness = DEFAULT_EFFECT_BRIGHTNESS;
    config.effect_reverse = DEFAULT_EFFECT_REVERSE;
    config.effect_mirror = DEFAULT_EFFECT_MIRROR;
    config.effect_allleds = DEFAULT_EFFECT_ALLLEDS;
    config.effect_speed = DEFAULT_EFFECT_SPEED;
    setFromConfig();
}

void EffectEngine::setFromConfig() {
    // Initialize with defaults
    setEffect(config.effect_name);
    setColor(config.effect_color);
    setBrightness(config.effect_brightness);
    setReverse(config.effect_reverse);
    setMirror(config.effect_mirror);
    setAllLeds(config.effect_allleds);
    setSpeed(config.effect_speed);
}

void EffectEngine::setBrightness(float brightness) {
    _effectBrightness = brightness;
    if (_effectBrightness > 1.0)
        _effectBrightness = 1.0;
    if (_effectBrightness < 0.0)
        _effectBrightness = 0.0;
}

// Yukky maths here. Input speeds from 1..10 get mapped to 17782..100
void EffectEngine::setSpeed(uint16_t speed) {
    _effectSpeed = speed;
    setDelay( pow (10, (10-speed)/4.0 +2 ) );
}

void EffectEngine::setDelay(uint16_t delay) {
    _effectDelay = delay;
    if (_effectDelay < MIN_EFFECT_DELAY)
        _effectDelay = MIN_EFFECT_DELAY;
}

void EffectEngine::begin(DRIVER* ledDriver, uint16_t ledCount) {
    _ledDriver = ledDriver;
    _ledCount = ledCount;
    _initialized = true;
}

void EffectEngine::run() {
    if (_initialized && _activeEffect && _activeEffect->func) {
        if (millis() - _effectLastRun >= _effectWait) {
            _effectLastRun = millis();
            uint16_t wait = (this->*_activeEffect->func)();
            _effectWait = max((int)wait, MIN_EFFECT_DELAY);
            _effectCounter++;
        }
    }
}

void EffectEngine::setEffect(const String effectName) {
    const uint8_t effectCount = sizeof(EFFECT_LIST) / sizeof(EffectDesc);
    for (uint8_t effect = 0; effect < effectCount; effect++) {
        if ( effectName.equalsIgnoreCase(EFFECT_LIST[effect].name) ) {
            if (_activeEffect != &EFFECT_LIST[effect]) {
                _activeEffect = &EFFECT_LIST[effect];
                _effectLastRun = millis();
                _effectWait = MIN_EFFECT_DELAY;
                _effectCounter = 0;
                _effectStep = 0;
            }
            return;
        }
    }

    _activeEffect = nullptr;
    clearAll();
}

int EffectEngine::getEffectCount() {
    return sizeof(EFFECT_LIST) / sizeof(EffectDesc);
}

// find effect info by its index in the table
const EffectDesc* EffectEngine::getEffectInfo(unsigned a) {
    if (a >= sizeof(EFFECT_LIST) / sizeof(EffectDesc))
	a = 0;

    return &EFFECT_LIST[a];
}

// find effect info by its web services Tcode
const EffectDesc* EffectEngine::getEffectInfo(const String TCode) {
    const uint8_t effectCount = sizeof(EFFECT_LIST) / sizeof(EffectDesc);
    for (uint8_t effect = 0; effect < effectCount; effect++) {
        if ( TCode.equalsIgnoreCase(EFFECT_LIST[effect].wsTCode) ) {
            return &EFFECT_LIST[effect];
        }
    }
    return nullptr;
}

bool EffectEngine::isValidEffect(const String effectName) {
    const uint8_t effectCount = sizeof(EFFECT_LIST) / sizeof(EffectDesc);
    for (uint8_t effect = 0; effect < effectCount; effect++) {
        if ( effectName.equalsIgnoreCase(EFFECT_LIST[effect].name) ) {
            return true;
        }
    }
    return false;
}

void EffectEngine::setPixel(uint16_t idx,  CRGB color) {
    _ledDriver->setValue(3 * idx + 0, (uint8_t)(color.r * _effectBrightness) );
    _ledDriver->setValue(3 * idx + 1, (uint8_t)(color.g * _effectBrightness) );
    _ledDriver->setValue(3 * idx + 2, (uint8_t)(color.b * _effectBrightness) );
}

void EffectEngine::setRange(uint16_t first, uint16_t len, CRGB color) {
    for (uint16_t i=first; i < min(uint16_t(first+len), _ledCount); i++) {
        setPixel(i, color);
    }
}

void EffectEngine::clearRange(uint16_t first, uint16_t len) {
    for (uint16_t i=first; i < min(uint16_t(first+len), _ledCount); i++) {
        setPixel(i, {0, 0, 0});
    }
}

void EffectEngine::setAll(CRGB color) {
    setRange(0, _ledCount, color);
}

void EffectEngine::clearAll() {
    clearRange(0, _ledCount);
}

CRGB EffectEngine::colorWheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return { 255 - pos * 3, 0, pos * 3};
    } else if (pos < 170) {
        pos -= 85;
        return { 0, pos * 3, 255 - pos * 3 };
    } else {
        pos -= 170;
        return { pos * 3, 255 - pos * 3, 0 };
    }
}

uint16_t EffectEngine::effectSolidColor() {
    for (uint16_t i=0; i < _ledCount; i++) {
        setPixel(i, _effectColor);
    }
    return 32;
}

uint16_t EffectEngine::effectChase() {
    // calculate only half the pixels if mirroring
    uint16_t lc = _ledCount;
    if (_effectMirror) {
        lc = lc / 2;
    }
    // Prevent errors if we come from another effect with more steps
    // or switch from the upper half of non-mirror to mirror mode
    _effectStep = _effectStep % lc;

    for (uint16_t i=0; i < lc; i++) {
        if (i != _effectStep) {
            if (_effectMirror) {
                setPixel(i + lc, {0, 0, 0});
                setPixel(lc - 1 - i, {0, 0, 0});
            } else {
                setPixel(i, {0, 0, 0});
            }
        }
    }
    uint16_t pixel = _effectStep;
    if (_effectReverse) {
      pixel = lc - 1 - pixel;
    }
    if (_effectMirror) {
        setPixel(pixel + lc, _effectColor);
        setPixel(lc - 1 - pixel, _effectColor);
    } else {
        setPixel(pixel, _effectColor);
    }

    _effectStep = (1+_effectStep) % lc;
    return _effectDelay / 32;
}

uint16_t EffectEngine::effectRainbow() {
    // calculate only half the pixels if mirroring
    uint16_t lc = _ledCount;
    if (_effectMirror) {
        lc = lc / 2;
    }
    for (uint16_t i=0; i < lc; i++) {
//      CRGB color = colorWheel(((i * 256 / lc) + _effectStep) & 0xFF);

        double hue = 0;
        if (_effectAllLeds) {
            hue = _effectStep*360.0d / 256;	// all same colour
        } else {
            hue = 360.0 * (((i * 256 / lc) + _effectStep) & 0xFF) / 255;
        }
        double sat = 1.0;
        double val = 1.0;
        CRGB color = hsv2rgb ( { hue, sat, val } );

        uint16_t pixel = i;
        if (_effectReverse) {
            pixel = lc - 1 - pixel;
        }
        if (_effectMirror) {
            setPixel(pixel + lc, color);
            setPixel(lc - 1 - pixel, color);
        } else {
            setPixel(pixel, color);
        }
    }

    _effectStep = (1+_effectStep) & 0xFF;
    return _effectDelay / 256;
}

uint16_t EffectEngine::effectBlink() {
    // The Blink effect uses two "time slots": on, off
    // Using default delay, a complete sequence takes 2s.
    if (_effectStep % 2) {
      clearAll();
    } else {
      setAll(_effectColor);
    }

    _effectStep = (1+_effectStep) % 2;
    return _effectDelay / 1;
}

uint16_t EffectEngine::effectFlash() {
    // The Flash effect uses 6 "time slots": on, off, on, off, off, off
    // Using default delay, a complete sequence takes 2s.
    // Prevent errors if we come from another effect with more steps
    _effectStep = _effectStep % 6;

    switch (_effectStep) {
      case 0:
      case 2:
        setAll(_effectColor);
        break;
      default:
        clearAll();
    }

    _effectStep = (1+_effectStep) % 6;
    return _effectDelay / 3;
}

uint16_t EffectEngine::effectFireFlicker() {
  byte rev_intensity = 6; // more=less intensive, less=more intensive
  byte lum = max(_effectColor.r, max(_effectColor.g, _effectColor.b)) / rev_intensity;
  for ( int i = 0; i < _ledCount; i++) {
    byte flicker = random(lum);
    setPixel(i, CRGB { max(_effectColor.r - flicker, 0), max(_effectColor.g - flicker, 0), max(_effectColor.b - flicker, 0) });
  }
  _effectStep = (1+_effectStep) % _ledCount;
  return _effectDelay / 10;
}

uint16_t EffectEngine::effectLightning() {
  static byte maxFlashes;
  static int timeslot = _effectDelay / 1000; // 1ms
  int flashPause = 10; // 10ms
  uint16_t ledStart = random(_ledCount);
  uint16_t ledLen = random(1, _ledCount - ledStart);
  byte intensity; // flash intensity

  if (_effectStep % 2) {
    // odd steps = clear
    clearAll();
    if (_effectStep == 1) {
      // pause after 1st flash is longer
      flashPause = 130;
    } else {
      flashPause = random(50, 151); // pause between flashes 50-150ms
    }
  } else {
    // even steps = flashes
    if (_effectStep == 0) {
      // first flash (weaker and longer pause)
      maxFlashes = random(3, 8); // 2-6 follow-up flashes
      intensity = random(128);
    } else {
      // follow-up flashes (stronger)
      intensity = random(128, 256); // next flashes are stronger
    }
    CRGB temprgb = { _effectColor.r*intensity/256, _effectColor.g*intensity/256, _effectColor.b*intensity/256 };
    setRange(ledStart, ledLen, temprgb );
    flashPause = random(4, 21); // flash duration 4-20ms
  }

  _effectStep++;

  if (_effectStep >= maxFlashes * 2) {
    _effectStep = 0;
    flashPause = random(100, 5001); // between 0.1 and 5s
  }
  return timeslot * flashPause;
}

uint16_t EffectEngine::effectBreathe() {
  /*
   * Subtle "breathing" effect, works best with gamma correction on.
   *
   * The average resting respiratory rate of an adult is 12–18 breaths/minute.
   * We use 12 breaths/minute = 5.0s/breath at the default _effectDelay.
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
  float val = (exp(sin(millis()/(_effectDelay*5.0)*2*PI)) - 0.367879441) * 0.106364766 + 0.75;
  setAll({_effectColor.r*val, _effectColor.g*val, _effectColor.b*val});
  return _effectDelay / 40; // update every 25ms
}


// dCHSV hue 0->360 sat 0->1.0 val 0->1.0
dCHSV EffectEngine::rgb2hsv(CRGB in_int)
{
    dCHSV       out;
    dCRGB       in = {in_int.r/255.0d, in_int.g/255.0d, in_int.b/255.0d};
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}


// dCHSV hue 0->360 sat 0->1.0 val 0->1.0
CRGB EffectEngine::hsv2rgb(dCHSV in)
{
    double      hh, p, q, t, ff;
    long        i;
    dCRGB       out;
    CRGB out_int = {0,0,0};

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        out_int = {255*out.r, 255*out.g, 255*out.b};
        return out_int;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
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
    out_int = {255*out.r, 255*out.g, 255*out.b};
    return out_int;
}

