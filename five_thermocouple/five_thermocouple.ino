#include <SPI.h>
#include "Adafruit_MAX31855.h"

// Chip Select Pins
#define MAX1_CS 5
#define MAX2_CS 2
#define MAX3_CS 17
#define MAX4_CS 16
#define MAX5_CS 4

Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);
Adafruit_MAX31855 thermocouple3(MAX3_CS);
Adafruit_MAX31855 thermocouple4(MAX4_CS);
Adafruit_MAX31855 thermocouple5(MAX5_CS);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection

  // Initialize all thermocouples
  if (!thermocouple1.begin()) Serial.println("Sensor 1 error");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 error");
  if (!thermocouple3.begin()) Serial.println("Sensor 3 error");
  if (!thermocouple4.begin()) Serial.println("Sensor 4 error");
  if (!thermocouple5.begin()) Serial.println("Sensor 5 error");
}

void loop() {
  // Read all temperatures (returns NaN if error occurs)
  float temp1 = readTempC(thermocouple1);
  float temp2 = readTempC(thermocouple2);
  float temp3 = readTempC(thermocouple3);
  float temp4 = readTempC(thermocouple4);
  float temp5 = readTempC(thermocouple5);

  // Set flow and pressure to zero as requested
  float flow = 0.0;
  float pressure = 0.0;

  // Format data string
  String data = "flow:" + String(flow, 1) +
                ",pressure:" + String(pressure, 1) +
                ",temperature_1:" + String(temp1, 1) +
                ",temperature_2:" + String(temp2, 1) +
                ",temperature_3:" + String(temp3, 1) +
                ",temperature_4:" + String(temp4, 1) +
                ",temperature_5:" + String(temp5, 1);

  Serial.println(data);
  delay(1000); // 1 second between readings
}

float readTempC(Adafruit_MAX31855 &sensor) {
  float temp = sensor.readCelsius();
  if (isnan(temp) || (sensor.readError() != 0)) {
    return NAN; // Return NaN for any error
  }
  return temp;
}