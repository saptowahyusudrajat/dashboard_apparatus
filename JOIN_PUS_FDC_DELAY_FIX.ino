#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ModbusMaster.h>

/* ================= PIN CONFIG ================= */
#define MAX_SCK   18
#define MAX_MISO  19

#define MAX1_CS 2
#define MAX2_CS 4
#define MAX3_CS 33
#define MAX4_CS 32
#define MAX5_CS 5

#define MAX485_DE     23
#define MAX485_RE_NEG 23

/* ================= OBJECTS ================= */
Adafruit_MAX31855 thermocouple1(MAX1_CS);
Adafruit_MAX31855 thermocouple2(MAX2_CS);
Adafruit_MAX31855 thermocouple3(MAX3_CS);
Adafruit_MAX31855 thermocouple4(MAX4_CS);
Adafruit_MAX31855 thermocouple5(MAX5_CS);

Adafruit_ADS1115 ads;
ModbusMaster node;

/* ================= UNION ================= */
union {
  float f;
  uint8_t b[4];
} floatConverter;

/* ================= MODBUS CONTROL ================= */
bool modbusBusy = false;
unsigned long lastModbusTime = 0;
const unsigned long MODBUS_GUARD_TIME = 20;   // ms

/* ================= FLOW READ ================= */
unsigned long lastFlowRead = 0;
const unsigned long FLOW_READ_INTERVAL = 500;
float cachedFlow = NAN;

/* ================= SERIAL ================= */
String serialBuffer = "";
bool newFlowCommand = false;

/* ================= PRESSURE FILTER ================= */
const int MA_WINDOW_SIZE = 10;
int ma_buffer[MA_WINDOW_SIZE];
int ma_index = 0;
long ma_sum = 0;
bool buffer_filled = false;

const int SAMPLE_COUNT = 20;
int filtered_sum = 0;
int sample_counter = 0;
unsigned long last_sample_time = 0;

/* ================= PRESSURE SENSOR ================= */
const float PSI_MIN = 0.0;
const float PSI_MAX = 3000.0;
const float mA_MIN = 4.0;
const float mA_MAX = 20.0;

/* ================= RS485 ================= */
void preTransmission() {
  digitalWrite(MAX485_DE, HIGH);
  digitalWrite(MAX485_RE_NEG, HIGH);
  delayMicroseconds(200);
}

void postTransmission() {
  delayMicroseconds(200);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);
}

/* ================= MOVING AVERAGE ================= */
int movingAverage(int v) {
  ma_sum -= ma_buffer[ma_index];
  ma_buffer[ma_index] = v;
  ma_sum += v;
  ma_index = (ma_index + 1) % MA_WINDOW_SIZE;
  if (ma_index == 0) buffer_filled = true;
  return buffer_filled ? ma_sum / MA_WINDOW_SIZE : ma_sum / ma_index;
}

/* ================= FLOW READ ================= */
float readFlowRate() {
  if (modbusBusy) return cachedFlow;
  if (millis() - lastModbusTime < MODBUS_GUARD_TIME) return cachedFlow;

  modbusBusy = true;
  lastModbusTime = millis();

  uint8_t res = node.readHoldingRegisters(0x0010, 2);

  modbusBusy = false;
  delay(MODBUS_GUARD_TIME);

  if (res == node.ku8MBSuccess) {
    floatConverter.b[0] = node.getResponseBuffer(0) & 0xFF;
    floatConverter.b[1] = (node.getResponseBuffer(0) >> 8) & 0xFF;
    floatConverter.b[2] = node.getResponseBuffer(1) & 0xFF;
    floatConverter.b[3] = (node.getResponseBuffer(1) >> 8) & 0xFF;
    cachedFlow = floatConverter.f;
  }
  return cachedFlow;
}

/* ================= FLOW WRITE ================= */
void setFlowRate(float sccm) {
  if (modbusBusy) return;
  if (millis() - lastModbusTime < MODBUS_GUARD_TIME) return;

  modbusBusy = true;
  lastModbusTime = millis();

  floatConverter.f = sccm;
  uint16_t lo = (floatConverter.b[1] << 8) | floatConverter.b[0];
  uint16_t hi = (floatConverter.b[3] << 8) | floatConverter.b[2];

  node.setTransmitBuffer(0, lo);
  node.setTransmitBuffer(1, hi);
  node.writeMultipleRegisters(0x0020, 2);

  modbusBusy = false;
  delay(MODBUS_GUARD_TIME);

  Serial.print("Flow set to: ");
  Serial.println(sccm, 2);
}

/* ================= SERIAL ================= */
void checkSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      float val = serialBuffer.toFloat();
      serialBuffer = "";
      if (val >= 0 && val <= 30) {
        newFlowCommand = true;
        setFlowRate(val);
      }
    } else if (isDigit(c) || c == '.' || c == '-') {
      serialBuffer += c;
    }
  }
}

/* ================= PRESSURE ================= */
float readPressure() {
  if (millis() - last_sample_time < 50) return NAN;
  last_sample_time = millis();

  int raw = ads.readADC_SingleEnded(0);
  int filt = movingAverage(raw);

  filtered_sum += filt;
  sample_counter++;

  if (sample_counter >= SAMPLE_COUNT) {
    int avg = filtered_sum / SAMPLE_COUNT;
    filtered_sum = 0;
    sample_counter = 0;

    float mA = 0.0006764 * avg + 3.9865;
    float psi = (mA - mA_MIN) * PSI_MAX / (mA_MAX - mA_MIN);
    return constrain(psi * 0.0689476, 0, 300);
  }
  return NAN;
}

/* ================= TEMP ================= */
float readTempC(Adafruit_MAX31855 &tc) {
  float t = tc.readCelsius();
  return (isnan(t) || tc.readError()) ? NAN : t;
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  SPI.begin(MAX_SCK, MAX_MISO);

  thermocouple1.begin();
  thermocouple2.begin();
  thermocouple3.begin();
  thermocouple4.begin();
  thermocouple5.begin();

  Wire.begin(21, 22);
  ads.begin();
  ads.setGain(GAIN_ONE);

  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);

  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  setFlowRate(5.0);
}

/* ================= LOOP ================= */
void loop() {
  checkSerialCommands();

  unsigned long now = millis();
  if (!newFlowCommand && now - lastFlowRead >= FLOW_READ_INTERVAL) {
    lastFlowRead = now;
    readFlowRate();
  }
  newFlowCommand = false;

  float pressure = readPressure();
  if (!isnan(pressure)) {

    float t1 = readTempC(thermocouple1);
    float t2 = readTempC(thermocouple2);
    float t3 = readTempC(thermocouple3);
    float t4 = readTempC(thermocouple4);
    float t5 = readTempC(thermocouple5);

    String data =
      "flow:" + String(cachedFlow, 4) +
      ",pressure:" + String(pressure, 1) +
      ",temperature_1:" + String(t1, 1) +
      ",temperature_2:" + String(t2, 1) +
      ",temperature_3:" + String(t3, 1) +
      ",temperature_4:" + String(t4, 1) +
      ",temperature_5:" + String(t5, 1);

    Serial.println(data);
  }

  delay(5);
}
