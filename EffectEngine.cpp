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