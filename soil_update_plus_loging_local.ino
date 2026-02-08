#include <WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <time.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>

// === RS485 PIN ===
#define RE_PIN 4
#define RXD2 17
#define TXD2 16

// === SD Card CS PIN ===
#define SD_CS 5

// === WIFI & MQTT ===
const char* ssid         = "Raspi";
const char* password     = "12345678";
const char* mqtt_server  = "145.79.8.95";
const int   mqtt_port    = 1883;

// === Nama Tanaman & Varietas ===
const char* plant_name = "Tomat";
const char* variety    = "Mawar Merah";

WiFiClient espClient;
PubSubClient client(espClient);
ModbusMaster node;
RTC_DS3231 rtc;

// === RS485 Hooks ===
void preTransmission() {
  digitalWrite(RE_PIN, HIGH);
}
void postTransmission() {
  digitalWrite(RE_PIN, LOW);
}

// === Setup WiFi ===
void setup_wifi() {
  delay(10);
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung. IP: " + WiFi.localIP().toString());
}

// === Reconnect MQTT ===
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Terhubung ke MQTT broker!");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

// === Timestamp dari NTP (WIB) ===
String getLocalTimestamp() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
  return String(buf);
}

// === Timestamp dari RTC saat offline ===
String getRTCTimestamp() {
  DateTime now = rtc.now();
  char buf[25];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());
  return String(buf);
}

// === Baca Sensor ===
float readSensor(uint16_t reg, float factor) {
  uint8_t result = node.readHoldingRegisters(reg, 1);
  if (result == node.ku8MBSuccess) {
    uint16_t raw = node.getResponseBuffer(0);
    return raw * factor;
  } else {
    Serial.print("Gagal baca reg 0x");
    Serial.println(reg, HEX);
    return -1;
  }
}

// === Inisialisasi SD ===
void initSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Gagal inisialisasi SD card!");
  } else {
    Serial.println("SD card siap digunakan.");
    if (!SD.exists("/log.csv")) {
      File file = SD.open("/log.csv", FILE_WRITE);
      file.println("timestamp,temperature,humidity,tds,co2,conductivity,pH,nitrogen,phosphorus,potassium,salinity");
      file.close();
    }
  }
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);

  pinMode(RE_PIN, OUTPUT);
  digitalWrite(RE_PIN, LOW);

  node.begin(1, Serial2); // Slave ID
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Timezone WIB (GMT+7)
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sinkronisasi waktu NTP");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWaktu tersinkronisasi.");

  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
  } else {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    rtc.adjust(DateTime(1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec));
    Serial.println("RTC disinkronisasi dari NTP.");
  }

  initSD();
  Serial.println("Monitoring sensor dimulai...");
}

// === LOOP ===
void loop() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    reconnect();
  }
  client.loop();

  // === Baca Sensor ===
  float humidity     = readSensor(0x0000, 0.1);
  float temperature  = readSensor(0x0001, 0.1);
  float conductivity = readSensor(0x0002, 1.0);
  float pH           = readSensor(0x0003, 0.1);
  float nitrogen     = readSensor(0x0004, 1.0);
  float phosphorus   = readSensor(0x0005, 1.0);
  float potassium    = readSensor(0x0006, 1.0);
  float salinity     = readSensor(0x0007, 1.0);
  float tds          = readSensor(0x0008, 1.0);
  float co2          = 99; // placeholder

  String timestamp = (WiFi.status() == WL_CONNECTED) ? getLocalTimestamp() : getRTCTimestamp();

  // === JSON Payload ===
  String json = "{";
  json += "\"plant_name\":\""   + String(plant_name) + "\",";
  json += "\"variety\":\""      + String(variety) + "\",";
  json += "\"timestamp\":\""    + timestamp + "\",";
  json += "\"temperature\":"    + String(temperature, 1) + ",";
  json += "\"humidity\":"       + String(humidity, 1) + ",";
  json += "\"tds\":"            + String(tds, 0) + ",";
  json += "\"co2\":"            + String(co2, 0) + ",";
  json += "\"conductivity\":"   + String(conductivity, 0) + ",";
  json += "\"pH\":"             + String(pH, 1) + ",";
  json += "\"nitrogen\":"       + String(nitrogen, 0) + ",";
  json += "\"phosphorus\":"     + String(phosphorus, 0) + ",";
  json += "\"potassium\":"      + String(potassium, 0) + ",";
  json += "\"salinity\":"       + String(salinity, 0);
  json += "}";

  Serial.println("Data:");
  Serial.println(json);

  // === Kirim MQTT jika online ===
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    client.publish("sensor/data", json.c_str());
  }

  // === Simpan ke SD card ===
  File file = SD.open("/log.csv", FILE_APPEND);
  if (file) {
    file.print(timestamp); file.print(",");
    file.print(temperature, 1); file.print(",");
    file.print(humidity, 1); file.print(",");
    file.print(tds, 0); file.print(",");
    file.print(co2, 0); file.print(",");
    file.print(conductivity, 0); file.print(",");
    file.print(pH, 1); file.print(",");
    file.print(nitrogen, 0); file.print(",");
    file.print(phosphorus, 0); file.print(",");
    file.print(potassium, 0); file.print(",");
    file.println(salinity, 0);
    file.close();
    Serial.println("✅ Data disimpan ke SD card.");
  } else {
    Serial.println("❌ Gagal menyimpan ke SD card.");
  }

  delay(5000); // jeda antar log
}
