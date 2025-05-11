#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define MAX485_DE     4
#define MAX485_RE_NEG 4

ModbusMaster node;

// Union for float-to-bytes conversion (IEEE754 little-endian)
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
  Serial1.begin(9600, SERIAL_8N1, 16, 17); // RX = GPIO16, TX = GPIO17

  node.begin(1, Serial1); // Slave address = 1
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  Serial.println("🔧 ESP32 Modbus RTU - FDC-300 Flow Controller");

  // Optional: test communication
  testCommunication();

  // Set initial flow rate (example: 1000.0 SCCM)
  setFlowRate(3.0);
}

void loop() {
  readActualFlowRate();
  checkWarningCode(); // Add this
  delay(3000); // Read every 3 seconds
}

void testCommunication() {
  uint8_t result = node.readHoldingRegisters(0x0001, 1);
  if (result == node.ku8MBSuccess && node.getResponseBuffer(0) == 0x0101) {
    Serial.println("✅ Communication test passed (0x0101)");
  } else {
    Serial.print("❌ Comm test failed. Code: 0x");
    Serial.println(result, HEX);
  }
}

// Function to set target flow rate (write to 0x20)
void setFlowRate(float targetSCCM) {
  floatConverter.f = targetSCCM;

  uint16_t lowWord  = (floatConverter.b[1] << 8) | floatConverter.b[0];
  uint16_t highWord = (floatConverter.b[3] << 8) | floatConverter.b[2];

  node.setTransmitBuffer(0, lowWord);
  node.setTransmitBuffer(1, highWord);

  uint8_t result = node.writeMultipleRegisters(0x0020, 2);

  Serial.print("🔄 Setting flow rate to ");
  Serial.print(targetSCCM, 2);
  Serial.print(" SCCM... ");

  if (result == node.ku8MBSuccess) {
    Serial.println("✅ Success!");
  } else {
    Serial.print("❌ Failed. Error: 0x");
    Serial.println(result, HEX);
  }
}

// Function to read actual flow rate (from 0x10)
void readActualFlowRate() {
  uint8_t result = node.readHoldingRegisters(0x0010, 2);
  if (result == node.ku8MBSuccess) {
    floatConverter.b[0] = node.getResponseBuffer(0) & 0xFF;
    floatConverter.b[1] = (node.getResponseBuffer(0) >> 8) & 0xFF;
    floatConverter.b[2] = node.getResponseBuffer(1) & 0xFF;
    floatConverter.b[3] = (node.getResponseBuffer(1) >> 8) & 0xFF;

    Serial.print("📊 Actual Flow Rate: ");
    Serial.print(floatConverter.f, 2);
    Serial.println(" SCCM");
  } else {
    Serial.print("⚠️ Error reading flow rate. Code: 0x");
    Serial.println(result, HEX);
  }
}


void checkWarningCode() {
  uint8_t result = node.readHoldingRegisters(0x0061, 1);  // Read 1 register at 0x61

  if (result == node.ku8MBSuccess) {
    uint16_t code = node.getResponseBuffer(0);

    Serial.print("⚠️ Warning Code: 0x");
    Serial.println(code, HEX);

    switch (code) {
      case 0x00:
        Serial.println("✅ No warning (0x00)");
        break;
      case 0x01:
        Serial.println("❌ Sensor abnormality or valve leakage");
        Serial.println("➡️  Perform zero adjustment after preheating.");
        break;
      case 0x02:
        Serial.println("❌ Abnormal air source");
        Serial.println("➡️  Check air supply.");
        break;
      case 0x03:
        Serial.println("❌ Abnormal power supply voltage");
        Serial.println("➡️  Check voltage input to the device.");
        break;
      case 0x04:
        Serial.println("❌ Set signal over the limit");
        Serial.println("➡️  Verify the set value is within valid range.");
        break;
      default:
        Serial.println("❓ Unknown warning code.");
        break;
    }
  } else {
    Serial.print("❌ Failed to read warning code. Error: 0x");
    Serial.println(result, HEX);
  }
}

