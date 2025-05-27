#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include <Adafruit_ADS1X15.h>

// SPI pin configuration for ESP32
#define MAX_SCK   18  // SPI Clock
#define MAX_MISO  19  // SPI MISO (DO)

// Chip Select Pins for each sensor
#define MAX1_CS 2
#define MAX2_CS 4 //sievert 1
#define MAX3_CS 33
#define MAX4_CS 32
#define MAX5_CS 5 //sievert 2

// Create thermocouple sensor objects
Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);
Adafruit_MAX31855 thermocouple3(MAX3_CS);
Adafruit_MAX31855 thermocouple4(MAX4_CS);
Adafruit_MAX31855 thermocouple5(MAX5_CS);

// ADS1115 setup
Adafruit_ADS1115 ads;

// Moving average filter parameters
const int MA_WINDOW_SIZE = 10;
int ma_buffer[MA_WINDOW_SIZE];
int ma_index = 0;
long ma_sum = 0;
bool buffer_filled = false;

// For averaging samples
const int SAMPLE_COUNT = 20;
int raw_sum = 0;
int filtered_sum = 0;
int sample_counter = 0;
unsigned long last_sample_time = 0;

// Pressure sensor parameters (0-3000 psig for 4-20 mA)
const float PSI_MIN = 0.0;
const float PSI_MAX = 3000.0;
const float mA_MIN = 4.0;
const float mA_MAX = 20.0;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection (for some boards)

  // Initialize thermocouples
  SPI.begin(MAX_SCK, MAX_MISO); // SCK, MISO
  if (!thermocouple1.begin()) Serial.println("Sensor 1 error");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 error");
  if (!thermocouple3.begin()) Serial.println("Sensor 3 error");
  if (!thermocouple4.begin()) Serial.println("Sensor 4 error");
  if (!thermocouple5.begin()) Serial.println("Sensor 5 error");

  // Initialize ADS1115
  Wire.begin(21, 22); // SDA, SCL for ESP32
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check wiring.");
    while (1);
  }
  ads.setGain(GAIN_ONE); // ±4.096V input range
}

int movingAverage(int newValue) {
  ma_sum -= ma_buffer[ma_index];
  ma_buffer[ma_index] = newValue;
  ma_sum += newValue;
  ma_index = (ma_index + 1) % MA_WINDOW_SIZE;

  if (ma_index == 0) buffer_filled = true;

  if (buffer_filled)
    return ma_sum / MA_WINDOW_SIZE;
  else
    return ma_sum / ma_index;
}

float readPressure() {
  unsigned long currentMillis = millis();

  if (currentMillis - last_sample_time >= 50) {
    last_sample_time = currentMillis;

    int raw = ads.readADC_SingleEnded(0);
    int filtered = movingAverage(raw);

    raw_sum += raw;
    filtered_sum += filtered;
    sample_counter++;

    if (sample_counter >= SAMPLE_COUNT) {
      int avg_filtered = filtered_sum / SAMPLE_COUNT;
      
      // YOUR CUSTOM mA FORMULA
      float current_mA = 0.0006764 * avg_filtered + 3.9865;
      
      // Convert mA to PSI (0-3000 psig range)
      float pressure = (current_mA - mA_MIN) * (PSI_MAX - PSI_MIN) / (mA_MAX - mA_MIN);
      
      // Clamp pressure to valid range (in case of slight calibration errors)
      pressure = constrain(pressure, PSI_MIN, PSI_MAX);
      pressure = pressure*0.0689476;
      
      // Reset for next cycle
      raw_sum = 0;
      filtered_sum = 0;
      sample_counter = 0;
      
      return pressure;
    }
  }
  return NAN; // Return NaN if not enough samples collected yet
}

float readTempC(Adafruit_MAX31855 &sensor) {
  float temp = sensor.readCelsius();
  if (isnan(temp) || (sensor.readError() != 0)) {
    return NAN; // Return NaN if read failed
  }
  return temp;
}

void loop() {
  // Read pressure
  float pressure = readPressure();
  
  // Only print when we have a new pressure reading
  if (!isnan(pressure)) {
    // Read temperatures from each sensor
    float temp1 = readTempC(thermocouple1);
    float temp2 = readTempC(thermocouple2);
    float temp3 = readTempC(thermocouple3);
    float temp4 = readTempC(thermocouple4);
    float temp5 = readTempC(thermocouple5);

    // Dummy value for flow (replace with actual flow sensor reading if available)
    float flow = 0.0;

    // Create formatted output string
    String data = "flow:" + String(flow, 1) +
                  ",pressure:" + String(pressure, 1) +
                  ",temperature_1:" + String(temp1, 1) +
                  ",temperature_2:" + String(temp2, 1) +
                  ",temperature_3:" + String(temp3, 1) +
                  ",temperature_4:" + String(temp4, 1) +
                  ",temperature_5:" + String(temp5, 1);

    Serial.println(data);
  }
  
  delay(10); // Small delay to prevent busy-waiting
}