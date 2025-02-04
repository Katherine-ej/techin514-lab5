#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include "esp_wifi.h"
#include "esp_sleep.h"


// === Ultrasonic Sensor Pins ===
#define TRIG_PIN 3
#define ECHO_PIN 2


// === Wi-Fi Credentials ===
#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "S{,<i=nbU5"


// === Firebase Configuration ===
#define DATABASE_SECRET "AIzaSyCL22hZPm4abauFVQXRp5TMDXys4r0Hhtg"
#define DATABASE_URL "https://techin514lab5-2dc12-default-rtdb.firebaseio.com/"


// === Firebase Objects ===
WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
LegacyToken dbSecret(DATABASE_SECRET);


// === Logic Parameters ===
#define MOVEMENT_THRESHOLD 50.0  // Distance below this value is considered as "object detected"
#define MEASURE_INTERVAL 5000    // Measure every 5 seconds
#define DEEP_SLEEP_DURATION 5    // Deep sleep for 5 seconds (each wakeup cycle)
#define SUSTAINED_THRESHOLD_DURATION 15000  // 15 seconds for sustained object presence below 50 cm


unsigned long sustainedMovementStartTime = 0;  // Tracks the time when object distance is less than 50 cm
bool isObjectDetected = false;  // Whether an object is detected within 50 cm


// === Connect to Wi-Fi ===
void connectToWiFi() {
   if (WiFi.status() == WL_CONNECTED) return;
   Serial.println("Connecting to WiFi...");
   WiFi.mode(WIFI_STA);
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   unsigned long startAttempt = millis();
   while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000) {
       delay(500);
   }
   if (WiFi.status() == WL_CONNECTED) {
       Serial.println("WiFi Connected.");
   } else {
       Serial.println("WiFi Connection Failed.");
   }
}


// === Initialize Firebase ===
void initFirebase() {
   Serial.println("Initializing Firebase...");
   ssl.setInsecure();
   initializeApp(client, app, getAuth(dbSecret));
   app.getApp<RealtimeDatabase>(Database);
   Database.url(DATABASE_URL);
   client.setAsyncResult(result);
}


// === Measure Ultrasonic Distance ===
float measureDistance() {
   digitalWrite(TRIG_PIN, LOW);
   delayMicroseconds(2);
   digitalWrite(TRIG_PIN, HIGH);
   delayMicroseconds(10);
   digitalWrite(TRIG_PIN, LOW);
   long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout of 30ms
   float distance = (duration * 0.0343f) / 2.0f;
   Serial.printf("Distance: %.2f cm\n", distance);
   return distance;
}


// === Disconnect Wi-Fi ===
void disconnectWiFi() {
   Serial.println("Forcing WiFi shutdown...");
   WiFi.disconnect(true);
   WiFi.mode(WIFI_OFF);
   esp_wifi_stop();
}


// === Send Data to Firebase (Only when conditions are met) ===
void sendDataToFirebase(float distance) {
   connectToWiFi();
   initFirebase();
   bool status = Database.set<float>(client, "/sensor/distance", distance);


   if (status) {
       Serial.println("Upload Success.");
   } else {
       Serial.println("Upload Failed.");
   }


   disconnectWiFi();
}


// === Enter Deep Sleep and Set Wakeup Every 5 Seconds ===
void enterDeepSleep() {
   Serial.printf("Entering deep sleep for %d seconds...\n", DEEP_SLEEP_DURATION);
  
   // Set timer to wake up after 5 seconds
   esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * 1000000ULL);  // 5 seconds
   esp_deep_sleep_start();
}


// === Check After Wakeup: Determine if Object is Persistently Detected ===
void checkAfterWakeup() {
   float distance = measureDistance();  // Measure the distance
   Serial.print("distance");
   Serial.println(distance);
   if (distance > 0 && distance < MOVEMENT_THRESHOLD) {
       // Object detected within 50 cm, start timer
       if (!isObjectDetected) {
           sustainedMovementStartTime = millis();  // Record the start time
           isObjectDetected = true;
           Serial.println("Object detected, starting timer...");
       }
   } else {
       // Object moved away, reset timer
       if (isObjectDetected) {
           sustainedMovementStartTime = 0;  // Reset the timer
           Serial.println("Object moved away, resetting timer...");
       }
       isObjectDetected = false;
   }


   // If object is detected for 15 seconds, send data to Firebase
   if (isObjectDetected && (millis() - sustainedMovementStartTime >= SUSTAINED_THRESHOLD_DURATION)) {
       sendDataToFirebase(distance);
       sustainedMovementStartTime = 0;  // Reset the timer after upload
       isObjectDetected = false;  // Reset object detection status
   }
}


void setup() {
   Serial.begin(115200);
   pinMode(TRIG_PIN, OUTPUT);
   pinMode(ECHO_PIN, INPUT);
   WiFi.mode(WIFI_OFF);
   esp_wifi_stop();


   // **Check if waking up from Deep Sleep and decide if to continue sleep**
   Serial.println("Woke up from Deep Sleep. Checking sensor...");
   checkAfterWakeup();


   //delay(5000);


   // Enter deep sleep and wait for the next wakeup
   enterDeepSleep();
}


void loop() {
   // Since ESP32 enters deep sleep in the setup, the loop is not used
   // After waking from deep sleep, the setup process will be executed again
}
