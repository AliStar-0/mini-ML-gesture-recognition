#include <Arduino.h>

#include "capture.h"
#include "config.h"
#include "imu.h"

static float features[N_FEATURES];

void setup() {
  Serial.begin(115200);
  delay(300);
  captureBegin();

  while (!imuBegin()) {
    Serial.println("# MPU6050 not found. Check wiring: SDA->21, SCL->22, VCC->3V3, GND->GND");
    delay(1000);
  }
  Serial.println("# Ready. Hold the button, draw the gesture, release.");
}

void loop() {
  if (!buttonDown()) {
    delay(5);
    return;
  }

  float seconds = 0;
  CaptureResult result = captureGesture(features, &seconds);

  if (result == CaptureResult::SensorError) {
    Serial.println("# Sensor read failed. Check the wiring.");
    return;
  }
  if (result == CaptureResult::TooShort) {
    Serial.println("# Too short, ignored. Keep the button held for the whole gesture.");
    return;
  }

  if (result == CaptureResult::NoMovement) {
    Serial.println("# No movement, ignored. Hold the board in your hand and move it while the button is held.");
    return;
  }

  Serial.print("G");
  for (uint16_t i = 0; i < N_FEATURES; i++) {
    Serial.print(',');
    Serial.print(features[i], 4);
  }
  Serial.println();
  Serial.printf("# Recorded %.2f s\n", seconds);
  if (seconds >= (float)MAX_SAMPLES / SAMPLE_RATE_HZ) {
    Serial.println("# That hit the 3 s limit. Let go of the button as soon as the gesture ends.");
  }
}
