#include <SPI.h>
#include "Adafruit_MAX31855.h"

// SPI pin configuration for ESP32
#define MAX_SCK   18  // SPI Clock
#define MAX_MISO  19  // SPI MISO (DO)

// Chip Select Pins for each sensor
#define MAX1_CS 2
#define MAX2_CS 4
#define MAX3_CS 33
#define MAX4_CS 32
#define MAX5_CS 5

// Create thermocouple sensor objects (uses default SPI)
Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);
Adafruit_MAX31855 thermocouple3(MAX3_CS);
Adafruit_MAX31855 thermocouple4(MAX4_CS);
Adafruit_MAX31855 thermocouple5(MAX5_CS);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection (for some boards)

  // Manually set SPI pins (only needed on ESP32 if not using default)
  SPI.begin(MAX_SCK, MAX_MISO); // SCK, MISO

  // Initialize each thermocouple sensor
  if (!thermocouple1.begin()) Serial.println("Sensor 1 error");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 error");
  if (!thermocouple3.begin()) Serial.println("Sensor 3 error");
  if (!thermocouple4.begin()) Serial.println("Sensor 4 error");
  if (!thermocouple5.begin()) Serial.println("Sensor 5 error");
}

void loop() {
  // Read temperatures from each sensor
  float temp1 = readTempC(thermocouple1);
  float temp2 = readTempC(thermocouple2);
  float temp3 = readTempC(thermocouple3);
  float temp4 = readTempC(thermocouple4);
  float temp5 = readTempC(thermocouple5);

  // Dummy values for flow and pressure
  float flow = 0.0;
  float pressure = 0.0;

  // Create formatted output string
  String data = "flow:" + String(flow, 1) +
                ",pressure:" + String(pressure, 1) +
                ",temperature_1:" + String(temp1, 1) +
                ",temperature_2:" + String(temp2, 1) +
                ",temperature_3:" + String(temp3, 1) +
                ",temperature_4:" + String(temp4, 1) +
                ",temperature_5:" + String(temp5, 1);

  Serial.println(data);
  delay(1000); // Wait 1 second between readings
}

// Helper function to safely read temperature in Celsius
float readTempC(Adafruit_MAX31855 &sensor) {
  float temp = sensor.readCelsius();
  if (isnan(temp) || (sensor.readError() != 0)) {
    return NAN; // Return NaN if read failed
  }
  return temp;
}
