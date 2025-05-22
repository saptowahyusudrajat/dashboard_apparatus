const int ledPin = 27;  // 16 corresponds to GPIO 16
int manualBrightness = -1;  // -1 means automatic fading mode, 0-255 means manual control

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);  // Initialize serial communication
}

void loop() {
  // Check for incoming serial data
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // Convert to integer (0-100)
    int percent = input.toInt();
    
    // Validate the input
    if (percent >= 0 && percent <= 100) {
      // Convert percentage (0-100) to PWM value (0-255)
      manualBrightness = map(percent, 0, 100, 0, 255);
      analogWrite(ledPin, manualBrightness);
    }
  }
  
  // If no manual control, do the automatic fading
  if (manualBrightness == -1) {
    // increase the LED brightness
    for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
      // Check for serial input during fading
      if (Serial.available() > 0) break;
      analogWrite(ledPin, dutyCycle);
      delay(15);
    }

    // decrease the LED brightness
    for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
      // Check for serial input during fading
      if (Serial.available() > 0) break;
      analogWrite(ledPin, dutyCycle);
      delay(15);
    }
  }
}