#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define MAX485_DE     23
#define MAX485_RE_NEG 23

ModbusMaster node;

// Union for float conversion
union {
  float f;
  uint8_t b[4];
} floatConverter;

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

void setup() {
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, 0);
  digitalWrite(MAX485_RE_NEG, 0);

  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);

  node.begin(1, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Example: Set initial flow rate (5.0 SCCM)
  setFlowRate(5.0);
}

void loop() {
  readActualFlowRate();
  delay(1000);
}

// Set target flow rate (write to 0x0020)
void setFlowRate(float targetSCCM) {
  floatConverter.f = targetSCCM;

  uint16_t lowWord  = (floatConverter.b[1] << 8) | floatConverter.b[0];
  uint16_t highWord = (floatConverter.b[3] << 8) | floatConverter.b[2];

  node.setTransmitBuffer(0, lowWord);
  node.setTransmitBuffer(1, highWord);

  node.writeMultipleRegisters(0x0020, 2);
}

// Read actual flow rate (from 0x0010)
void readActualFlowRate() {
  if (node.readHoldingRegisters(0x0010, 2) == node.ku8MBSuccess) {
    floatConverter.b[0] = node.getResponseBuffer(0) & 0xFF;
    floatConverter.b[1] = (node.getResponseBuffer(0) >> 8) & 0xFF;
    floatConverter.b[2] = node.getResponseBuffer(1) & 0xFF;
    floatConverter.b[3] = (node.getResponseBuffer(1) >> 8) & 0xFF;
    
    Serial.println(floatConverter.f, 2); // Prints flow rate (e.g., "5.00")
  }
}