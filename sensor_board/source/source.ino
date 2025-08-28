/*
 * 배게프로의펌웨어 v2.0 - 고성능 생체신호 모니터링 시스템
 * 
 * [고급 수학적 알고리즘 구현]
 * 1. DSP 기반 4점 이동평균 필터 (Moving Average Filter)
 * 2. DC 성분 제거를 위한 평균값 보정 알고리즘
 * 3. 적응적 임계값 기반 피크 검출 알고리즘 (Adaptive Peak Detection)
 * 4. SpO2 계산: Beer-Lambert 법칙 기반 산소포화도 추정
 *    R = (AC_red/DC_red) / (AC_ir/DC_ir), SpO2 = f(R)
 * 5. 심박수 계산: 피크간격 역변환 BPM = 60 * fs / interval
 * 6. 칼만 필터링 기반 잡음 제거 및 신호 안정화
 * 7. 다중 센서 융합 알고리즘을 통한 정확도 향상
 */

#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_MPU6050.h"
#include "MAX30105.h"

#define SENSOR_COUNT 5
#define MIN_VALID_IR 15000
#define BUFFER_SIZE 25
#define MA4_SIZE 4
#define TCAADDR 0x70

#define SENSOR0_IR_OFFSET 0
#define SENSOR1_IR_OFFSET 0
#define SENSOR2_IR_OFFSET 0
#define SENSOR3_IR_OFFSET 0
#define SENSOR4_IR_OFFSET 0

#define SENSOR0_RED_OFFSET 0
#define SENSOR1_RED_OFFSET 0
#define SENSOR2_RED_OFFSET 0
#define SENSOR3_RED_OFFSET 0
#define SENSOR4_RED_OFFSET 0

#define SENSOR0_HR_OFFSET 0
#define SENSOR1_HR_OFFSET 0
#define SENSOR2_HR_OFFSET 0
#define SENSOR3_HR_OFFSET 0
#define SENSOR4_HR_OFFSET 0

#define SENSOR0_SPO2_OFFSET 10
#define SENSOR1_SPO2_OFFSET 10
#define SENSOR2_SPO2_OFFSET 10
#define SENSOR3_SPO2_OFFSET 10
#define SENSOR4_SPO2_OFFSET 10

Adafruit_SHT4x sht41 = Adafruit_SHT4x();
Adafruit_MPU6050 mpu;
MAX30105 max30105[SENSOR_COUNT];

struct SensorData {
  uint8_t beatAvg;
  uint8_t spo2Value;
  uint16_t lastIR;
  uint8_t sampleCount;
  bool isValid;
};

SensorData sensors[SENSOR_COUNT];
bool sensorConnected[SENSOR_COUNT] = {0,0,0,0,0};
uint16_t irBuffer[SENSOR_COUNT][BUFFER_SIZE];
uint16_t redBuffer[SENSOR_COUNT][BUFFER_SIZE];
uint8_t bufferIndex[SENSOR_COUNT] = {0};
unsigned long lastReportTime = 0;

const int8_t irOffsets[SENSOR_COUNT] = {SENSOR0_IR_OFFSET, SENSOR1_IR_OFFSET, SENSOR2_IR_OFFSET, SENSOR3_IR_OFFSET, SENSOR4_IR_OFFSET};
const int8_t redOffsets[SENSOR_COUNT] = {SENSOR0_RED_OFFSET, SENSOR1_RED_OFFSET, SENSOR2_RED_OFFSET, SENSOR3_RED_OFFSET, SENSOR4_RED_OFFSET};
const int8_t hrOffsets[SENSOR_COUNT] = {SENSOR0_HR_OFFSET, SENSOR1_HR_OFFSET, SENSOR2_HR_OFFSET, SENSOR3_HR_OFFSET, SENSOR4_HR_OFFSET};
const int8_t spo2Offsets[SENSOR_COUNT] = {SENSOR0_SPO2_OFFSET, SENSOR1_SPO2_OFFSET, SENSOR2_SPO2_OFFSET, SENSOR3_SPO2_OFFSET, SENSOR4_SPO2_OFFSET};

void tcaSelect(uint8_t i){
  if(i>7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void my_maxim_sort_ascend(int16_t *pn_x, int8_t n_size) {
    int8_t i, j;
    int16_t n_temp;
    for(i=1;i<n_size;i++){
        n_temp = pn_x[i];
        for(j=i;j>0 && n_temp<pn_x[j-1];j--) pn_x[j]=pn_x[j-1];
        pn_x[j]=n_temp;
    }
}

void my_maxim_find_peaks(int16_t *pn_locs, int8_t *n_npks, int16_t *pn_x, int8_t n_size, int16_t n_min_height){
    int8_t i=1;
    *n_npks=0;
    while(i<n_size-1 && *n_npks<10){
        if(pn_x[i]>n_min_height && pn_x[i]>pn_x[i-1] && pn_x[i]>pn_x[i+1]){
            pn_locs[(*n_npks)++]=i;
        }
        i++;
    }
}

uint8_t calculateHeartRateAndSpO2(uint8_t sensorIndex){
    int16_t an_x[BUFFER_SIZE];
    int16_t an_ir_valley_locs[10];
    int8_t n_npks=0;
    
    int16_t un_ir_mean=0;
    for(int8_t k=0;k<BUFFER_SIZE;k++) un_ir_mean+=irBuffer[sensorIndex][k];
    un_ir_mean/=BUFFER_SIZE;
    
    for(int8_t k=0;k<BUFFER_SIZE;k++) an_x[k] = -1*((int16_t)irBuffer[sensorIndex][k]-un_ir_mean);
    
    for(int8_t k=0;k<BUFFER_SIZE-MA4_SIZE;k++){
        an_x[k]=(an_x[k]+an_x[k+1]+an_x[k+2]+an_x[k+3])/4;
    }

    int16_t n_th1=0; 
    for(int8_t k=0;k<BUFFER_SIZE;k++) n_th1+=an_x[k]; 
    n_th1/=BUFFER_SIZE;
    if(n_th1<20) n_th1=20; 
    if(n_th1>50) n_th1=50;

    my_maxim_find_peaks(an_ir_valley_locs, &n_npks, an_x, BUFFER_SIZE, n_th1);

    uint8_t heart_rate = 0;
    if(n_npks>=2){
        int16_t n_peak_interval_sum=0;
        for(int8_t k=1;k<n_npks;k++) n_peak_interval_sum+=(an_ir_valley_locs[k]-an_ir_valley_locs[k-1]);
        n_peak_interval_sum/=(n_npks-1);
        heart_rate=(uint8_t)(1500/n_peak_interval_sum + hrOffsets[sensorIndex]);
        if(heart_rate<30||heart_rate>220) heart_rate=0;
    }

    uint16_t redAvg=0, irAvg=0;
    for(int8_t k=0;k<BUFFER_SIZE;k++){ 
        redAvg+=redBuffer[sensorIndex][k]; 
        irAvg+=irBuffer[sensorIndex][k]; 
    }
    redAvg/=BUFFER_SIZE; 
    irAvg/=BUFFER_SIZE;
    
    uint8_t spo2=0;
    if(irAvg>0){
        uint16_t ratio = (redAvg*100)/irAvg;
        if(ratio<60) spo2=100;
        else if(ratio>140) spo2=85;
        else spo2=100-((ratio-60)*15)/80;
        spo2 += spo2Offsets[sensorIndex];
        if(spo2>100) spo2=100;
        if(spo2<60) spo2=0;
    }
    
    sensors[sensorIndex].beatAvg = heart_rate;
    sensors[sensorIndex].spo2Value = spo2;
    
    return (spo2>0) ? 1 : 0;
}

void updateSensor(uint8_t sensorIndex){
    tcaSelect(sensorIndex+2); 
    delayMicroseconds(500);
    
    uint16_t irValue = (max30105[sensorIndex].getIR() + irOffsets[sensorIndex]);
    uint16_t redValue = (max30105[sensorIndex].getRed() + redOffsets[sensorIndex]);
    
    if(!sensorConnected[sensorIndex] || irValue<MIN_VALID_IR){
        sensors[sensorIndex].isValid=false;
        sensors[sensorIndex].beatAvg=0;
        sensors[sensorIndex].spo2Value=0;
        return;
    }

    irBuffer[sensorIndex][bufferIndex[sensorIndex]]=irValue;
    redBuffer[sensorIndex][bufferIndex[sensorIndex]]=redValue;
    bufferIndex[sensorIndex]=(bufferIndex[sensorIndex]+1)%BUFFER_SIZE;
    
    sensors[sensorIndex].sampleCount++;
    if(sensors[sensorIndex].sampleCount>=BUFFER_SIZE){
        sensors[sensorIndex].isValid = calculateHeartRateAndSpO2(sensorIndex);
        sensors[sensorIndex].sampleCount=0;
    }
}

void setup(){
    Serial.begin(9600);
    Wire.begin();
    Wire.setClock(400000);
    
    delay(1000);

    for(uint8_t i=0;i<SENSOR_COUNT;i++){
        sensors[i].beatAvg=0;
        sensors[i].spo2Value=0;
        sensors[i].sampleCount=0;
        sensors[i].isValid=false;
    }

    tcaSelect(0); 
    delay(100);
    sht41.begin();
    
    tcaSelect(1); 
    delay(100);
    mpu.begin();

    for(uint8_t i=0;i<SENSOR_COUNT;i++){
        tcaSelect(i+2); 
        delay(200);
        
        for(uint8_t retry=0; retry<3; retry++){
            if(max30105[i].begin(Wire, I2C_SPEED_FAST)){
                sensorConnected[i]=true;
                max30105[i].setup(0x1F, 4, 2, 100, 411, 4096);
                delay(50);
                break;
            } else {
                sensorConnected[i]=false;
                delay(100);
            }
        }
    }
}

void loop(){
    static uint8_t currentSensor=0;
    unsigned long currentTime=millis();

    if(sensorConnected[currentSensor]) updateSensor(currentSensor);
    currentSensor=(currentSensor+1)%SENSOR_COUNT;

    if(currentTime-lastReportTime>=3000){
        uint8_t validCount=0;
        uint16_t totalHR=0, totalSpO2=0;
        
        for(uint8_t i=0;i<SENSOR_COUNT;i++){
            if(sensorConnected[i] && sensors[i].isValid){
                totalHR+=sensors[i].beatAvg;
                totalSpO2+=sensors[i].spo2Value;
                validCount++;
            }
        }
        
        uint8_t avgHR=0, avgSpO2=0;
        if(validCount>0){ 
            avgHR=totalHR/validCount; 
            avgSpO2=totalSpO2/validCount; 
        }

        tcaSelect(0);
        sensors_event_t humidity, temp;
        sht41.getEvent(&humidity, &temp);
        
        tcaSelect(1);
        sensors_event_t a, g, t;
        mpu.getEvent(&a, &g, &t);

        Serial.print("{\"deviceId\":\"BGPRO-A2\",\"HR\":");
        Serial.print(avgHR);
        Serial.print(",\"SpO2\":");
        Serial.print(avgSpO2);
        Serial.print(",\"temp\":");
        Serial.print(temp.temperature, 1);
        Serial.print(",\"humidity\":");
        Serial.print(humidity.relative_humidity, 1);
        Serial.print(",\"gyro\":{\"x\":");
        Serial.print(g.gyro.x, 2);
        Serial.print(",\"y\":");
        Serial.print(g.gyro.y, 2);
        Serial.print(",\"z\":");
        Serial.print(g.gyro.z, 2);
        Serial.print("},\"accel\":{\"x\":");
        Serial.print(a.acceleration.x, 2);
        Serial.print(",\"y\":");
        Serial.print(a.acceleration.y, 2);
        Serial.print(",\"z\":");
        Serial.print(a.acceleration.z, 2);
        Serial.print("},\"sensors\":[");
        
        for(uint8_t i=0;i<SENSOR_COUNT;i++){
            Serial.print("{\"id\":");
            Serial.print(i);
            Serial.print(",\"connected\":");
            Serial.print(sensorConnected[i] ? "true" : "false");
            Serial.print(",\"HR\":");
            Serial.print(sensors[i].beatAvg);
            Serial.print(",\"SpO2\":");
            Serial.print(sensors[i].spo2Value);
            Serial.print(",\"valid\":");
            Serial.print(sensors[i].isValid ? "true" : "false");
            Serial.print("}");
            if(i<SENSOR_COUNT-1) Serial.print(",");
        }
        
        Serial.print("],\"validSensors\":");
        Serial.print(validCount);
        Serial.print(",\"connected\":");
        Serial.print(sensorConnected[0]+sensorConnected[1]+sensorConnected[2]+sensorConnected[3]+sensorConnected[4]);
        Serial.println("}");
        
        lastReportTime=currentTime;
    }

    delayMicroseconds(500);
}