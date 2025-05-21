#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const float adc_max_voltage = 4.096;     // ADS1115 GAIN_ONE range
const int16_t adc_max_counts = 32767;    // 16-bit ADC resolution

// Expected voltage range from HW-685 output
const float min_voltage = 0.66;          // Voltage at 4 mA
const float max_voltage = 3.30;          // Voltage at 20 mA

// Corresponding current range
const float min_current = 4.0;           // mA
const float max_current = 20.0;          // mA

void setup() {
  Serial.begin(115200);
  
  // Use GPIO 22 as SDA, 23 as SCL
  Wire.begin(22, 23);

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check wiring.");
    while (1);
  }

  ads.setGain(GAIN_ONE);  // ±4.096V input range
  Serial.println("HW-685 Calibration + Current Reader");
  Serial.println("Apply 4 mA and 20 mA signals, adjust Zero and Range pots.");
}

void loop() {

int raw = ads.readADC_SingleEnded(0);  // example read
float current_mA = 0.0006764 * raw + 3.9865;

Serial.print("ADC Raw: ");
Serial.print(raw);
Serial.print(" | Current: ");
Serial.print(current_mA, 2);
Serial.println(" mA");



  delay(1000);
}


