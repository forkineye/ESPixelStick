#include <Arduino.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "EffectEngine.h"

// List of all the supported effects and their names
static const EffectDesc EFFECT_LIST[] = {
    { "Solid",          &EffectEngine::effectSolidColor },
    { "Rainbow",        &EffectEngine::effectRainbowCycle },
    { "Chase",          &EffectEngine::effectChase }
};

void EffectEngine::begin(DRIVER* ledDriver, uint16_t ledCount) {
    _ledDriver = ledDriver;
    _ledCount = ledCount;
    _initialized = true;
}

void EffectEngine::run() {
    if (_initialized && _activeEffect) {
        uint32_t now = millis();
        if(now > _effectTimeout) {
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
            if (_activeEffect != &EFFECT_LIST[effect])
            {
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
    for(uint16_t i=0; i < _ledCount; i++) {
        setPixel(i, color);
    }
}

void EffectEngine::clearAll() {
    setAll({0, 0, 0});
}

CRGB EffectEngine::colorWheel(uint8_t pos) {
    pos = 255 - pos;
    if(pos < 85) {
        return { 255 - pos * 3, 0, pos * 3};
    } else if(pos < 170) {
        pos -= 85;
        return { 0, pos * 3, 255 - pos * 3 };
    } else {
        pos -= 170;
        return { pos * 3, 255 - pos * 3, 0 };
    }
}

uint16_t EffectEngine::theaterChase(CRGB color1, CRGB color2) {
    uint32_t counter = _effectCounter % 3;
    for(uint16_t i=0; i < _ledCount; i++) {
        if((i % 3) == counter) {
            setPixel(i, color1);
        } else {
            setPixel(i, color2);
        }
    }

    return _effectSpeed / _ledCount;
}

uint16_t EffectEngine::effectSolidColor() {
    for(uint16_t i=0; i < _ledCount; i++) {
        setPixel(i, _effectColor);
    }
    return 32;
}

uint16_t EffectEngine::effectChase() {
    for(uint16_t i=0; i < _ledCount; i++) {
        if (i != _effectStep) {
            setPixel(i, {0, 0, 0});
        }
    }
    setPixel(_effectStep, _effectColor);

    _effectStep = ++_effectStep % _ledCount;
    return _effectSpeed / 32;
}

uint16_t EffectEngine::effectRainbowCycle() {
    for(uint16_t i=0; i < _ledCount; i++) {
        CRGB color = colorWheel(((i * 256 / _ledCount) + _effectStep) & 0xFF);
        setPixel(i, color);
    }

    _effectStep = ++_effectStep & 0xFF;
    return _effectSpeed / 256;
}




uint16_t EffectEngine::effectRainbowChase(void) {
    _effectStep = ++_effectStep & 0xFF;
    return theaterChase(colorWheel(_effectStep), {0, 0, 0});
}

uint16_t EffectEngine::effect0(void) {
    CRGB color = colorWheel((_effectStep * 256 / _ledCount) & 0xFF);

    const uint8_t w = 0;
    const uint8_t r = color.r;
    const uint8_t g = color.g;
    const uint8_t b = color.b;

    float radPerLed = (2.0 * 3.14159) / _ledCount;
    for(uint16_t i=0; i < _ledCount; i++) {
        int lum = map((int)(sin((i + _effectStep) * radPerLed) * 128), -128, 128, 0, 255);
        setPixel(i, {(r * lum) / 256, (g * lum) / 256, (b * lum) / 256});
    }
    _effectStep = ++_effectStep % _ledCount;
    return _effectSpeed / _ledCount;
}

uint16_t EffectEngine::effect1(void) {

    float radPerLed = (2.0 * 3.14159) / _ledCount;
    for(uint16_t i=0; i < _ledCount; i++) {
        CRGB color = colorWheel((_effectStep * 256 / _ledCount) & 0xFF);
        int lum = map((int)(sin((i + _effectStep) * radPerLed) * 128), -128, 128, 0, 255);
        setPixel(i, {(color.r * lum) / 256, (color.g * lum) / 256, (color.b * lum) / 256});
    }
    _effectStep = ++_effectStep % _ledCount;
    return _effectSpeed / _ledCount;
}