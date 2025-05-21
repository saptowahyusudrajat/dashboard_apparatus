#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const int MA_WINDOW_SIZE = 10;
int ma_buffer[MA_WINDOW_SIZE];
int ma_index = 0;
long ma_sum = 0;
bool buffer_filled = false;

// For averaging 20 samples every 1 second
const int SAMPLE_COUNT = 20;
int raw_sum = 0;
int filtered_sum = 0;
int sample_counter = 0;
unsigned long last_sample_time = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(22, 23);

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check wiring.");
    while (1);
  }

  ads.setGain(GAIN_ONE); // ±4.096V input range
  Serial.println("RawAvg\tFilteredAvg\tCurrent_mA");
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

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - last_sample_time >= 50) {
    last_sample_time = currentMillis;

    int raw = ads.readADC_SingleEnded(0);
    int filtered = movingAverage(raw);

    raw_sum += raw;
    filtered_sum += filtered;
    sample_counter++;

    if (sample_counter >= SAMPLE_COUNT) {
      int avg_raw = raw_sum / SAMPLE_COUNT;
      int avg_filtered = filtered_sum / SAMPLE_COUNT;
      float current_mA = 0.0006764 * avg_filtered + 3.9865;

      Serial.print(avg_raw);
      Serial.print("\t");
      Serial.print(avg_filtered);
      Serial.print("\t");
      Serial.println(current_mA, 2);

      // Reset for next cycle
      raw_sum = 0;
      filtered_sum = 0;
      sample_counter = 0;
    }
  }
}
