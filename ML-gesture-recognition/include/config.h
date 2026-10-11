#pragma once

#include <stdint.h>

constexpr uint8_t PIN_SDA = 21;
constexpr uint8_t PIN_SCL = 22;
constexpr uint8_t PIN_BUTTON = 0;
constexpr uint8_t PIN_LED = 2;

constexpr uint8_t MPU_ADDR = 0x68;

constexpr uint16_t SAMPLE_RATE_HZ = 100;
constexpr uint16_t MAX_SAMPLES = 300;
constexpr uint16_t MIN_SAMPLES = 30;
constexpr uint8_t N_CHANNELS = 6;

constexpr uint8_t N_STEPS = 50;
constexpr uint16_t N_FEATURES = N_STEPS * N_CHANNELS;

constexpr float ACCEL_SCALE = 1.0f / 4096.0f;
constexpr float GYRO_SCALE = 1.0f / (32.8f * 250.0f);

constexpr float MIN_GYRO_DPS = 60.0f;
constexpr float MIN_ACCEL_SWING_G = 0.30f;

constexpr float MIN_CONFIDENCE = 0.70f;
