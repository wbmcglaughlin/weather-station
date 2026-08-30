#include <Arduino.h>
#include "LTR390Sensor.h"
#include "InfluxReporter.h"
#include "config.h"

#define RGB_BUILTIN 10
#define I2C_SDA 8
#define I2C_SCL 9
#define LTR390_RETRY_INTERVAL 1000  // Retry every 1 second if sensor fails

LTR390Sensor ltr390;
InfluxReporter influx;

unsigned long lastReport = 0;
bool serverOk = false;

bool initializeLTR390() {
  return ltr390.begin(I2C_SDA, I2C_SCL);
}

void signalError() {
  neopixelWrite(RGB_BUILTIN, 64, 0, 0); // Red error
  delay(250);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Initialize LTR390 with retry logic
  while (!initializeLTR390()) {
    signalError();
    delay(LTR390_RETRY_INTERVAL);
  }

  // WiFi/InfluxDB reporting is best-effort; sensor still works over USB
  influx.begin();
}

void loop() {
  if (!ltr390.sample()) {
    neopixelWrite(RGB_BUILTIN, 64, 0, 0); // Red error
    delay(1000);
    return;
  }

  float uvIndex = ltr390.uvIndex();
  float lux = ltr390.lux();

  // Validate sensor readings
  if (isnan(uvIndex) || isnan(lux)) {
    neopixelWrite(RGB_BUILTIN, 64, 0, 0); // Red error
    delay(1000);
    return;
  }

  // Visual feedback: GREEN = server reachable, YELLOW = WiFi up but server unreachable
  neopixelWrite(RGB_BUILTIN, serverOk ? 0 : 64, 64, 0);
  delay(50);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);

  // Form clean JSON line
  Serial.printf("{\"uv_index\": %.2f, \"uv_raw\": %lu, \"lux\": %.2f, \"als_raw\": %lu}\n",
                uvIndex, (unsigned long)ltr390.uvsRaw(), lux,
                (unsigned long)ltr390.alsRaw());

  // Force transmission of CDC USB buffer
  Serial.flush();

  // Report directly to InfluxDB over WiFi (throttled)
  if (lastReport == 0 || millis() - lastReport >= REPORT_INTERVAL_S * 1000UL) {
    lastReport = millis();
    serverOk = influx.send(uvIndex, lux, ltr390.uvsRaw(), ltr390.alsRaw());

    // Longer confirmation flash of the report result
    neopixelWrite(RGB_BUILTIN, serverOk ? 0 : 64, 64, 0);
    delay(150);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
  }

  // Wait for next reading with watchdog reset
  for (int i = 0; i < 50; i++) {
    delay(100);
    yield(); // Allow ESP32 background tasks to run
  }
}