#define BLYNK_TEMPLATE_ID "TMPL6FDJE84PX"
#define BLYNK_TEMPLATE_NAME "CAMPURAN NUTRISI"
#define BLYNK_AUTH_TOKEN "a09dpWKKRPT6Tr7h9e3185bLJHcKoRYW"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"

// WiFi credentials
char ssid[] = "JERANTI";
char pass[] = "kujang2022";
char auth[] = BLYNK_AUTH_TOKEN;

// DHT config
#define DHTPIN 25
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// TDS config
#define TDS_PIN 32

// Pompa config
#define IN1 14
#define IN2 12
#define ENA 13
#define IN3 18
#define IN4 19
#define ENB 21

// sensor pH
#define PH_ANALOG_PIN  35
#define PH_DIGITAL_PIN 26
int phADC;
float voltage, pHValue;

// Solenoid valve config
#define SOLENOID_PIN 4  
int jarak = 0;

BlynkTimer timer;

float temperature = 25.0;
float tdsValue;

// -------------------------- BLYNK WRITE (INPUT V5) --------------------------
BLYNK_WRITE(V4) {
  String inputText = param.asStr();
  jarak = inputText.toInt(); // ubah ke integer

  if (jarak > 0) {
    Serial.print("[JARAK - BLYNK] Diterima: "); Serial.print(jarak); Serial.println(" cm");

    if (jarak < 10) {
      Serial.println("[SOLENOID] Jarak < 10 cm → Solenoid NONAKTIF (OFF)");
      digitalWrite(SOLENOID_PIN, LOW);
      Blynk.virtualWrite(V12, "Pipa Mati");
    } else {
      Serial.println("[SOLENOID] Jarak >= 10 cm → Solenoid AKTIF (ON)");
      digitalWrite(SOLENOID_PIN, HIGH);
      Blynk.virtualWrite(V12, "Pipa Hidup");
    }
  } else {
    Serial.println("[BLYNK INPUT] Nilai jarak tidak valid.");
    Blynk.virtualWrite(V12, "Input tidak valid");
  }
}

// -------------------------- Fungsi Baca Sensor --------------------------

void readAndSendTDS() {
  int analogValue = analogRead(TDS_PIN);
  float voltage = analogValue * (3.3 / 4095.0);
  float ec = (133.42 * pow(voltage, 3)) - (255.86 * pow(voltage, 2)) + (857.39 * voltage);
  float ecCorrected = ec / (1.0 + 0.02 * (temperature - 25.0));
  tdsValue = ecCorrected * 0.5;

  Serial.print("[TDS] Analog: "); Serial.print(analogValue);
  Serial.print(" | Voltage: "); Serial.print(voltage, 3); Serial.print(" V");
  Serial.print(" | EC: "); Serial.print(ecCorrected, 2); Serial.print(" uS/cm");
  Serial.print(" | TDS: "); Serial.print(tdsValue, 1); Serial.println(" ppm");

  Blynk.virtualWrite(V1, tdsValue);
}

void readAndSendPH() {
  phADC = analogRead(PH_ANALOG_PIN);
  voltage = phADC * (3.3 / 4095.0);
  pHValue = 7 + ((2.5 - voltage) * 3.5);
  pHValue = constrain(pHValue, 0, 14);

  String classification;
  if      (pHValue < 3.0)  classification = "Strong Acid";
  else if (pHValue < 5.5)  classification = "Weak Acid";
  else if (pHValue < 6.7)  classification = "Slightly Acidic";
  else if (pHValue < 7.5)  classification = "Neutral";
  else if (pHValue < 8.5)  classification = "Slightly Alkaline";
  else if (pHValue < 10.0) classification = "Mild Alkaline";
  else if (pHValue < 12.5) classification = "Alkaline";
  else                     classification = "Strong Alkaline";

  Serial.print("[pH] Analog: "); Serial.print(phADC);
  Serial.print(" | Voltage: "); Serial.print(voltage, 2); Serial.print(" V");
  Serial.print(" | pH: "); Serial.print(pHValue, 2);
  Serial.print(" | Classification: "); Serial.println(classification);

  Blynk.virtualWrite(V2, pHValue);
}

void kontrolPompa() {
  if (tdsValue < 60.0) {
    Serial.println("[POMPA] TDS rendah, kedua pompa dinyalakan.");
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, 255);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, 255);
    Blynk.virtualWrite(V3, "Pompa Hidup");
  } else {
    Serial.println("[POMPA] TDS normal/tinggi, kedua pompa dimatikan.");
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); analogWrite(ENB, 0);
    Blynk.virtualWrite(V3, "Pompa Mati");
  }
}

void sendSensorData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT22] Failed to read from DHT sensor!");
  } else {
    temperature = t;
    Serial.print("[DHT22] Temperature: "); Serial.print(t); Serial.print(" °C");
    Serial.print(" | Humidity: "); Serial.print(h); Serial.println(" %");
    Blynk.virtualWrite(V0, t);
    Blynk.virtualWrite(V6, h);
  }

  readAndSendTDS();
  kontrolPompa();
  Serial.println("====================================================");
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);

  pinMode(PH_DIGITAL_PIN, INPUT);

  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, HIGH); // default OFF

  Blynk.begin(auth, ssid, pass);

  timer.setInterval(3000L, readAndSendPH);
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}
