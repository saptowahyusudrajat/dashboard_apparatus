#include <SPI.h>
#include "Adafruit_MAX31855.h"

// Define the pins for each MAX31855
#define MAX1_CS 5
#define MAX2_CS 2
#define MAX3_CS 17
#define MAX4_CS 16
#define MAX5_CS 4

// Initialize the MAX31855 objects
Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);
Adafruit_MAX31855 thermocouple3(MAX3_CS);
Adafruit_MAX31855 thermocouple4(MAX4_CS);
Adafruit_MAX31855 thermocouple5(MAX5_CS);

void setup() {
  Serial.begin(115200);
  
  // Wait for serial connection
  while (!Serial) delay(10);
  
  Serial.println("MAX31855 thermocouple test");

  // Initialize all thermocouples
  if (!thermocouple1.begin()) {
    Serial.println("ERROR: Thermocouple 1 not found!");
    while (1) delay(10);
  }
  if (!thermocouple2.begin()) {
    Serial.println("ERROR: Thermocouple 2 not found!");
    while (1) delay(10);
  }
  if (!thermocouple3.begin()) {
    Serial.println("ERROR: Thermocouple 3 not found!");
    while (1) delay(10);
  }
  if (!thermocouple4.begin()) {
    Serial.println("ERROR: Thermocouple 4 not found!");
    while (1) delay(10);
  }
  if (!thermocouple5.begin()) {
    Serial.println("ERROR: Thermocouple 5 not found!");
    while (1) delay(10);
  }
}

void loop() {
  // Read and display temperature for each thermocouple
  readAndDisplay(1, thermocouple1);
  readAndDisplay(2, thermocouple2);
  readAndDisplay(3, thermocouple3);
  readAndDisplay(4, thermocouple4);
  readAndDisplay(5, thermocouple5);
  
  Serial.println(); // Add a blank line between readings
  delay(1000); // Wait 1 second between readings
}

void readAndDisplay(int sensorNum, Adafruit_MAX31855 &thermocouple) {
  double c = thermocouple.readCelsius();
  double f = thermocouple.readFahrenheit();
  
  if (isnan(c)) {
    Serial.print("Sensor ");
    Serial.print(sensorNum);
    Serial.println(": Thermocouple fault detected");
    uint8_t fault = thermocouple.readError();
    if (fault & MAX31855_FAULT_OPEN) Serial.println("FAULT: Thermocouple open");
    if (fault & MAX31855_FAULT_SHORT_GND) Serial.println("FAULT: Thermocouple short to ground");
    if (fault & MAX31855_FAULT_SHORT_VCC) Serial.println("FAULT: Thermocouple short to VCC");
  } else {
    Serial.print("Sensor ");
    Serial.print(sensorNum);
    Serial.print(": Temp = ");
    Serial.print(c);
    Serial.print("°C, ");
    Serial.print(f);
    Serial.println("°F");
    
    // Optional: Read and display internal junction temperature
    double internal = thermocouple.readInternal();
    Serial.print("  Internal Temp = ");
    Serial.print(internal);
    Serial.println("°C");
  }
}