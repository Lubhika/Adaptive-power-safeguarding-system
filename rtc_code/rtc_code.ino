#include <Wire.h>
#include "RTClib.h"

#define RELAY_PIN 32   // IN2 (as you tested)

RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);

  // Default OFF
  digitalWrite(RELAY_PIN, HIGH);

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }
}

void loop() {
  DateTime now = rtc.now();

  int hour = now.hour();
  int minute = now.minute();

  Serial.print("Time: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);

  // 🔥 DEMO LOGIC
  // After 20:50 → OFF
  if (hour > 20 || (hour == 20 && minute >= 54)) {
    digitalWrite(RELAY_PIN, HIGH);  // OFF
    Serial.println("Bulb OFF (Night rule)");
  } else {
    digitalWrite(RELAY_PIN, LOW);   // ON
    Serial.println("Bulb ON");
  }

  delay(2000);
}
