#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ---------------------- Firebase & WiFi Configuration ----------------------
#define FIREBASE_HOST "iotlab-b375f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "6wYImmhtOvrKdmsjMDdpR8NaKfoqNsW3F9n0Vzj3"
#define WIFI_SSID "gres"
#define WIFI_PASSWORD "griet7575"
String tag = "IOTLAB/Pedestrian_Safety/Status";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------------------- Sensor & Display Configuration ----------------------
Adafruit_BMP280 bmp;
#define SEALEVELPRESSURE_HPA (1013.25)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------- Pin Definitions ----------------------
#define IR_SENSOR 4
#define PIR_SENSOR 33
#define ULTRASONIC_TRIG 25
#define ULTRASONIC_ECHO 26
#define BUZZER 32
#define RELAY 13
#define BUILTIN_LED 2

// ---------------------- Global Variables ----------------------
const float CRITICAL_DISTANCE_CM = 100.0; // 1 meter
unsigned long lastSirenToggle = 0;
String currentStatus = "";
float lastDistance = 999.0;

// ---------------------- Function Prototypes ----------------------
float measureDistance();
void updateOutputs(bool alertPedestrians, bool showWarning);
void displayStatus(const String &line1, const String &line2, const String &line3);
void alertSiren();
void safeDisplay();

// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);

  pinMode(IR_SENSOR, INPUT);
  pinMode(PIR_SENSOR, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  ledcSetup(0, 1000, 8);
  ledcAttachPin(BUZZER, 0);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED not found!");
  }

  if (!bmp.begin(0x76)) {
    Serial.println("❌ BMP280 not found!");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  displayStatus("Pedestrian Safety", "Initializing...", "Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  displayStatus("Pedestrian Safety", "System Ready", "Monitoring...");
  delay(1500);
}

// ---------------------- Loop ----------------------
void loop() {
  int irValue = digitalRead(IR_SENSOR);
  bool irDetected = (irValue == HIGH); // Change to LOW if your IR is active LOW
  bool pirDetected = (digitalRead(PIR_SENSOR) == HIGH);
  bool alertPedestrians = false;
  bool showWarning = false;

  Serial.print("IR: ");
  Serial.print(irValue);
  Serial.print(" | PIR: ");
  Serial.println(digitalRead(PIR_SENSOR));

  if (irDetected) {
    if (pirDetected) {
      // PIR detected motion → measure distance
      lastDistance = measureDistance();
      Serial.print("Distance: ");
      Serial.println(lastDistance);

      if (lastDistance <= CRITICAL_DISTANCE_CM) {
        // Pedestrians very close — stop vehicles
        alertPedestrians = true;
        showWarning = true;
        displayStatus("🚨 STOP!", "Pedestrians on road", "Dist: " + String(lastDistance, 1) + " cm");
        currentStatus = "STOP! Pedestrians on road";
      } else {
        // PIR detected motion, but distance is safe
        showWarning = true;
        displayStatus("⚠️ Slow Down!!", "Pedestrians Ahead", "Drive Carefully");
        delay(500);
        currentStatus = "Slow Down!!";
      }
    } else {
      // IR detects, but no PIR motion — safe
      safeDisplay();
      currentStatus = "Safe to Drive";
    }
  } else {
    // No IR detection — safe
    safeDisplay();
    currentStatus = "Safe to Drive";
  }

  // Update outputs
  updateOutputs(alertPedestrians, showWarning);

  // Firebase update only when status changes
  static String lastStatus = "";
  if (currentStatus != lastStatus) {
    if (WiFi.isConnected() && Firebase.ready()) {
      Firebase.RTDB.setString(&fbdo, tag, currentStatus);
      lastStatus = currentStatus;
    }
  }

  delay(100); // small loop delay for stability
}

// ---------------------- Helper Functions ----------------------

float measureDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 50000);
  float distance = duration * 0.034 / 2;
  if (distance > 400 || distance <= 0) return 999.0;
  return distance;
}

void updateOutputs(bool alertPedestrians, bool showWarning) {
  if (alertPedestrians) {
    // Critical proximity — alternating siren tone
    alertSiren();
    digitalWrite(RELAY, HIGH);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(500);
  } else if (showWarning) {
    // Slow down — steady tone
    ledcWriteTone(0, 1000);
    digitalWrite(RELAY, HIGH);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(500);
  } else {
    // Safe — everything off
    ledcWrite(0, 0);
    digitalWrite(RELAY, LOW);
    digitalWrite(BUILTIN_LED, LOW);
  }
}

void alertSiren() {
  unsigned long currentTime = millis();
  if (currentTime - lastSirenToggle >= 200) {
    if (currentTime % 400 < 200) {
      ledcWriteTone(0, 400);
    } else {
      ledcWriteTone(0, 600);
    }
    lastSirenToggle = currentTime;
  }

  delay(200);
}

void safeDisplay() {
  ledcWrite(0, 0);
  digitalWrite(RELAY, LOW);
  digitalWrite(BUILTIN_LED, LOW);

  float tempC = bmp.readTemperature();
  displayStatus("🟢 Safe to Drive", "Temp: " + String(tempC, 1) + " C", "");
}

void displayStatus(const String &line1, const String &line2, const String &line3) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 15);
  display.println(line2);
  display.setCursor(0, 30);
  display.println(line3);
  display.display();
}
