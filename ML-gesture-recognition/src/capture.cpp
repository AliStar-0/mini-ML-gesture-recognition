#include "capture.h"

#include <Arduino.h>
#include <math.h>

#include "gesture_features.h"
#include "imu.h"

namespace {

int16_t raw[MAX_SAMPLES][N_CHANNELS];

constexpr uint8_t RELEASE_SAMPLES = 3;

bool hasMovement(uint16_t n) {
  float gyroPeak = 0;
  float accelMin = 1e9f;
  float accelMax = 0;
  for (uint16_t i = 0; i < n; i++) {
    for (uint8_t ch = 3; ch < 6; ch++) {
      float dps = fabsf((float)raw[i][ch]) * GYRO_SCALE * 250.0f;
      if (dps > gyroPeak) gyroPeak = dps;
    }
    float ax = raw[i][0] * ACCEL_SCALE;
    float ay = raw[i][1] * ACCEL_SCALE;
    float az = raw[i][2] * ACCEL_SCALE;
    float g = sqrtf(ax * ax + ay * ay + az * az);
    if (g < accelMin) accelMin = g;
    if (g > accelMax) accelMax = g;
  }
  return gyroPeak >= MIN_GYRO_DPS || (accelMax - accelMin) >= MIN_ACCEL_SWING_G;
}

}

void captureBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
}

bool buttonDown() { return digitalRead(PIN_BUTTON) == LOW; }

CaptureResult captureGesture(float* features, float* seconds) {
  const uint32_t periodUs = 1000000UL / SAMPLE_RATE_HZ;

  ImuStatus status = imuCheck();
  if (status == ImuStatus::NoAnswer) {
    while (buttonDown()) delay(10);
    return CaptureResult::SensorError;
  }
  if (status == ImuStatus::Restarted) {
    Serial.println("# Sensor had restarted (loose wire?). It is set up again.");
  }

  digitalWrite(PIN_LED, HIGH);

  uint16_t n = 0;
  uint8_t upCount = 0;
  bool sensorOk = true;
  uint32_t next = micros();

  while (n < MAX_SAMPLES && upCount < RELEASE_SAMPLES) {
    if (!imuRead(raw[n])) {
      sensorOk = false;
      break;
    }
    n++;
    upCount = buttonDown() ? 0 : upCount + 1;

    next += periodUs;
    int32_t waitUs = (int32_t)(next - micros());
    if (waitUs > 0) delayMicroseconds((uint32_t)waitUs);
  }

  digitalWrite(PIN_LED, LOW);

  while (buttonDown()) delay(10);

  if (!sensorOk) return CaptureResult::SensorError;

  if (upCount >= RELEASE_SAMPLES) n -= RELEASE_SAMPLES;

  if (seconds) *seconds = (float)n / (float)SAMPLE_RATE_HZ;
  if (n < MIN_SAMPLES) return CaptureResult::TooShort;
  if (!hasMovement(n)) return CaptureResult::NoMovement;

  buildFeatures(raw, n, features);
  return CaptureResult::Ok;
}
