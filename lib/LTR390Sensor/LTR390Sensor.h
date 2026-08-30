#pragma once

#include <Arduino.h>
#include <Adafruit_LTR390.h>
#include <Wire.h>

class LTR390Sensor {
public:
  bool begin(uint8_t sda, uint8_t scl);
  bool sample();

  uint32_t uvsRaw() const;
  uint32_t alsRaw() const;
  float uvIndex() const;
  float lux() const;

private:
  bool waitForData(uint32_t timeoutMs = 500);

  Adafruit_LTR390 ltr_;
  uint32_t uvsRaw_ = 0;
  uint32_t alsRaw_ = 0;
};