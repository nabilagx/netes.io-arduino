#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- WiFi & MQTT ---
const char* ssid = "Galaxtya";
const char* password = "mYnameisGX";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --- Firebase ---
#define API_KEY "AIzaSyDGl0Zxs8ZhHhKQZvb4h6DFE-VGafctwVc"
#define DATABASE_URL "https://netes-io-default-rtdb.firebaseio.com"
#define USER_EMAIL "netesio@gmail.com"
#define USER_PASSWORD "12345678"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- Pins ---
#define RELAY1 27   // Motor rotasi telur
#define RELAY2 26   // Lampu pemanas

#define DHTPIN 32
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);


bool relay1State = HIGH;
bool relay2State = HIGH;



const bool relay1Pin = RELAY1;
const bool relay2Pin = RELAY2;

// --- Timing & State ---
unsigned long motorMillis = 0;
const unsigned long motorOnDuration = 24000; // 24 detik ON
unsigned long motorOffDuration = 6UL * 3600000UL; // default OFF 6 jam (6 jam * 3600 detik * 1000 ms)

bool motorActive = false;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 30000; // 30 detik

unsigned long lastDurasiCheck = 0;
const unsigned long durasiInterval = 86400000; // 24 jam


bool manualControlRelay2 = false;
unsigned long manualControlTimeout = 0;
const unsigned long manualControlDuration = 60000; // 1 menit timeout manual control relay2


// --- MQTT Callback ---
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.printf("MQTT message received on topic [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == "esp32/GXrelay2") {
    manualControlRelay2 = true;
    manualControlTimeout = millis();
    if (msg.equalsIgnoreCase("ON")) {
      digitalWrite(RELAY2, LOW);
      Serial.println("Relay2 turned ON (manual via MQTT)");
    } else if (msg.equalsIgnoreCase("OFF")) {
      digitalWrite(RELAY2, HIGH);
      Serial.println("Relay2 turned OFF (manual via MQTT)");
    }
  }

  if (String(topic) == "netesio/relay1/manual_trigger") {
    if (msg.equalsIgnoreCase("start")) {
      Serial.println("Manual trigger relay1 ON for 24 seconds");
      digitalWrite(RELAY1, LOW);
      delay(motorOnDuration);
      digitalWrite(RELAY1, HIGH);
      Serial.println("Manual trigger relay1 OFF");
      uploadRotasiEvent(); // Catat event rotasi manual ke Firebase
    }
  }
}

// --- MQTT reconnect ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      client.subscribe("esp32/GXrelay2");
      client.subscribe("netesio/relay1/manual_trigger");
      Serial.println("Subscribed to MQTT topics");
    } else {
      Serial.printf("failed, rc=%d. Trying again in 5 seconds\n", client.state());
      delay(5000);
    }
  }
}

// --- Baca pengaturan dari Firebase ---
void bacaFirebasePengaturan() {
  Serial.println("Reading settings from Firebase...");

  // Baca jadwal_rotasi
  if (Firebase.RTDB.getInt(&fbdo, "jadwal_rotasi")) {
    int jr = fbdo.intData();
    Serial.printf("jadwal_rotasi: %d\n", jr);

    if (jr <= 0) jr = 6; // default 6 jam

    unsigned long newDuration;
    if (jr == 1) {
      // 3 menit
      newDuration = 180000UL;
    } else {
      newDuration = (unsigned long)jr * 3600000UL;
    }

    if (newDuration != motorOffDuration) {
      motorOffDuration = newDuration;
      motorActive = true;
      motorMillis = millis();
    }

    
    
  } else {
    Serial.printf("Failed to get jadwal_rotasi: %s\n", fbdo.errorReason().c_str());
  }

  // Baca jenis_kelamin & kontrol relay2 otomatis
  if (Firebase.RTDB.getString(&fbdo, "jenis_kelamin")) {
    String jk = fbdo.stringData();
    Serial.printf("jenis_kelamin: %s\n", jk.c_str());

    float suhu = dht.readTemperature();
    if (isnan(suhu)) {
      Serial.println("Failed to read temperature from DHT sensor");
      return;
    }
    Serial.printf("Current temperature: %.2f C\n", suhu);

    if (!manualControlRelay2) {
      bool relay2On = false;
      if (jk == "jantan") {
        relay2On = (suhu < 37.0);
      } else if (jk == "seimbang") {
        relay2On = (suhu < 37.5);
      } else if (jk == "betina") {
        relay2On = (suhu < 38.0);
      }

      if (relay2On) {
        digitalWrite(RELAY2, LOW);
        Serial.println("Relay2 ON (auto control based on jenis_kelamin & suhu)");
        // Update status relay2 di RTDB jadi ON (true)
          Firebase.RTDB.setBool(&fbdo, "status/relay2", true);
          } else {
        digitalWrite(RELAY2, HIGH);
        Serial.println("Relay2 OFF (auto control based on jenis_kelamin & suhu)");
          // Update status relay2 di RTDB jadi OFF (false)
        Firebase.RTDB.setBool(&fbdo, "status/relay2", false);
      }
    } else {
      Serial.println("Manual control active on Relay2, skipping auto control");
    }
  } else {
    Serial.printf("Failed to get jenis_kelamin: %s\n", fbdo.errorReason().c_str());
  }
}

// --- Upload data sensor ke Firebase ---
void uploadDataSensor(float suhu, float hum) {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready, skipping data upload.");
    return;
  }

  String path = "riwayat/" + String(millis());
  Serial.printf("Uploading sensor data to Firebase at path: %s\n", path.c_str());

  if (Firebase.RTDB.setFloat(&fbdo, path + "/suhu", suhu)) {
    Serial.println("Suhu uploaded");
  } else {
    Serial.printf("Failed to upload suhu: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.setFloat(&fbdo, path + "/kelembaban", hum)) {
    Serial.println("Kelembaban uploaded");
  } else {
    Serial.printf("Failed to upload kelembaban: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.setInt(&fbdo, path + "/timestamp", millis())) {
    Serial.println("Timestamp uploaded");
  } else {
    Serial.printf("Failed to upload timestamp: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.setString(&fbdo, path + "/type", "suhu_kelembaban")) {
    Serial.println("Type uploaded");
  } else {
    Serial.printf("Failed to upload type: %s\n", fbdo.errorReason().c_str());
  }
}


// --- Update durasi inkubasi tiap 24 jam ---
void updateDurasi() {
  if (Firebase.RTDB.getInt(&fbdo, "durasi_inkubasi")) {
    int durasi = fbdo.intData();
    Serial.printf("Current durasi_inkubasi: %d\n", durasi);
    int durasiBaru = max(durasi - 1, 0);
    bool ok = Firebase.RTDB.setInt(&fbdo, "durasi_inkubasi", durasiBaru);
    if (ok) {
      Serial.printf("Updated durasi_inkubasi to: %d\n", durasiBaru);
      String path = "riwayat/" + String(millis());
      Firebase.RTDB.setInt(&fbdo, path + "/durasi_inkubasi_sisa", durasiBaru);
      Firebase.RTDB.setInt(&fbdo, path + "/timestamp", millis());
      Firebase.RTDB.setString(&fbdo, path + "/type", "update_durasi_inkubasi");
    } else {
      Serial.printf("Failed to update durasi_inkubasi: %s\n", fbdo.errorReason().c_str());
    }
  } else {
    Serial.printf("Failed to get durasi_inkubasi: %s\n", fbdo.errorReason().c_str());
  }
}

// --- Upload event rotasi telur ---
void uploadRotasiEvent() {
  String path = "riwayat/" + String(millis());
  Serial.printf("Uploading rotasi telur event at path: %s\n", path.c_str());

  bool ok = Firebase.RTDB.setString(&fbdo, path + "/detail", "rotasi_telur_24detik");
  if (!ok) Serial.printf("Failed to upload rotasi detail: %s\n", fbdo.errorReason().c_str());

  ok &= Firebase.RTDB.setInt(&fbdo, path + "/timestamp", millis());
  if (!ok) Serial.printf("Failed to upload timestamp: %s\n", fbdo.errorReason().c_str());

  ok &= Firebase.RTDB.setString(&fbdo, path + "/type", "rotasi_telur");
  if (!ok) Serial.printf("Failed to upload type: %s\n", fbdo.errorReason().c_str());

  if (ok) Serial.println("Rotasi telur event uploaded successfully");
  else Serial.println("Failed to upload rotasi telur event");
}

void uploadStatusDevices(bool relay1Status, bool relay2Status) {
  String path = "status_devices"; // path di RTDB untuk simpan status
  bool ok = true;

  // Relay1: OFF = HIGH, ON = LOW, supaya lebih jelas, simpan boolean ON/OFF (true=ON)
  ok &= Firebase.RTDB.setBool(&fbdo, path + "/relay1", relay1Status);
  if (!ok) Serial.printf("Failed to upload relay1 status: %s\n", fbdo.errorReason().c_str());

  ok &= Firebase.RTDB.setBool(&fbdo, path + "/relay2", relay2Status);
  if (!ok) Serial.printf("Failed to upload relay2 status: %s\n", fbdo.errorReason().c_str());

  if (ok) Serial.println("Device statuses uploaded successfully");
  else Serial.println("Failed to upload some device statuses");
}


void setup() {
  Serial.begin(115200);


  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);

  // Set initial states

  digitalWrite(relay1Pin, relay1State);
  digitalWrite(relay2Pin, relay2State);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
 

  digitalWrite(RELAY1, HIGH); // Relay OFF
  digitalWrite(RELAY2, HIGH); // Relay OFF


  dht.begin();

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Setup Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Setup complete");
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  // Sensor baca dan upload setiap 5 detik
  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;
    float suhu = dht.readTemperature();
    float hum = dht.readHumidity();

    if (isnan(suhu) || isnan(hum)) {
      Serial.println("Failed to read from DHT sensor");
   
    } else {
      Serial.printf("Temperature: %.2f C, Humidity: %.2f %%\n", suhu, hum);

      // Upload data sensor
      uploadDataSensor(suhu, hum);

      // Baca pengaturan dari Firebase dan kontrol relay2
      bacaFirebasePengaturan();

      
    }
  }

   // Motor rotasi telur ON selama 24 detik
  if (motorActive) {
    unsigned long elapsed = now - motorMillis;
    if (elapsed < motorOnDuration) {
      digitalWrite(RELAY1, LOW); // ON
      if (relay1State != LOW) {
        relay1State = LOW;
        uploadStatusDevices(relay1State == LOW, relay2State == LOW);
      }
    } else if (elapsed < motorOnDuration + motorOffDuration) {
      digitalWrite(RELAY1, HIGH); // OFF
      if (relay1State != HIGH) {
        relay1State = HIGH;
        uploadStatusDevices(relay1State == LOW, relay2State == LOW);
      }
    } else {
      motorMillis = now;
      digitalWrite(RELAY1, LOW);
      if (relay1State != LOW) {
        relay1State = LOW;
        uploadStatusDevices(relay1State == LOW, relay2State == LOW);
      }
      Serial.println("Relay1 motor rotasi ON 24 detik cycle restart");
    }
  } else {
    digitalWrite(RELAY1, HIGH);
    if (relay1State != HIGH) {
      relay1State = HIGH;
      uploadStatusDevices(relay1State == LOW, relay2State == LOW);
    }
  }


  // Jika manual control relay2 aktif, cek timeout 1 menit
  if (manualControlRelay2 && (now - manualControlTimeout > manualControlDuration)) {
    manualControlRelay2 = false;
    Serial.println("Manual control relay2 timeout, kembali ke kontrol otomatis");
  }

  // Update durasi inkubasi setiap 24 jam
  if (now - lastDurasiCheck > durasiInterval) {
    lastDurasiCheck = now;
    updateDurasi();
  }

  delay(100);
}
