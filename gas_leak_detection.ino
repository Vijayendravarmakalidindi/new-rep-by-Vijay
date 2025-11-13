#include <Servo.h>

const int mq2Pin = A0;       // MQ-2 sensor analog pin
const int buzzerPin = 8;     // Buzzer pin
const int threshold = 400;   // Gas threshold value (adjust based on testing)

Servo regulatorServo;

void setup() {
  Serial.begin(9600);
  pinMode(buzzerPin, OUTPUT);
  regulatorServo.attach(9);       // Servo motor pin
  regulatorServo.write(0);        // Start in OFF position
  Serial.println("LPG Gas Detector System Initialized");
}

void loop() {
  int gasLevel = analogRead(mq2Pin);
  Serial.print("Gas Level: ");
  Serial.println(gasLevel);

  if (gasLevel > threshold) {
    digitalWrite(buzzerPin, HIGH);   // Turn ON buzzer
    regulatorServo.write(90);        // Turn servo to shut off valve
    Serial.println("!! Gas Leak Detected !! Valve Closed");
  } else {
    digitalWrite(buzzerPin, LOW);    // Turn OFF buzzer
    regulatorServo.write(0);         // Valve remains open
  }

  delay(1000);  // Delay for stability
}