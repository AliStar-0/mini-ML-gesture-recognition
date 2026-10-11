#pragma once

#include <stdint.h>

#include "config.h"

enum class CaptureResult {
  Ok,
  TooShort,
  NoMovement,
  SensorError,
};

void captureBegin();

bool buttonDown();

CaptureResult captureGesture(float* features, float* seconds);
