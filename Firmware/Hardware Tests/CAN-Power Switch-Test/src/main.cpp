#include <Arduino.h>

#define POWER_SWITCH_PIN 11

void setup() {
  Serial.begin(115200);
  Serial.println("Base Station CAN Bus Power Switch Test");
  // put your setup code here, to run once:
  pinMode(POWER_SWITCH_PIN, OUTPUT);
  digitalWrite(POWER_SWITCH_PIN, LOW);

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("CAN Bus Power Switch is OFF");
  delay(3000);
  digitalWrite(POWER_SWITCH_PIN, HIGH);
  Serial.println("CAN Bus Power Switch is ON");
  delay(3000);
  digitalWrite(POWER_SWITCH_PIN, LOW);
}
