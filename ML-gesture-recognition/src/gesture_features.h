#pragma once

#include <stdint.h>

#include "config.h"

constexpr int INPUT_VERSION = 2;

inline void buildFeatures(const int16_t raw[][N_CHANNELS], uint16_t n,
                          float* out) {
  for (uint8_t t = 0; t < N_STEPS; t++) {
    float pos = (float)t * (float)(n - 1) / (float)(N_STEPS - 1);
    uint16_t i0 = (uint16_t)pos;
    if (i0 > n - 1) i0 = n - 1;
    uint16_t i1 = (i0 + 1 < n) ? i0 + 1 : i0;
    float frac = pos - (float)i0;

    for (uint8_t ch = 0; ch < N_CHANNELS; ch++) {
      float a = (float)raw[i0][ch];
      float b = (float)raw[i1][ch];
      float v = a + frac * (b - a);
      out[t * N_CHANNELS + ch] = v * (ch < 3 ? ACCEL_SCALE : GYRO_SCALE);
    }
  }
}

inline void prepareInputs(float* f) {
  for (uint8_t ch = 0; ch < 3; ch++) {
    float sum = 0;
    for (uint8_t t = 0; t < N_STEPS; t++) sum += f[t * N_CHANNELS + ch];
    float mean = sum / (float)N_STEPS;
    for (uint8_t t = 0; t < N_STEPS; t++) f[t * N_CHANNELS + ch] -= mean;
  }
}
