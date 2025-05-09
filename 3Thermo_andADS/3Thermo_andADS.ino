#include <Adafruit_MAX31855.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// SPI pin for MAX31855
// Cek perubahan
// cek perubahan kedua
#define MAXDO   19  // MISO
#define MAXCLK  18  // SCK
#define MAXCS1  5
#define MAXCS2  4
#define MAXCS3  21

// I2C pins (default)
#define I2C_SDA 22
#define I2C_SCL 23

// MAX31855 Sensors
Adafruit_MAX31855 thermocouple1(MAXCLK, MAXCS1, MAXDO);
Adafruit_MAX31855 thermocouple2(MAXCLK, MAXCS2, MAXDO);
Adafruit_MAX31855 thermocouple3(MAXCLK, MAXCS3, MAXDO);

// ADS1115 ADC
Adafruit_ADS1115 ads;  // default I2C address 0x48

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("=== ESP32 + MAX31855 + ADS1115 ===");

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check wiring!");
    while (1);
  }

  ads.setGain(GAIN_ONE); // +/- 4.096V input range (1 bit = 125uV)

  // Check thermocouples
  if (!thermocouple1.begin()) Serial.println("Sensor 1 not found!");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 not found!");
  if (!thermocouple3.begin()) Serial.println("Sensor 3 not found!");
}


void read4to20mA(uint8_t channel) {
  int16_t adc = ads.readADC_SingleEnded(channel);
  float voltage = adc * 0.125 / 1000.0; // ADS1115 GAIN_ONE → 0.125 mV/bit → V

  // Map 0V (4 mA) to 3.0691V (20 mA)
  float current_mA = (voltage * (16.0 / 3.0691)) + 4.0;
  float percentage = voltage * (100.0 / 3.0691);

  // Clamp to expected ranges
  current_mA = constrain(current_mA, 4.0, 20.0);
  percentage = constrain(percentage, 0.0, 100.0);

  Serial.print("A"); Serial.print(channel);
  Serial.print(": ");
  Serial.print(voltage, 4); Serial.print(" V | ");
  Serial.print(current_mA, 2); Serial.print(" mA | ");
  Serial.print(percentage, 1); Serial.println(" %");
}


void loop() {
  Serial.println("\n--- Thermocouples ---");
  readThermo("Sensor 1", thermocouple1);
  readThermo("Sensor 2", thermocouple2);
  readThermo("Sensor 3", thermocouple3);

  Serial.println("\n--- ADS1115 Analog Inputs (4–20 mA Mapping) ---");
  for (int i = 0; i < 4; i++) {
    read4to20mA(i);
  }


  delay(1000);
}

void readThermo(const char* label, Adafruit_MAX31855& sensor) {
  double tempC = sensor.readCelsius();
  double internalC = sensor.readInternal();

  Serial.print(label);
  Serial.print(" | TC: ");
  if (isnan(tempC)) {
    Serial.print("ERROR");
  } else {
    Serial.print(tempC);
    Serial.print(" °C");
  }

  Serial.print(" | INT: ");
  if (isnan(internalC)) {
    Serial.print("N/A");
  } else {
    Serial.print(internalC);
    Serial.print(" °C");
  }

  // Faults
  if (sensor.readError()) {
    Serial.print(" | FAULT: ");
    uint8_t err = sensor.readError();
    if (err & 0x01) Serial.print("Open ");
    if (err & 0x02) Serial.print("Short-GND ");
    if (err & 0x04) Serial.print("Short-VCC ");
  }


  Serial.println();
}

