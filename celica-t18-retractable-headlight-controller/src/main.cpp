#include <Arduino.h>

void setup() {
  Serial.begin(115200);

  pinMode(4, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
}

void loop() {
  Serial.print(digitalRead(4));
  Serial.print(digitalRead(6));
  Serial.print(digitalRead(10));
  Serial.println(digitalRead(12));

  delay(20);
}
