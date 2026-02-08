#include <ModbusMaster.h>

#define RE_PIN 4
#define RXD2 17
#define TXD2 16

ModbusMaster node;

void preTransmission() {
  digitalWrite(RE_PIN, HIGH);
}
void postTransmission() {
  digitalWrite(RE_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);

  pinMode(RE_PIN, OUTPUT);
  digitalWrite(RE_PIN, LOW);

  node.begin(1, Serial2); // slave ID 0x01
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  Serial.println("Soil Sensor Monitoring Started...");
}

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

void loop() {
  float humidity    = readSensor(0x0000, 0.1);
  float temperature = readSensor(0x0001, 0.1);
  float conductivity= readSensor(0x0002, 1.0);
  float pH          = readSensor(0x0003, 0.1);
  float nitrogen    = readSensor(0x0004, 1.0);
  float phosphorus  = readSensor(0x0005, 1.0);
  float potassium   = readSensor(0x0006, 1.0);
  float salinity    = readSensor(0x0007, 1.0);
  float tds         = readSensor(0x0008, 1.0);

  Serial.println("=== Soil Sensor Readings ===");
  Serial.printf("Humidity     : %.1f %%\n", humidity);
  Serial.printf("Temperature  : %.1f °C\n", temperature);
  Serial.printf("Conductivity : %.0f uS/cm\n", conductivity);
  Serial.printf("pH           : %.1f\n", pH);
  Serial.printf("Nitrogen     : %.0f mg/kg\n", nitrogen);
  Serial.printf("Phosphorus   : %.0f mg/kg\n", phosphorus);
  Serial.printf("Potassium    : %.0f mg/kg\n", potassium);
  Serial.printf("Salinity     : %.0f g/L\n", salinity);
  Serial.printf("TDS          : %.0f mg/L\n", tds);
  Serial.println("============================\n");

  delay(5000); // delay antar baca (5 detik)
}
