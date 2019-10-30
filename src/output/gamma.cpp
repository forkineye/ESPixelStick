#include <Arduino.h>

uint8_t GAMMA_TABLE[256] = { 0 };

void updateGammaTable(float gammaVal, float briteVal) {
  for (int i = 0; i < 256; i++) {
    GAMMA_TABLE[i] = (uint8_t) min((255.0 * pow(i * briteVal / 255.0, gammaVal) + 0.5), 255.0);
  }
}

