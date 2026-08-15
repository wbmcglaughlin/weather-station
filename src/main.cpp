#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define RGB_BUILTIN 10 
#define I2C_SDA 8
#define I2C_SCL 9
#define BME280_RETRY_INTERVAL 1000  // Retry every 1 second if sensor fails

Adafruit_BME280 bme;

bool initializeBME280() {
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Try both common I2C addresses
  if (bme.begin(0x77, &Wire) || bme.begin(0x76, &Wire)) {
    // Set recommended settings for weather monitoring
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Initialize BME280 with retry logic
  while (!initializeBME280()) {
    neopixelWrite(RGB_BUILTIN, 64, 0, 0); // Red error
    delay(250);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    delay(BME280_RETRY_INTERVAL);
  }
}

void loop() {
  // Take a measurement
  bme.takeForcedMeasurement();
  
  // Read sensor values with error checking
  float temp = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;

  // Validate sensor readings
  if (isnan(temp) || isnan(humidity) || isnan(pressure)) {
    neopixelWrite(RGB_BUILTIN, 64, 0, 0); // Red error
    delay(1000);
    return;
  }

  // Visual feedback
  neopixelWrite(RGB_BUILTIN, 0, 32, 0); 
  delay(50);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);  

  // Form clean JSON line
  Serial.printf("{\"temperature\": %.2f, \"humidity\": %.2f, \"pressure\": %.2f}\n", 
                temp, humidity, pressure);
  
  // Force transmission of CDC USB buffer
  Serial.flush();

  // Wait for next reading with watchdog reset
  for (int i = 0; i < 50; i++) {
    delay(100);
    yield(); // Allow ESP32 background tasks to run
  }
}
