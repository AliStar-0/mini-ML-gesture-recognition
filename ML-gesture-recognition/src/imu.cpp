#include "imu.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t REG_SMPLRT_DIV = 0x19;
constexpr uint8_t REG_CONFIG = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t PWR_AWAKE = 0x01;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

}

static bool configure() {
  if (!writeReg(REG_PWR_MGMT_1, PWR_AWAKE)) return false;
  delay(100);

  bool ok = true;
  ok &= writeReg(REG_CONFIG, 0x03);
  ok &= writeReg(REG_SMPLRT_DIV, 9);
  ok &= writeReg(REG_GYRO_CONFIG, 0x10);
  ok &= writeReg(REG_ACCEL_CONFIG, 0x10);

  delay(60);
  return ok;
}

bool imuBegin() {
  Wire.begin((int)PIN_SDA, (int)PIN_SCL);
  Wire.setClock(400000);
  return configure();
}

ImuStatus imuCheck() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_PWR_MGMT_1);
  if (Wire.endTransmission(false) != 0) return ImuStatus::NoAnswer;
  if (Wire.requestFrom((int)MPU_ADDR, 1) != 1) return ImuStatus::NoAnswer;

  if (Wire.read() == PWR_AWAKE) return ImuStatus::Ok;
  return configure() ? ImuStatus::Restarted : ImuStatus::NoAnswer;
}

bool imuRead(int16_t out[N_CHANNELS]) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)MPU_ADDR, 14) != 14) return false;

  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (int16_t)((hi << 8) | lo);
  }

  out[0] = v[0];
  out[1] = v[1];
  out[2] = v[2];

  out[3] = v[4];
  out[4] = v[5];
  out[5] = v[6];
  return true;
}
