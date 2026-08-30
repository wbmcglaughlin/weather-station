#include "LTR390Sensor.h"

namespace {
constexpr float kGainFactor = 3.0f;      // LTR390_GAIN_3
constexpr float kIntegration = 0.25f;    // LTR390_RESOLUTION_16BIT
constexpr float kUviDivisor =
    (kGainFactor / 18.0f) * (65536.0f / 1048576.0f) * 2300.0f;
}  // namespace

bool LTR390Sensor::begin(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);

  if (!ltr_.begin(&Wire)) {
    return false;
  }

  ltr_.setGain(LTR390_GAIN_3);
  ltr_.setResolution(LTR390_RESOLUTION_16BIT);
  ltr_.setMode(LTR390_MODE_UVS);

  return true;
}

bool LTR390Sensor::waitForData(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!ltr_.newDataAvailable()) {
    if (millis() - start >= timeoutMs) {
      return false;
    }
    delay(10);
  }
  return true;
}

bool LTR390Sensor::sample() {
  ltr_.setMode(LTR390_MODE_UVS);
  delay(30);
  if (!waitForData()) {
    return false;
  }
  uvsRaw_ = ltr_.readUVS();

  ltr_.setMode(LTR390_MODE_ALS);
  delay(30);
  if (!waitForData()) {
    return false;
  }
  alsRaw_ = ltr_.readALS();

  return true;
}

uint32_t LTR390Sensor::uvsRaw() const {
  return uvsRaw_;
}

uint32_t LTR390Sensor::alsRaw() const {
  return alsRaw_;
}

float LTR390Sensor::uvIndex() const {
  return static_cast<float>(uvsRaw_) / kUviDivisor;
}

float LTR390Sensor::lux() const {
  return (static_cast<float>(alsRaw_) * 0.6f) / (kGainFactor * kIntegration);
}