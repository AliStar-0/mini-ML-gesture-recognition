#pragma once

#include <math.h>

#include "model.h"

inline int nnPredict(const float* x, float* probs) {
  static float bufA[MODEL_MAX_WIDTH];
  static float bufB[MODEL_MAX_WIDTH];

  const float* in = x;
  const float* w = MODEL_WEIGHTS;
  const float* b = MODEL_BIASES;
  float* out = bufA;

  for (int layer = 0; layer < MODEL_N_LAYERS; layer++) {
    const int nIn = MODEL_LAYER_SIZES[layer];
    const int nOut = MODEL_LAYER_SIZES[layer + 1];
    const bool last = (layer == MODEL_N_LAYERS - 1);
    out = (layer % 2 == 0) ? bufA : bufB;

    for (int o = 0; o < nOut; o++) {
      float acc = b[o];
      const float* row = w + o * nIn;
      for (int i = 0; i < nIn; i++) acc += row[i] * in[i];
      if (!last && acc < 0) acc = 0;
      out[o] = acc;
    }

    w += nIn * nOut;
    b += nOut;
    in = out;
  }

  int best = 0;
  for (int c = 1; c < MODEL_N_CLASSES; c++) {
    if (out[c] > out[best]) best = c;
  }
  float sum = 0;
  for (int c = 0; c < MODEL_N_CLASSES; c++) {
    probs[c] = expf(out[c] - out[best]);
    sum += probs[c];
  }
  for (int c = 0; c < MODEL_N_CLASSES; c++) probs[c] /= sum;

  return best;
}
