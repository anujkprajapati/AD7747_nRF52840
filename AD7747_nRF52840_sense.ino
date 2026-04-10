#include <Wire.h>

// AD7747 I2C Address (7-bit)
#define AD7747_ADDR 0x48
#define RDY_PIN 6

// Register addresses
#define REG_STATUS        0x00
#define REG_CAP_DATA_H    0x01
#define REG_CAP_DATA_M    0x02
#define REG_CAP_DATA_L    0x03
#define REG_CAP_SETUP     0x07
#define REG_VT_SETUP      0x08
#define REG_EXC_SETUP     0x09
#define REG_CONFIG        0x0A
#define REG_CAP_DAC_A     0x0B
#define REG_CAP_DAC_B     0x0C

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Wire.begin();
  Wire.setClock(400000);        // 400 kHz for faster I2C on nRF52840
  delay(50);

  Serial.println("AD7747 + nRF52840 Initializing...");
  pinMode(RDY_PIN, INPUT);

  // === Basic recommended configuration for grounded sensor with shield ===
  writeRegister(REG_CAP_SETUP, 0xA0);     // Enable capacitive channel (CAPEN=1)
  writeRegister(REG_CAP_SETUP, 0xC0);   // CAPEN=1 + CAPDIFF=1 (important!)
  writeRegister(REG_VT_SETUP,  0x81);     // Disable VT channel for now
  writeRegister(REG_EXC_SETUP, 0x0F);     // Excitation: full VDD, both EXC pins enabled
  writeRegister(REG_CAP_DAC_A, 0xFF);     // Adjust later if needed to null your 17.7 pF base C
  writeRegister(REG_CAP_DAC_B, 0x00);

  // Configuration: Continuous conversion, slowest rate (best noise ~4-5 Hz), MD=000
  writeRegister(REG_CONFIG,    0x41);     // Change to 0xA1 for ~8 Hz, or experiment

  Serial.println("AD7747 configured for continuous conversion.");
  delay(300);
}


void loop() {
  // Fast poll using RDY pin (most efficient)
  if (digitalRead(RDY_PIN) == LOW) {
    uint8_t status = readRegister(REG_STATUS);

    if ((status & 0x01) == 0) {        // RDYCAP bit cleared = new data
      long code = read24Bit(REG_CAP_DATA_H);

      float deltaC_pF = (code * 8.192) / 16777216.0;

      Serial.print("Raw: ");
      Serial.print(code);
      Serial.print("   ΔC: ");
      Serial.println(deltaC_pF, 6);
      // Serial.println(" pF");
    }
  }
}

// ==================== Helper Functions ====================

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AD7747_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
  delay(3);
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(AD7747_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  
  Wire.requestFrom(AD7747_ADDR, (uint8_t)1);
  return Wire.read();
}

long read24Bit(uint8_t startReg) {
  Wire.beginTransmission(AD7747_ADDR);
  Wire.write(startReg);
  Wire.endTransmission(false);
  
  Wire.requestFrom(AD7747_ADDR, (uint8_t)3);
  long value = 0;
  value = ((long)Wire.read() << 16);
  value |= ((long)Wire.read() << 8);
  value |= Wire.read();
  
  // Convert from 24-bit unsigned to signed (two's complement)
  if (value & 0x800000) {
    value |= 0xFF000000;   // sign extend
  }
  return value;
}