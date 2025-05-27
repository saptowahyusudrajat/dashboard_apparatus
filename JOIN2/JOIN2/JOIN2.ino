#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include <Adafruit_ADS1X15.h>

// SPI pin configuration for ESP32
#define MAX_SCK   18  // SPI Clock
#define MAX_MISO  19  // SPI MISO (DO)

// Chip Select Pins for the two sensors (CS1 = GPIO4, CS2 = GPIO5)
#define MAX1_CS 4  // Sievert 1 (CS1)
#define MAX2_CS 5  // Sievert 2 (CS2)

// Create thermocouple sensor objects (only using MAX1 and MAX2)
Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);

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

// Pressure sensor parameters (-1.01 to 55.2 bar for 4-20 mA)
const float BAR_MIN = -1.01;
const float BAR_MAX = 55.2;
const float mA_MIN = 4.0;
const float mA_MAX = 20.0;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection (for some boards)

  // Initialize thermocouples (only MAX1 and MAX2)
  SPI.begin(MAX_SCK, MAX_MISO); // SCK, MISO
  if (!thermocouple1.begin()) Serial.println("Sensor 1 (CS1) error");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 (CS2) error");

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
      
      // Convert mA to Bar (-1.01 to 55.2 bar range)
      float pressure_bar = (current_mA - mA_MIN) * (BAR_MAX - BAR_MIN) / (mA_MAX - mA_MIN) + BAR_MIN;
      
      // Clamp pressure to valid range (in case of slight calibration errors)
      pressure_bar = constrain(pressure_bar, BAR_MIN, BAR_MAX);
      
      // Reset for next cycle
      raw_sum = 0;
      filtered_sum = 0;
      sample_counter = 0;
      
      return pressure_bar;
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
    // Read temperatures from the two active sensors
    float temp1 = readTempC(thermocouple1);
    float temp2 = readTempC(thermocouple2);

    // Dummy value for flow (replace with actual flow sensor reading if available)
    float flow = 0.0;

    // Create formatted output string with NAN for unused sensors
    // Pressure now displayed in bar with 4 decimal precision
    String data = "flow:" + String(flow, 1) +
                  ",pressure:" + String(pressure, 4) +
                  ",temperature_1:" + String(temp1, 1) +
                  ",temperature_2:" + String(temp2, 1) +
                  ",temperature_3:" + String(NAN) +
                  ",temperature_4:" + String(NAN) +
                  ",temperature_5:" + String(NAN);

    Serial.println(data);
  }
  
  delay(10); // Small delay to prevent busy-waiting
}