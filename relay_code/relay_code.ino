#define RELAY_PIN 32  // connected to IN2

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  // Turn relay ON (bulb should glow)
  digitalWrite(RELAY_PIN, LOW);   // ACTIVE-LOW
  delay(3000);                     // keep ON for 3 seconds

  // Turn relay OFF (bulb should go OFF)
  digitalWrite(RELAY_PIN, HIGH);  // relay OFF
  delay(3000);                     // keep OFF for 3 seconds
}
