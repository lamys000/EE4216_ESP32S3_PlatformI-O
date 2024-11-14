#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include "time.h"
#include <HTTPClient.h>

// Telegram bot credentials
#define BOT_TOKEN "7745485363:AAFQeGyYECRdHTVSqLgckj0msOFiNMuvxDk"  // Replace with your actual bot token
#define CHAT_ID "1939482390"      // Replace with your actual chat ID

// ESP32-CAM server IP for taking photo
const char* serverName = "http://192.168.110.234/take_photo?trigger=EE4216";


// Include the token generation process and RTDB helper.
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Pixel_5744"
#define WIFI_PASSWORD "yslol235"

// Insert Firebase project API Key
#define API_KEY "AIzaSyB2ivesHz3wQM1cdIfNyAr2Sx3m-33Dopg"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "menjiying@gmail.com"
#define USER_PASSWORD "Hidejy666"

// Insert RTDB URL
#define DATABASE_URL "https://ee4216-project-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
String tempPath = "/temperature";
String humPath = "/humidity";
String gasPath = "/gas";
String flamePath = "/flame";
String soundPath = "/sound";
String pirPath = "/pir";
String timePath = "/timestamp";

// Sensor pins
#define DHTPIN 4         // DHT22 sensor pin
#define DHTTYPE DHT11    // DHT sensor type
#define PIRPIN 5         // PIR sensor pin
#define MQ2PIN 13        // MQ2 gas sensor pin (Analog)                                                                                                                                                                                                                                                                       
#define FLAMEPIN 16      // Flame sensor digital  pin
#define MICPIN 10      // Microphone  digital pin

DHT dht(DHTPIN, DHTTYPE);

// Timer variables
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 10000;  // 3 minutes

// Firebase Cloud Messaging variables
String camModuleUrl = "http://<ESP32-CAM-IP>/capture";  // URL of the ESP32-CAM module for capturing photos

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// Function to get the current time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return (0);
  }
  time(&now);
  return now;
}

// Function to initialize sensors
void initSensors() {
  dht.begin();
  pinMode(PIRPIN, INPUT);
  pinMode(FLAMEPIN, INPUT);
  pinMode(MICPIN, INPUT);
}

void sendTeleAlert() {
  HTTPClient http;
  String serverPath = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=Flame%20or%20smoke%20detected!%20Please%20check%20the%20web%20app.";

  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.println("Telegram alert sent successfully.");
  } else {
    Serial.println("Error sending Telegram alert.");
  }

  http.end();
}


// Function to send alert to ESP32-CAM
void sendCamAlert() {

  HTTPClient http;
  http.begin(serverName);
  
  int httpResponseCode = http.GET();  // Send the request
  if (httpResponseCode > 0) {
    Serial.println("Photo taken successfully.");
  } else {
    Serial.println("Error taking photo.");
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initSensors();

  configTime(0, 0, "pool.ntp.org");

  // Firebase configuration
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);

  // Wait for user UID
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }

  // Set user UID and database path
  uid = auth.token.uid.c_str();
  databasePath = "/UsersData/" + uid + "/readings";
  
}

void loop() {
  // Send new readings every 3 minutes
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    int timestamp = getTime();
    String parentPath = databasePath + "/" + String(timestamp);

    // Read sensor data
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int gasLevel = analogRead(MQ2PIN);  // Read gas level from MQ2
    int flameDetected = digitalRead(FLAMEPIN);  // Read flame sensor state
    int soundDetected = digitalRead(MICPIN);    // Read microphone state
    int motionDetected = digitalRead(PIRPIN);   // Read PIR sensor state

    // Log sensor data to Firebase
    FirebaseJson json;
    json.set(tempPath.c_str(), String(temperature));
    json.set(humPath.c_str(), String(humidity));
    json.set(gasPath.c_str(), String(gasLevel));
    json.set(flamePath.c_str(), flameDetected ? "Flame Detected" : "No Flame");
    json.set(soundPath.c_str(), soundDetected ? "Sound Detected" : "No Sound");
    json.set(pirPath.c_str(), motionDetected ? "Motion Detected" : "No Motion");
    json.set(timePath, String(timestamp));

    // Send data to Firebase
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());

    // If motion is detected, send alert to ESP32-CAM
    if (motionDetected) {
      sendCamAlert();
      Serial.println("Motion detected, alert sent to ESP32-CAM.");
      
    }

    if (gasLevel > 2000 || flameDetected == 1) {
      sendTeleAlert();
      Serial.println("Flame or smoke detected! Please check the web app.");
    }
  }
}
