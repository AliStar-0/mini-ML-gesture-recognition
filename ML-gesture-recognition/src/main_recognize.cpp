#include <Arduino.h>

#include "capture.h"
#include "config.h"
#include "gesture_features.h"
#include "imu.h"
#include "nn.h"

static_assert(MODEL_N_INPUTS == N_FEATURES,
              "model.h does not match config.h. Re-record and retrain.");

static_assert(MODEL_INPUT_VERSION == INPUT_VERSION,
              "model.h was trained by an older train.py. Run train.bat again.");

static float features[N_FEATURES];
static float probs[MODEL_N_CLASSES];

static void onGesture(int id, const char* label) {
  (void)label;

  for (int i = 0; i <= id; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(120);
    digitalWrite(PIN_LED, LOW);
    delay(120);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  captureBegin();

  while (!imuBegin()) {
    Serial.println("MPU6050 not found. Check wiring: SDA->21, SCL->22, VCC->3V3, GND->GND");
    delay(1000);
  }

#if MODEL_IS_PLACEHOLDER
  Serial.println("WARNING: include/model.h is still the placeholder.");
  Serial.println("Record data and run tools/train.py, then upload again.");
#endif

  Serial.print("Gestures:");
  for (int c = 0; c < MODEL_N_CLASSES; c++) {
    Serial.print(' ');
    Serial.print(MODEL_LABELS[c]);
  }
  Serial.println();
  Serial.println("Ready. Hold the button, draw a gesture, release.");
}

void loop() {
  if (!buttonDown()) {
    delay(5);
    return;
  }

  float seconds = 0;
  CaptureResult result = captureGesture(features, &seconds);

  if (result == CaptureResult::SensorError) {
    Serial.println("Sensor read failed. Check the wiring.");
    return;
  }
  if (result == CaptureResult::TooShort) {
    Serial.println("Too short, ignored.");
    return;
  }

  if (result == CaptureResult::NoMovement) {
    Serial.println("No movement, ignored.");
    return;
  }

  uint32_t t0 = micros();
  prepareInputs(features);
  int id = nnPredict(features, probs);
  uint32_t inferenceUs = micros() - t0;

  if (probs[id] < MIN_CONFIDENCE) {
    Serial.printf("Unknown gesture (best guess %s, %.0f%%)\n",
                  MODEL_LABELS[id], probs[id] * 100.0f);
    return;
  }

  Serial.printf("Gesture: %s (%.0f%%)  inference %lu us\n", MODEL_LABELS[id],
                probs[id] * 100.0f, (unsigned long)inferenceUs);
  onGesture(id, MODEL_LABELS[id]);
}
