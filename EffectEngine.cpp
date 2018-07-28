#include <Arduino.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "EffectEngine.h"

// List of all the supported effects and their names
static const EffectDesc EFFECT_LIST[] = {
    { "Solid",          &EffectEngine::effectSolidColor },
    { "Blink",          &EffectEngine::effectBlink },
    { "Flash",          &EffectEngine::effectFlash },
    { "Rainbow",        &EffectEngine::effectRainbowCycle },
    { "Chase",          &EffectEngine::effectChase }
};

// Effect defaults
const char DEFAULT_EFFECT[] = "Solid";
const CRGB DEFAULT_EFFECT_COLOR = { 255, 255, 255 };
const uint8_t DEFAULT_EFFECT_BRIGHTNESS = 127;
const bool DEFAULT_EFFECT_REVERSE = false;
const bool DEFAULT_EFFECT_MIRROR = false;

EffectEngine::EffectEngine() {
    // Initialize with defaults
    setEffect(DEFAULT_EFFECT);
    setColor(DEFAULT_EFFECT_COLOR);
    setBrightness(DEFAULT_EFFECT_BRIGHTNESS);
    setReverse(DEFAULT_EFFECT_REVERSE);
    setMirror(DEFAULT_EFFECT_MIRROR);
}

void EffectEngine::begin(DRIVER* ledDriver, uint16_t ledCount) {
    _ledDriver = ledDriver;
    _ledCount = ledCount;
    _initialized = true;
}

void EffectEngine::run() {
    if (_initialized && _activeEffect) {
        uint32_t now = millis();
        if (now > _effectTimeout) {
            uint16_t delay = (this->*_activeEffect->func)();
            _effectTimeout = now + max((int)delay, MIN_EFFECT_DELAY);
            _effectCounter++;
        }
    }
}

void EffectEngine::setEffect(const char* effectName) {
    const uint8_t effectCount = sizeof(EFFECT_LIST) / sizeof(EffectDesc);
    for (uint8_t effect = 0; effect < effectCount; effect++) {
        if (strcmp(effectName, EFFECT_LIST[effect].name) == 0) {
            if (_activeEffect != &EFFECT_LIST[effect]) {
                _activeEffect = &EFFECT_LIST[effect];
                _effectTimeout = 0;
                _effectCounter = 0;
                _effectStep = 0;
            }
            return;
        }
    }

    _activeEffect = nullptr;
    clearAll();
}

void EffectEngine::setPixel(uint16_t idx,  CRGB color) {
    _ledDriver->setValue(3 * idx + 0, (color.r * _effectBrightness) >> 8);
    _ledDriver->setValue(3 * idx + 1, (color.g * _effectBrightness) >> 8);
    _ledDriver->setValue(3 * idx + 2, (color.b * _effectBrightness) >> 8);
}

void EffectEngine::setAll(CRGB color) {
    for (uint16_t i=0; i < _ledCount; i++) {
        setPixel(i, color);
    }
}

void EffectEngine::clearAll() {
    setAll({0, 0, 0});
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

    _effectStep = ++_effectStep % lc;
    return _effectSpeed / 32;
}

uint16_t EffectEngine::effectRainbowCycle() {
    // calculate only half the pixels if mirroring
    uint16_t lc = _ledCount;
    if (_effectMirror) {
        lc = lc / 2;
    }
    for (uint16_t i=0; i < lc; i++) {
        CRGB color = colorWheel(((i * 256 / lc) + _effectStep) & 0xFF);
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

    _effectStep = ++_effectStep & 0xFF;
    return _effectSpeed / 256;
}

uint16_t EffectEngine::effectBlink() {
    // The Blink effect uses two "time slots": on, off
    // Using default speed, a complete sequence takes 2s.
    if (_effectStep % 2) {
      clearAll();
    } else {
      setAll(_effectColor);
    }

    _effectStep = ++_effectStep % 2;
    return _effectSpeed / 1;
}

uint16_t EffectEngine::effectFlash() {
    // The Flash effect uses 6 "time slots": on, off, on, off, off, off
    // Using default speed, a complete sequence takes 2s.
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

    _effectStep = ++_effectStep % 6;
    return _effectSpeed / 3;
}

