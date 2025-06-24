#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ModbusMaster.h>

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

// Modbus setup
#define MAX485_DE     23
#define MAX485_RE_NEG 23
ModbusMaster node;

// Union for float conversion
union {
  float f;
  uint8_t b[4];
} floatConverter;

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

// Serial input handling
String serialBuffer = "";
bool newCommandAvailable = false;

void preTransmission() {
  digitalWrite(MAX485_DE, 1);
  digitalWrite(MAX485_RE_NEG, 1);
  delayMicroseconds(50);
}

void postTransmission() {
  delayMicroseconds(50);
  digitalWrite(MAX485_DE, 0);
  digitalWrite(MAX485_RE_NEG, 0);
}

float readFlowRate() {
  if (node.readHoldingRegisters(0x0010, 2) == node.ku8MBSuccess) {
    floatConverter.b[0] = node.getResponseBuffer(0) & 0xFF;
    floatConverter.b[1] = (node.getResponseBuffer(0) >> 8) & 0xFF;
    floatConverter.b[2] = node.getResponseBuffer(1) & 0xFF;
    floatConverter.b[3] = (node.getResponseBuffer(1) >> 8) & 0xFF;
    return floatConverter.f;
  }
  return NAN;
}

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection

  // Initialize thermocouples
  SPI.begin(MAX_SCK, MAX_MISO);
  if (!thermocouple1.begin()) Serial.println("Sensor 1 error");
  if (!thermocouple2.begin()) Serial.println("Sensor 2 error");
  if (!thermocouple3.begin()) Serial.println("Sensor 3 error");
  if (!thermocouple4.begin()) Serial.println("Sensor 4 error");
  if (!thermocouple5.begin()) Serial.println("Sensor 5 error");

  // Initialize ADS1115
  Wire.begin(21, 22);
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check wiring.");
    while (1);
  }
  ads.setGain(GAIN_ONE);

  // Initialize Modbus
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, 0);
  digitalWrite(MAX485_RE_NEG, 0);

  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Set initial flow rate
  setFlowRate(5.0);
}

void setFlowRate(float targetSCCM) {
  floatConverter.f = targetSCCM;
  uint16_t lowWord  = (floatConverter.b[1] << 8) | floatConverter.b[0];
  uint16_t highWord = (floatConverter.b[3] << 8) | floatConverter.b[2];
  node.setTransmitBuffer(0, lowWord);
  node.setTransmitBuffer(1, highWord);
  node.writeMultipleRegisters(0x0020, 2);
  
  // Send confirmation back to Node-RED
  Serial.print("Flow set to: ");
  Serial.println(targetSCCM, 1);
}

void checkSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      newCommandAvailable = true;
      break;
    } else if (isDigit(c) || c == '.' || c == '-') {
      serialBuffer += c;
    }
  }

  if (newCommandAvailable) {
    float newFlowRate = serialBuffer.toFloat();
    serialBuffer = "";
    newCommandAvailable = false;
    
    // Validate and set new flow rate
    if (newFlowRate >= 0 && newFlowRate <= 30) {
      setFlowRate(newFlowRate);
    } else {
      //Serial.println("Invalid flow rate (0-30 SCCM)");
    }
  }
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
      float current_mA = 0.0006764 * avg_filtered + 3.9865;
      float pressure = (current_mA - mA_MIN) * (PSI_MAX - PSI_MIN) / (mA_MAX - mA_MIN);
      pressure = constrain(pressure, PSI_MIN, PSI_MAX);
      pressure = pressure*0.0689476;
      
      raw_sum = 0;
      filtered_sum = 0;
      sample_counter = 0;
      
      return pressure;
    }
  }
  return NAN;
}

float readTempC(Adafruit_MAX31855 &sensor) {
  float temp = sensor.readCelsius();
  if (isnan(temp) || (sensor.readError() != 0)) {
    return NAN;
  }
  return temp;
}

void loop() {
  // Check for incoming serial commands from Node-RED
  checkSerialCommands();

  // Read pressure
  float pressure = readPressure();
  
  // Only print when we have a new pressure reading
  if (!isnan(pressure)) {
    // Read all sensors
    float temp1 = readTempC(thermocouple1);
    float temp2 = readTempC(thermocouple2);
    float temp3 = readTempC(thermocouple3);
    float temp4 = readTempC(thermocouple4);
    float temp5 = readTempC(thermocouple5);
    float flow = readFlowRate();

    // Create formatted output string
    String data = "flow:" + String(flow, 4) +
                  ",pressure:" + String(pressure, 1) +
                  ",temperature_1:" + String(temp1, 1) +
                  ",temperature_2:" + String(temp2, 1) +
                  ",temperature_3:" + String(temp3, 1) +
                  ",temperature_4:" + String(temp4, 1) +
                  ",temperature_5:" + String(temp5, 1);

    Serial.println(data);
  }
  
  delay(10);
}