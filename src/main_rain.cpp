#include <Arduino.h>

#include "InfluxReporter.h"
#include "config.h"

#define RGB_BUILTIN 10
#define RAIN_PIN 4
#define RAIN_TIP_MM 0.3f        // MISOL: one bucket tip == 0.3 mm
#define RAIN_DEBOUNCE_US 50000  // 50 ms minimum between tips (reed switch bounce)

volatile uint32_t tipCount = 0;
volatile uint32_t lastTipUs = 0;

InfluxReporter influx;

unsigned long lastReport = 0;
uint32_t lastReportTips = 0;
bool serverOk = false;

void IRAM_ATTR onTip() {
  uint32_t now = micros();
  if (now - lastTipUs < RAIN_DEBOUNCE_US) {
    return;
  }
  lastTipUs = now;
  tipCount++;
}

void setup() {
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 2000) {
    delay(10);
  }

  pinMode(RAIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), onTip, FALLING);

  // WiFi/InfluxDB reporting is best-effort; tips still count over USB
  influx.begin();
}

void loop() {
  uint32_t total = tipCount;

  // Flash blue on each tip for visual confirmation
  static uint32_t lastSeenTips = 0;
  if (total != lastSeenTips) {
    lastSeenTips = total;
    neopixelWrite(RGB_BUILTIN, 0, 0, 64);
    delay(10);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
  }

  // Report directly to InfluxDB over WiFi (throttled)
  if (lastReport == 0 || millis() - lastReport >= REPORT_INTERVAL_S * 1000UL) {
    lastReport = millis();

    uint32_t tips = total - lastReportTips;
    lastReportTips = total;

    float rainfallMm = tips * RAIN_TIP_MM;
    float totalRainfallMm = total * RAIN_TIP_MM;

    Serial.printf("{\"tips\": %lu, \"rainfall_mm\": %.2f, \"total_tips\": %lu, \"total_rainfall_mm\": %.2f}\n",
                  (unsigned long)tips, rainfallMm, (unsigned long)total,
                  totalRainfallMm);
    Serial.flush();

    serverOk = influx.sendRainGauge(tips, total, rainfallMm, totalRainfallMm);

    // Longer confirmation flash: GREEN = server reachable, YELLOW = WiFi up but server unreachable
    neopixelWrite(RGB_BUILTIN, serverOk ? 0 : 64, 64, 0);
    delay(150);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
  }

  delay(100);
}
