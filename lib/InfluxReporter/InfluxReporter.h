#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class InfluxReporter {
public:
  bool begin();
  bool send(float uvIndex, float lux, uint32_t uvRaw, uint32_t alsRaw);

private:
  bool connect();
  bool ensureWifi();
  bool resolveHost(const char *hostname, IPAddress &ip);
  void scanAndLog();

  IPAddress serverIp_;
  bool mdnsStarted_ = false;
  bool scannedOnce_ = false;
};