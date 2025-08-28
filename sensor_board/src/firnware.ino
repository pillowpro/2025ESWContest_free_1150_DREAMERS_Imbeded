#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_MPU6050.h"
#include "MAX30105.h"

Adafruit_SHT4x sht41 = Adafruit_SHT4x();
Adafruit_MPU6050 mpu;
MAX30105 max30105[5];

#define TCAADDR 0x70
#define BUFFER_SIZE 8
#define HR_HISTORY_SIZE 3

struct SensorData {
  uint16_t irBuffer[BUFFER_SIZE];
  uint16_t redBuffer[BUFFER_SIZE];
  byte bufferIndex;
  byte hrHistory[HR_HISTORY_SIZE];
  byte hrHistoryIndex;
  byte heartRate;
  byte spO2;
  byte filteredHR;
  bool isValid;
  unsigned long lastValidTime;
};

SensorData sensors[5];
bool sensorConnected[5] = {false, false, false, false, false};

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

byte calculateHeartRate(byte sensorIndex) {
  uint16_t max_val = 0, min_val = 65535;
  
  for (byte i = 0; i < BUFFER_SIZE; i++) {
    uint16_t val = sensors[sensorIndex].irBuffer[i];
    if (val > max_val) max_val = val;
    if (val < min_val) min_val = val;
  }
  
  if (max_val - min_val < 60) return 0;
  
  uint16_t threshold = min_val + (max_val - min_val) / 2;
  byte peaks = 0;
  bool above_threshold = false;
  
  for (byte i = 0; i < BUFFER_SIZE; i++) {
    if (sensors[sensorIndex].irBuffer[i] > threshold && !above_threshold) {
      peaks++;
      above_threshold = true;
    } else if (sensors[sensorIndex].irBuffer[i] <= threshold) {
      above_threshold = false;
    }
  }
  
  if (peaks == 0) return 0;
  
  byte bpm = peaks * 150;
  
  if (bpm > 180 || bpm < 40) return 0;
  
  return bpm;
}

byte calculateSpO2(byte sensorIndex) {
  uint32_t redSum = 0, irSum = 0;
  
  for (byte i = 0; i < BUFFER_SIZE; i++) {
    redSum += sensors[sensorIndex].redBuffer[i];
    irSum += sensors[sensorIndex].irBuffer[i];
  }
  
  if (irSum == 0) return 0;
  
  float ratio = (float)redSum / irSum;
  
  byte spO2;
  if (ratio < 0.7) spO2 = 100;
  else if (ratio > 1.3) spO2 = 85;
  else spO2 = 100 - (ratio - 0.7) * 25;
  
  if (spO2 < 70 || spO2 > 100) return 0;
  
  return spO2;
}

byte filterHeartRate(byte sensorIndex, byte newHR) {
  if (newHR == 0) return 0;
  
  // 아웃라이어 제거: 이전 값과 차이가 15 이상이면 무시
  if (sensors[sensorIndex].filteredHR > 0) {
    if (abs(newHR - sensors[sensorIndex].filteredHR) > 15) {
      return sensors[sensorIndex].filteredHR;
    }
  }
  
  // 극단값 제거
  if (newHR > 120 || newHR < 50) {
    if (sensors[sensorIndex].filteredHR > 0) {
      return sensors[sensorIndex].filteredHR;
    }
    return 0;
  }
  
  // 히스토리에 추가
  sensors[sensorIndex].hrHistory[sensors[sensorIndex].hrHistoryIndex] = newHR;
  sensors[sensorIndex].hrHistoryIndex = (sensors[sensorIndex].hrHistoryIndex + 1) % HR_HISTORY_SIZE;
  
  // 중간값 필터 (더 안정적)
  byte sortedHR[HR_HISTORY_SIZE];
  byte validCount = 0;
  
  for (byte i = 0; i < HR_HISTORY_SIZE; i++) {
    if (sensors[sensorIndex].hrHistory[i] > 0) {
      sortedHR[validCount] = sensors[sensorIndex].hrHistory[i];
      validCount++;
    }
  }
  
  if (validCount == 0) return 0;
  if (validCount == 1) return sortedHR[0];
  
  // 간단한 정렬
  for (byte i = 0; i < validCount - 1; i++) {
    for (byte j = i + 1; j < validCount; j++) {
      if (sortedHR[i] > sortedHR[j]) {
        byte temp = sortedHR[i];
        sortedHR[i] = sortedHR[j];
        sortedHR[j] = temp;
      }
    }
  }
  
  return sortedHR[validCount / 2];
}

void updateSensor(byte sensorIndex) {
  tcaSelect(sensorIndex + 2);
  delay(1);
  
  uint32_t irValue = max30105[sensorIndex].getIR();
  uint32_t redValue = max30105[sensorIndex].getRed();
  
  if (irValue < 30000) {
    sensors[sensorIndex].isValid = false;
    return;
  }
  
  uint16_t ir = irValue / 16;
  uint16_t red = redValue / 16;
  
  sensors[sensorIndex].irBuffer[sensors[sensorIndex].bufferIndex] = ir;
  sensors[sensorIndex].redBuffer[sensors[sensorIndex].bufferIndex] = red;
  sensors[sensorIndex].bufferIndex = (sensors[sensorIndex].bufferIndex + 1) % BUFFER_SIZE;
  
  static byte calcCounter[5] = {0, 0, 0, 0, 0};
  calcCounter[sensorIndex]++;
  
  if (calcCounter[sensorIndex] >= BUFFER_SIZE * 2) {
    calcCounter[sensorIndex] = 0;
    
    byte rawHR = calculateHeartRate(sensorIndex);
    byte rawSpO2 = calculateSpO2(sensorIndex);
    
    if (rawHR > 0) {
      sensors[sensorIndex].heartRate = rawHR;
      sensors[sensorIndex].filteredHR = filterHeartRate(sensorIndex, rawHR);
      sensors[sensorIndex].spO2 = rawSpO2;
      sensors[sensorIndex].lastValidTime = millis();
      
      sensors[sensorIndex].isValid = 
        (sensors[sensorIndex].filteredHR > 0 && sensors[sensorIndex].spO2 > 0);
    } else {
      sensors[sensorIndex].isValid = false;
    }
  }
  
  if (millis() - sensors[sensorIndex].lastValidTime > 10000) {
    sensors[sensorIndex].isValid = false;
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  for (byte i = 0; i < 5; i++) {
    sensors[i].bufferIndex = 0;
    sensors[i].hrHistoryIndex = 0;
    sensors[i].heartRate = 0;
    sensors[i].spO2 = 0;
    sensors[i].filteredHR = 0;
    sensors[i].isValid = false;
    sensors[i].lastValidTime = millis();
    
    for (byte j = 0; j < HR_HISTORY_SIZE; j++) {
      sensors[i].hrHistory[j] = 0;
    }
  }
  
  tcaSelect(0);
  if (!sht41.begin()) {
    Serial.println("RESP:ARD2:ERROR:SHT41_NOT_FOUND");
  } else {
    Serial.println("RESP:ARD2:OK:SHT41_DETECTED");
  }
  
  tcaSelect(1);
  if (!mpu.begin()) {
    Serial.println("RESP:ARD2:ERROR:MPU6050_NOT_FOUND");
  } else {
    Serial.println("RESP:ARD2:OK:MPU6050_DETECTED");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  }
  
  for (byte i = 0; i < 5; i++) {
    tcaSelect(i + 2);
    delay(100);
    
    if (!max30105[i].begin(Wire, I2C_SPEED_FAST)) {
      Serial.print("RESP:ARD2:ERROR:MAX30105_");
      Serial.print(i);
      Serial.println("_NOT_FOUND");
      sensorConnected[i] = false;
    } else {
      Serial.print("RESP:ARD2:OK:MAX30105_");
      Serial.print(i);
      Serial.println("_DETECTED");
      sensorConnected[i] = true;
      
      max30105[i].setup();
      max30105[i].setPulseAmplitudeRed(0x1F);
      max30105[i].setPulseAmplitudeIR(0x1F);
      max30105[i].setPulseAmplitudeGreen(0);
    }
  }
  
  Serial.println("RESP:ARD2:OK:SETUP_COMPLETE");
}

void loop() {
  static unsigned long lastReport = 0;
  static byte currentSensor = 0;
  static unsigned long lastSensorSwitch = 0;
  unsigned long currentTime = millis();
  
  // 센서별로 순차 처리 (멈춤 방지)
  if (currentTime - lastSensorSwitch >= 50) {
    if (sensorConnected[currentSensor]) {
      updateSensor(currentSensor);
    }
    
    currentSensor = (currentSensor + 1) % 5;
    lastSensorSwitch = currentTime;
  }
  
  if (currentTime - lastReport >= 5000) {
    tcaSelect(0);
    sensors_event_t humidity, temp;
    sht41.getEvent(&humidity, &temp);
    Serial.print("RESP:ARD2:OK:TEMP=");
    Serial.print(temp.temperature);
    Serial.print(":HUMID=");
    Serial.println(humidity.relative_humidity);
    
    tcaSelect(1);
    sensors_event_t a, g, temp_mpu;
    mpu.getEvent(&a, &g, &temp_mpu);
    Serial.print("RESP:ARD2:OK:ACCEL_X=");
    Serial.print(a.acceleration.x);
    Serial.print(":ACCEL_Y=");
    Serial.print(a.acceleration.y);
    Serial.print(":ACCEL_Z=");
    Serial.println(a.acceleration.z);
    
    byte connectedCount = 0, validCount = 0, weakSignalCount = 0, noSignalCount = 0;
    int totalHR = 0, totalSpO2 = 0;
    
    for (byte i = 0; i < 5; i++) {
      if (sensorConnected[i]) {
        connectedCount++;
        
        tcaSelect(i + 2);
        delay(1);
        uint32_t currentIR = max30105[i].getIR();
        
        if (currentIR < 1000) {
          noSignalCount++;
        } else if (currentIR < 30000) {
          weakSignalCount++;
        } else if (sensors[i].isValid && sensors[i].filteredHR > 0 && sensors[i].spO2 > 0) {
          totalHR += sensors[i].filteredHR;
          totalSpO2 += sensors[i].spO2;
          validCount++;
        }
      }
    }
    
    if (validCount > 0) {
      byte avgHR = totalHR / validCount;
      byte avgSpO2 = totalSpO2 / validCount;
      
      Serial.print("RESP:ARD2:OK:HR=");
      Serial.print(avgHR);
      Serial.print(":SPO2=");
      Serial.print(avgSpO2);
      Serial.print(":VALID_SENSORS=");
      Serial.print(validCount);
      Serial.print(":CONNECTED=");
      Serial.print(connectedCount);
      Serial.print(":WEAK_SIGNAL=");
      Serial.print(weakSignalCount);
      Serial.print(":NO_SIGNAL=");
      Serial.println(noSignalCount);
    } else {
      Serial.print("RESP:ARD2:ERROR:NO_VALID_SENSORS:CONNECTED=");
      Serial.print(connectedCount);
      Serial.print(":WEAK_SIGNAL=");
      Serial.print(weakSignalCount);
      Serial.print(":NO_SIGNAL=");
      Serial.println(noSignalCount);
    }
    
    lastReport = currentTime;
  }
  
  delay(10);
}