#include "WiFiS3.h"

// --- ENTER YOUR DETAILS HERE ---
const char ssid[] = "iPhone";  // WiFi Name
const char pass[] = "cookedoden";            // WiFi Password
const char FIREBASE_HOST[] = "smartparkingiot-a9468-default-rtdb.europe-west1.firebasedatabase.app"; // WITHOUT https:// and WITHOUT trailing /

// --- PIN SETTINGS ---
const int sensorPins[3] = {A0, A1, A2}; // The 3 KY-035 sensors
const int batteryPin = A3;              // Cable from the battery voltage divider

// --- SENSOR SETTINGS ---
const int centerValue = 700; // The idle center value (0-1023)
const int threshold = 50;    // Sensitivity threshold (may need tuning with the actual magnet)

// Variables to remember the previous state
bool lastStatus[3] = {false, false, false};

// Client for secure HTTPS connection
WiFiSSLClient client;

void setup() {
  Serial.begin(9600);
  delay(2000); // Small pause to allow Serial Monitor to open properly

  Serial.println("\n--- Smart Parking Starting (Arduino R4 WiFi) ---");

  // Set pins as inputs
  for(int i = 0; i < 3; i++) {
    pinMode(sensorPins[i], INPUT);
  }
  pinMode(batteryPin, INPUT);

  // Connect to WiFi
  connectToWiFi();
}

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 2000;

void loop() {
  bool somethingChanged = false;
  bool currentStatus[3];

  // 1. READ THE 3 SENSORS
  for(int i = 0; i < 3; i++) {
    int val = analogRead(sensorPins[i]);
    currentStatus[i] = (abs(val - centerValue) > threshold);

    if (currentStatus[i] != lastStatus[i]) {
      somethingChanged = true;
      lastStatus[i] = currentStatus[i];
    }
  }

  // 2. IF SOMETHING CHANGED -> SEND DATA TO FIREBASE
  if (somethingChanged) {
    Serial.println("\n[!] Change detected! Preparing to send...");

    int batVal = analogRead(batteryPin);
    int batPercent = map(batVal, 0, 1023, 0, 100);

    String jsonPayload = "{";
    for(int i = 0; i < 3; i++) {
      String statusStr = currentStatus[i] ? "OCCUPIED" : "EMPTY";
      jsonPayload += "\"spot_" + String(i+1) + "\": {\"status\": \"" + statusStr + "\"}, ";
    }
    jsonPayload += "\"battery\": " + String(batPercent) + "}";

    Serial.println("Data to send: " + jsonPayload);
    sendToFirebase(jsonPayload);
  }

  // 3. PRINT VALUES EVERY 2 SECONDS
  if (millis() - lastPrintTime >= printInterval) {
    Serial.print("Sensor values -> ");
    for(int i = 0; i < 3; i++) {
      Serial.print("Spot ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(analogRead(sensorPins[i]));
      Serial.print("  ");
    }
    Serial.println();
    lastPrintTime = millis();
  }

  delay(1000);
}
// ==========================================
// HELPER FUNCTIONS
// ==========================================

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected successfully! IP: " + WiFi.localIP().toString());
}

void sendToFirebase(String jsonData) {
  // Make sure WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectToWiFi();
  }

  Serial.println("Connecting to Firebase...");
  if (client.connect(FIREBASE_HOST, 443)) {

    // HTTP PUT Request
    client.println("PUT /parking.json HTTP/1.1");
    client.print("Host: ");
    client.println(FIREBASE_HOST);
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println(); // Empty line
    client.println(jsonData);

    // Wait briefly for data to be sent
    delay(100);
    client.stop();
    Serial.println("-> Firebase updated successfully!\n");

  } else {
    Serial.println("-> ERROR: Failed to connect to Firebase.\n");
  }
}