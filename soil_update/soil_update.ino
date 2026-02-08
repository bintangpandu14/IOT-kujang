#include <WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <time.h>

// === RS485 PIN ===
#define RE_PIN 4
#define RXD2 17
#define TXD2 16

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

// === WIB Timestamp Format for MySQL ===
String getLocalTimestamp() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now); // Gunakan localtime (bukan gmtime)
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t); // MySQL-friendly format
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

  // ⏰ Set timezone ke WIB (GMT+7 → 25200 detik)
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sinkronisasi waktu NTP");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWaktu tersinkronisasi.");
  Serial.println("Monitoring sensor dimulai...");
}

// === LOOP ===
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Baca data sensor
  float humidity     = readSensor(0x0000, 0.1);
  float temperature  = readSensor(0x0001, 0.1);
  float conductivity = readSensor(0x0002, 1.0);
  float pH           = readSensor(0x0003, 0.1);
  float nitrogen     = readSensor(0x0004, 1.0);
  float phosphorus   = readSensor(0x0005, 1.0);
  float potassium    = readSensor(0x0006, 1.0);
  float salinity     = readSensor(0x0007, 1.0);
  float tds          = readSensor(0x0008, 1.0);
  float co2          = 99; // placeholder jika tidak ada sensor CO2

  String timestamp = getLocalTimestamp(); // Sudah WIB dan MySQL format

  // Buat JSON payload
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

  Serial.println("Kirim ke MQTT:");
  Serial.println(json);

  client.publish("sensor/data", json.c_str());

  delay(5000); // delay antar pengiriman
}
