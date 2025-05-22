// LED control
const int ledPin = 27;  // GPIO 16
int sliderValue = 50;   // Default value (0-100)

// Simulated sensor values
float flow = 50.0;
float pressure = 5.0;
float temp1 = 500;
float temp2 = 600;
float temp3 = 700;
float temp4 = 450;
float temp5 = 570;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);  // Match this with Node-RED baud rate
}

void loop() {
  // 1. Handle incoming serial data (slider values)
  handleSerialInput();
  
  // 2. Update LED brightness based on slider value
  int brightness = map(sliderValue, 0, 100, 0, 255);
  analogWrite(ledPin, brightness);
  
  // 3. Simulate sensor data with random walk
  updateSensorValues();
  
  // 4. Send sensor data to Node-RED
  sendSensorData();
  
  delay(1000);  // Update every second
}

void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      int value = input.toInt();
      if (value >= 0 && value <= 100) {
        sliderValue = value;
      }
    }
  }
}

void updateSensorValues() {
  // Add small random variations
  flow += random(-5, 6) * 0.1;
  pressure += random(-3, 4) * 0.1;
  temp1 += random(-20, 30) * 0.1;
  temp2 += random(-20, 30) * 0.1;
  temp3 += random(-20, 30) * 0.1;
  temp4 += random(-20, 30) * 0.1;
  temp5 += random(-20, 30) * 0.1;
  
  // Constrain to realistic ranges
  flow = constrain(flow, 0.0, 100.0);
  pressure = constrain(pressure, 0.0, 10.0);
  temp1 = constrain(temp1, 20.0, 800.0);
  temp2 = constrain(temp2, 20.0, 800.0);
  temp3 = constrain(temp3, 20.0, 800.0);
  temp4 = constrain(temp4, 20.0, 800.0);
  temp5 = constrain(temp5, 20.0, 800.0);
}

void sendSensorData() {
  String data = "flow:" + String(flow, 1) +
                ",pressure:" + String(pressure, 1) +
                ",temperature_1:" + String(temp1, 1) +
                ",temperature_2:" + String(temp2, 1) +
                ",temperature_3:" + String(temp3, 1) +
                ",temperature_4:" + String(temp4, 1) +
                ",temperature_5:" + String(temp5, 1) +
                ",slider:" + String(sliderValue);
                
  Serial.println(data);
}