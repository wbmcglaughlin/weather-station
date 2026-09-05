#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class InfluxReporter {
public:
  bool begin();
  bool send(float uvIndex, float lux, uint32_t uvRaw, uint32_t alsRaw);
  bool sendBme280(float temperature, float humidity, float pressure);
  bool sendRainGauge(uint32_t tips, uint32_t totalTips, float rainfallMm,
                     float totalRainfallMm);

private:
  bool post(const String &body);
  bool connect();
  bool ensureWifi();
  bool resolveHost(const char *hostname, IPAddress &ip);
  void scanAndLog();

  IPAddress serverIp_;
  bool mdnsStarted_ = false;
  bool scannedOnce_ = false;
};