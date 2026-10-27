/*
 * ==============================================================================
 * PRAKTIKUM 7: SIMPUL SENSOR-AKTUATOR IOT UNTUK BROKER LOKAL
 * ==============================================================================
 * Membaca sensor DHT11, mengirim telemetri JSON ke Mosquitto lokal,
 * dan menerima instruksi kendali LED melalui MQTT.
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Wi-Fi
const char* ssid     = "Feni";
const char* password = "12121212";

// 2. Konfigurasi Broker MQTT Lokal
// Sesuaikan dengan IPv4 laptop dari perintah ipconfig.
const char* mqttServer = "172.20.10.3";
const int mqttPort = 1883;

// 3. Autentikasi MQTT
const char* mqttUser = "admin";
const char* mqttPassword = "rahasia123";

// 4. Identitas Simpul dan Topik MQTT
const char* clientId = "ESP8266_Local_Node01";
const char* topicPubTele = "kampus/labiot/telemetri";
const char* topicSubCmd = "kampus/labiot/kendali";

// 5. Konfigurasi Pin
const byte dhtPin = 2;  // D4 (GPIO 2)
const byte ledPin = 5;  // D1 (GPIO 5)

#define DHTTYPE DHT11

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000;

// Menerima instruksi dari Node-RED melalui MQTT
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]: %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);

    Serial.printf(
      "[AKTUATOR] Status LED: %s\n",
      stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)"
    );
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan!");
  }

  Serial.println("----------------------------------------");
}

// Menghubungkan ESP8266 ke Wi-Fi
void connectToWiFi() {
  Serial.println();
  Serial.print("[Wi-Fi] Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Berhasil Terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

// Menghubungkan ESP8266 ke broker MQTT dengan autentikasi
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan ke Broker Lokal (");
    Serial.print(mqttServer);
    Serial.print(")...");

    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println(" Terhubung dengan Autentikasi Sah!");

      if (client.subscribe(topicSubCmd)) {
        Serial.printf(
          "[MQTT] Berlangganan ke topik: %s\n",
          topicSubCmd
        );
      } else {
        Serial.println("[MQTT] Gagal subscribe ke topik kendali!");
      }
    } else {
      Serial.printf(
        " Gagal (rc=%d). Coba lagi dalam 4 detik...\n",
        client.state()
      );
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  unsigned long now = millis();

  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;

      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      if (client.publish(topicPubTele, teleBuffer)) {
        Serial.printf(
          "[TELEMETRI TERKIRIM] %s\n",
          teleBuffer
        );
      } else {
        Serial.println(
          "[ERROR] Gagal mengirim telemetri ke broker!"
        );
      }
    } else {
      Serial.println(
        "[ERROR] Gagal membaca sensor DHT!"
      );
    }
  }
}