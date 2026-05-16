#include <Arduino.h>
void setup() {
  Serial.begin(115200);
  
  // Set Pin 9 to output 5V and listen for the optocoupler to pull it to Ground
  pinMode(9, INPUT_PULLUP); 
  
  Serial.println("--- Right Headlight Sensor Test (Pin 7) ---");
  Serial.println("Turn the manual knob on the right motor to see the state change.");
}

void loop() {
  int sensorState = digitalRead(9);

  if (sensorState == LOW) { 
    // Optocoupler LED is ON because Pin 7 is spitting 12V
    Serial.println("Signal: 0  |  State: MOTOR MOVING (Wiper is on the copper)");
  } else { 
    // Optocoupler LED is OFF because Pin 7 is dead
    Serial.println("Signal: 1  |  State: FULLY PARKED UP (Wiper is on the plastic)");
  }

  delay(100); 
}