#include "InfluxReporter.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#include "config.h"

namespace {
constexpr uint32_t kConnectTimeoutMs = 15000;

struct Network {
  const char *ssid;
  const char *pass;
};

const Network kNetworks[] = {
    {WIFI_SSID, WIFI_PASS},
    {WIFI_SSID_ALT, WIFI_PASS_ALT},
};
}  // namespace

bool InfluxReporter::resolveHost(const char *hostname, IPAddress &ip) {
  IPAddress resolved = MDNS.queryHost(hostname, 3000);
  if (static_cast<uint32_t>(resolved) == 0) {
    return false;
  }
  ip = resolved;
  return true;
}

void InfluxReporter::scanAndLog() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  Serial.printf("[influx] startup scan: %d network(s)\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("[influx]   '%s' BSSID=%s ch=%d RSSI=%d dBm\n",
                  WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(),
                  WiFi.channel(i), WiFi.RSSI(i));
  }
}

bool InfluxReporter::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  delay(200);

  for (const auto &net : kNetworks) {
    Serial.printf("[influx]   connecting to '%s'...\n", net.ssid);
    WiFi.disconnect();
    delay(200);
    WiFi.begin(net.ssid, net.pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[influx]   connected to '%s'\n", net.ssid);
      return true;
    }

    Serial.printf("[influx]   '%s' failed (status=%d)\n", net.ssid,
                  (int)WiFi.status());
  }

  int n = WiFi.scanNetworks();
  Serial.printf("[influx] scan found %d network(s):\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("[influx]   '%s' RSSI=%d dBm\n", WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i));
  }

  return false;
}

bool InfluxReporter::connect() {
  if (!scannedOnce_) {
    scannedOnce_ = true;
    scanAndLog();
  }

  if (!ensureWifi()) {
    return false;
  }

  Serial.printf("[influx] WiFi connected, IP %s, AP %s, RSSI %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.BSSIDstr().c_str(),
                WiFi.RSSI());

  if (!mdnsStarted_) {
    if (MDNS.begin("esp32-ltr390")) {
      mdnsStarted_ = true;
    } else {
      Serial.println("[influx] mDNS init failed");
    }
  }

  if (static_cast<uint32_t>(serverIp_) == 0) {
    if (mdnsStarted_ && resolveHost(INFLUX_HOST, serverIp_)) {
      Serial.printf("[influx] resolved %s -> %s\n", INFLUX_HOST,
                    serverIp_.toString().c_str());
    } else {
      serverIp_.fromString(INFLUX_FALLBACK_IP);
      Serial.printf("[influx] using fallback IP %s\n",
                    serverIp_.toString().c_str());
    }
  }

  return static_cast<uint32_t>(serverIp_) != 0;
}

bool InfluxReporter::begin() {
  return connect();
}

bool InfluxReporter::send(float uvIndex, float lux, uint32_t uvRaw,
                          uint32_t alsRaw) {
  if (!connect()) {
    Serial.println("[influx] not ready");
    return false;
  }

  String url = "http://" + serverIp_.toString() + ":" +
               String(INFLUX_PORT) + "/api/v2/write?org=" + INFLUX_ORG +
               "&bucket=" + INFLUX_BUCKET;

  String body = "ltr390_telemetry,location=" + String(INFLUX_LOCATION) +
                " uv_index=" + String(uvIndex, 2) +
                ",lux=" + String(lux, 2) +
                ",uv_raw=" + String((unsigned long)uvRaw) + "i" +
                ",als_raw=" + String((unsigned long)alsRaw) + "i";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", "Token " + String(INFLUX_TOKEN));
  http.addHeader("Content-Type", "text/plain; charset=utf-8");

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  Serial.printf("[influx] body: %s\n", body.c_str());
  Serial.printf("[influx] POST -> %d %s\n", code,
                (code >= 200 && code < 300) ? "ok" : "FAIL");
  if (code < 200 || code >= 300) {
    Serial.printf("[influx] error: %s\n", http.errorToString(code).c_str());
    Serial.printf("[influx] response: %s\n", resp.c_str());
  }

  return code >= 200 && code < 300;
}