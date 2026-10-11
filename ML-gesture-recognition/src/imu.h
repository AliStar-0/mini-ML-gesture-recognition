#pragma once

#include <stdint.h>

#include "config.h"

bool imuBegin();

enum class ImuStatus {
  Ok,
  Restarted,
  NoAnswer,
};

ImuStatus imuCheck();

bool imuRead(int16_t out[N_CHANNELS]);
