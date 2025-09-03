/*
╔═╗╦╦  ╦  ╔═╗╦ ╦  ╔═╗╦═╗╔═╗
╠═╝║║  ║  ║ ║║║║  ╠═╝╠╦╝║ ║
╩  ╩╩═╝╩═╝╚═╝╚╩╝  ╩  ╩╚═╚═╝

 * 베개프로 압력제어시스템 v4.3 (SET_ALL 추가)
 */

#include <SoftwareSerial.h>
SoftwareSerial uartBridge(A2, -1);

#define LEFT_PUMP_RATIO 1.5
#define RIGHT_PUMP_RATIO 1.5
#define LEFT_AIR_TIME 30000
#define RIGHT_AIR_TIME 30000
#define MAX_PRESSURE_PSI_L_CONTINUOUS 0.95
#define MAX_PRESSURE_PSI_R_CONTINUOUS 0.7
#define MAINTAIN_INTERVAL 600000
#define MAINTAIN_BOOST 5.0
#define MIN_MAINTAIN_LEVEL 30.0
#define MAX_MOTOR_ON_TIME 50
#define DEBUG 1
#define UART_BUFFER_SIZE 256
#define UART_TIMEOUT 50
#define SET_ALL_DELAY 10000

const int ALARM_MOTOR = 2, LEFT_PUMP = 3, ALARM_MOTOR_2 = 5;
const int RIGHT_PUMP = 7, LEFT_AIR_PUMP = 10, RIGHT_AIR_PUMP = 11;
const int PRESSURE_LEFT = A0, PRESSURE_RIGHT = A1;

const byte rhythmPattern[] = {40, 10, 25, 40, 15, 40, 10, 25, 25, 40, 10, 40, 15, 25, 40, 10};

unsigned long alarmStartTime, lastPrintTime, leftMaintainTime, rightMaintainTime;
unsigned long leftPumpStartTime, rightPumpStartTime, leftAirStartTime, rightAirStartTime;
unsigned long leftLastOverTime, rightLastOverTime;
unsigned long setAllStartTime;
byte alarmIndex = 0, leftOverCount = 0, rightOverCount = 0;
float leftTargetPressure = 0, rightTargetPressure = 0;
float leftCurrentLevel = 0, rightCurrentLevel = 0;

bool alarmActive = false, leftPumpActive = false, rightPumpActive = false;
bool leftAirActive = false, rightAirActive = false, leftMaintainMode = false, rightMaintainMode = false;
bool setAllActive = false;

String uartBuffer = "";
unsigned long lastUartReceive = 0;

void setup() {
  Serial.begin(9600);
  uartBridge.begin(9600);
  
  pinMode(13, INPUT);
  pinMode(ALARM_MOTOR, OUTPUT); pinMode(LEFT_PUMP, OUTPUT); pinMode(ALARM_MOTOR_2, OUTPUT);
  pinMode(RIGHT_PUMP, OUTPUT); pinMode(LEFT_AIR_PUMP, OUTPUT); pinMode(RIGHT_AIR_PUMP, OUTPUT);
  
  allRelaysOff();
  unsigned long t = millis();
  lastPrintTime = leftMaintainTime = rightMaintainTime = t;
  leftLastOverTime = rightLastOverTime = t;
  lastUartReceive = t;
  setAllStartTime = t;
  uartBuffer.reserve(UART_BUFFER_SIZE);
}

void loop() {
  updateAlarmRhythm();
  checkSerialCommands();
  updatePumpControl();
  checkMaintenance();
  checkSetAllTimer();
  handleUartData();
  
  if (millis() - lastPrintTime >= 1000) {
    lastPrintTime = millis();
  }
  delay(50);
}

void checkSetAllTimer() {
  if (setAllActive && millis() - setAllStartTime >= SET_ALL_DELAY) {
    leftTargetPressure = 100.0;
    rightTargetPressure = 100.0;
    setLeftPressure();
    setRightPressure();
    setAllActive = false;
    sendResponse("SET_ALL", "EXECUTED");
    if (DEBUG) {
      Serial.println("SET_ALL executed: Both sides -> 100%");
    }
  }
}

void handleUartData() {
  while (uartBridge.available()) {
    char c = uartBridge.read();
    uartBuffer += c;
    lastUartReceive = millis();
    
    if (uartBuffer.length() >= UART_BUFFER_SIZE - 1) {
      Serial.print(uartBuffer);
      uartBuffer = "";
    }
  }
  
  if (uartBuffer.length() > 0 && millis() - lastUartReceive > UART_TIMEOUT) {
    Serial.print(uartBuffer);
    uartBuffer = "";
  }
}

void allRelaysOff() {
  digitalWrite(ALARM_MOTOR, HIGH); digitalWrite(LEFT_PUMP, HIGH); digitalWrite(ALARM_MOTOR_2, HIGH);
  digitalWrite(RIGHT_PUMP, HIGH); digitalWrite(LEFT_AIR_PUMP, HIGH); digitalWrite(RIGHT_AIR_PUMP, HIGH);
}

void controlRelay(int pin, bool state) { digitalWrite(pin, state ? LOW : HIGH); }

void updateAlarmRhythm() {
  if (!alarmActive) return;
  
  unsigned long elapsed = millis() - alarmStartTime;
  unsigned long patternTime = 0;
  
  for (int i = 0; i < 16; i += 2) {
    patternTime += rhythmPattern[i] + rhythmPattern[i + 1];
  }
  
  unsigned long cycleTime = elapsed % patternTime;
  unsigned long currentPos = 0;
  bool motorsOn = false;
  
  for (int i = 0; i < 16; i += 2) {
    byte onTime = rhythmPattern[i];
    byte offTime = rhythmPattern[i + 1];
    
    if (onTime > MAX_MOTOR_ON_TIME) onTime = MAX_MOTOR_ON_TIME;
    if (offTime < 100) offTime = 100;
    
    if (cycleTime >= currentPos && cycleTime < currentPos + onTime) {
      motorsOn = true;
      break;
    } else if (cycleTime >= currentPos + onTime && cycleTime < currentPos + onTime + offTime) {
      motorsOn = false;
      break;
    }
    currentPos += onTime + offTime;
  }
  
  controlRelay(ALARM_MOTOR, motorsOn);
  controlRelay(ALARM_MOTOR_2, motorsOn);
}

void checkSerialCommands() {
  if (uartBridge.available()) {
    return;
  }
  
  if (Serial.available()) {
    String cmd = Serial.readString();
    cmd.trim();
    
    if (cmd.startsWith("LEFT_SET:")) {
      leftTargetPressure = cmd.substring(9).toFloat();
      setLeftPressure();
    }
    else if (cmd.startsWith("RIGHT_SET:")) {
      rightTargetPressure = cmd.substring(10).toFloat();
      setRightPressure();
    }
    else if (cmd == "SET_ALL") {
      setAllActive = true;
      setAllStartTime = millis();
      sendResponse("SET_ALL", "SCHEDULED");
      if (DEBUG) {
        Serial.println("SET_ALL scheduled: 10 seconds to 100%");
      }
    }
    else if (cmd == "STOP_LEFT") {
      leftPumpActive = leftAirActive = leftMaintainMode = false;
      controlRelay(LEFT_PUMP, false); controlRelay(LEFT_AIR_PUMP, false);
      sendResponse("STOP_LEFT", "OK");
    }
    else if (cmd == "STOP_RIGHT") {
      rightPumpActive = rightAirActive = rightMaintainMode = false;
      controlRelay(RIGHT_PUMP, false); controlRelay(RIGHT_AIR_PUMP, false);
      sendResponse("STOP_RIGHT", "OK");
    }
    else if (cmd == "CANCEL_SET_ALL") {
      setAllActive = false;
      sendResponse("CANCEL_SET_ALL", "OK");
      if (DEBUG) {
        Serial.println("SET_ALL cancelled");
      }
    }
    else if (cmd.startsWith("SET_ALARM:")) {
      if (cmd.substring(10).toInt() == 1) {
        alarmStartTime = millis(); alarmIndex = 0; alarmActive = true;
        sendResponse("SET_ALARM", "ON");
      } else {
        alarmActive = false; controlRelay(ALARM_MOTOR, false); controlRelay(ALARM_MOTOR_2, false);
        sendResponse("SET_ALARM", "OFF");
      }
    }
    else if (cmd == "UART_CLEAR") {
      uartBuffer = "";
      Serial.println("UART buffer cleared manually");
    }
    else if (cmd == "UART_STATUS") {
      Serial.print("UART buffer length: ");
      Serial.println(uartBuffer.length());
      Serial.print("Buffer content: ");
      Serial.println(uartBuffer);
    }
  }
}

void updatePumpControl() {
  unsigned long t = millis();
  
  if (leftPumpActive) {
    unsigned long elapsed = t - leftPumpStartTime;
    float targetTime = (leftTargetPressure - leftCurrentLevel) * 100 / LEFT_PUMP_RATIO;
    float sensorPressure = getPressurePercent(readPressure(PRESSURE_LEFT), true);
    
    if (sensorPressure >= leftTargetPressure) {
      if (t - leftLastOverTime <= 1000) leftOverCount++; else leftOverCount = 1;
      leftLastOverTime = t;
      if (leftOverCount >= 2) {
        leftPumpActive = false; controlRelay(LEFT_PUMP, false);
        leftCurrentLevel = leftTargetPressure; leftMaintainMode = true; leftMaintainTime = t;
        leftOverCount = 0;
        if (DEBUG) { Serial.print("L sensor stop: "); Serial.println(sensorPressure); }
        return;
      }
    }
    
    if (elapsed >= targetTime) {
      leftPumpActive = false; controlRelay(LEFT_PUMP, false);
      leftCurrentLevel = leftTargetPressure; leftMaintainMode = true; leftMaintainTime = t;
      if (DEBUG) { Serial.print("L time done: "); Serial.print(leftCurrentLevel); Serial.print("% sensor:"); Serial.println(sensorPressure); }
    }
  }
  
  if (rightPumpActive) {
    unsigned long elapsed = t - rightPumpStartTime;
    float targetTime = (rightTargetPressure - rightCurrentLevel) * 100 / RIGHT_PUMP_RATIO;
    float sensorPressure = getPressurePercent(readPressure(PRESSURE_RIGHT), false);
    
    if (sensorPressure >= rightTargetPressure) {
      if (t - rightLastOverTime <= 1000) rightOverCount++; else rightOverCount = 1;
      rightLastOverTime = t;
      if (rightOverCount >= 2) {
        rightPumpActive = false; controlRelay(RIGHT_PUMP, false);
        rightCurrentLevel = rightTargetPressure; rightMaintainMode = true; rightMaintainTime = t;
        rightOverCount = 0;
        if (DEBUG) { Serial.print("R sensor stop: "); Serial.println(sensorPressure); }
        return;
      }
    }
    
    if (elapsed >= targetTime) {
      rightPumpActive = false; controlRelay(RIGHT_PUMP, false);
      rightCurrentLevel = rightTargetPressure; rightMaintainMode = true; rightMaintainTime = t;
      if (DEBUG) { Serial.print("R time done: "); Serial.print(rightCurrentLevel); Serial.print("% sensor:"); Serial.println(sensorPressure); }
    }
  }
  
  if (leftAirActive) {
    unsigned long elapsed = t - leftAirStartTime;
    float pressureDiff = leftCurrentLevel - leftTargetPressure;
    unsigned long targetTime = pressureDiff * LEFT_AIR_TIME;
    
    if (elapsed >= targetTime) {
      leftAirActive = false; controlRelay(LEFT_AIR_PUMP, false);
      leftCurrentLevel = leftTargetPressure; leftMaintainMode = true; leftMaintainTime = t;
      if (DEBUG) { Serial.print("L air done: "); Serial.println(leftCurrentLevel); }
    }
  }
  
  if (rightAirActive) {
    unsigned long elapsed = t - rightAirStartTime;
    float pressureDiff = rightCurrentLevel - rightTargetPressure;
    unsigned long targetTime = pressureDiff * RIGHT_AIR_TIME;
    
    if (elapsed >= targetTime) {
      rightAirActive = false; controlRelay(RIGHT_AIR_PUMP, false);
      rightCurrentLevel = rightTargetPressure; rightMaintainMode = true; rightMaintainTime = t;
      if (DEBUG) { Serial.print("R air done: "); Serial.println(rightCurrentLevel); }
    }
  }
}

void checkMaintenance() {
  unsigned long t = millis();
  
  if (leftMaintainMode && t - leftMaintainTime >= MAINTAIN_INTERVAL && leftCurrentLevel >= MIN_MAINTAIN_LEVEL) {
    leftTargetPressure = leftCurrentLevel + MAINTAIN_BOOST;
    if (leftTargetPressure > 100) leftTargetPressure = 100;
    leftPumpActive = true; leftPumpStartTime = t; controlRelay(LEFT_PUMP, true);
    leftMaintainTime = t;
    if (DEBUG) { Serial.print("L maintain: "); Serial.print(leftCurrentLevel); Serial.print("->"); Serial.println(leftTargetPressure); }
  }
  
  if (rightMaintainMode && t - rightMaintainTime >= MAINTAIN_INTERVAL && rightCurrentLevel >= MIN_MAINTAIN_LEVEL) {
    rightTargetPressure = rightCurrentLevel + MAINTAIN_BOOST;
    if (rightTargetPressure > 100) rightTargetPressure = 100;
    rightPumpActive = true; rightPumpStartTime = t; controlRelay(RIGHT_PUMP, true);
    rightMaintainTime = t;
    if (DEBUG) { Serial.print("R maintain: "); Serial.print(rightCurrentLevel); Serial.print("->"); Serial.println(rightTargetPressure); }
  }
}

void setLeftPressure() {
  leftPumpActive = leftAirActive = false;
  controlRelay(LEFT_PUMP, false); controlRelay(LEFT_AIR_PUMP, false);
  leftOverCount = 0;
  
  if (leftCurrentLevel < leftTargetPressure) {
    leftPumpActive = true; leftPumpStartTime = millis(); controlRelay(LEFT_PUMP, true);
    sendResponse("LEFT_SET", "PUMP");
    if (DEBUG) { Serial.print("L pump: "); Serial.print(leftCurrentLevel); Serial.print("->"); Serial.println(leftTargetPressure); }
  } else if (leftCurrentLevel > leftTargetPressure) {
    leftAirActive = true; leftAirStartTime = millis(); controlRelay(LEFT_AIR_PUMP, true);
    sendResponse("LEFT_SET", "AIR");
    if (DEBUG) { Serial.print("L air: "); Serial.print(leftCurrentLevel); Serial.print("->"); Serial.println(leftTargetPressure); }
  } else {
    leftMaintainMode = true; leftMaintainTime = millis();
    sendResponse("LEFT_SET", "SAME");
  }
}

void setRightPressure() {
  rightPumpActive = rightAirActive = false;
  controlRelay(RIGHT_PUMP, false); controlRelay(RIGHT_AIR_PUMP, false);
  rightOverCount = 0;
  
  if (rightCurrentLevel < rightTargetPressure) {
    rightPumpActive = true; rightPumpStartTime = millis(); controlRelay(RIGHT_PUMP, true);
    sendResponse("RIGHT_SET", "PUMP");
    if (DEBUG) { Serial.print("R pump: "); Serial.print(rightCurrentLevel); Serial.print("->"); Serial.println(rightTargetPressure); }
  } else if (rightCurrentLevel > rightTargetPressure) {
    rightAirActive = true; rightAirStartTime = millis(); controlRelay(RIGHT_AIR_PUMP, true);
    sendResponse("RIGHT_SET", "AIR");
    if (DEBUG) { Serial.print("R air: "); Serial.print(rightCurrentLevel); Serial.print("->"); Serial.println(rightTargetPressure); }
  } else {
    rightMaintainMode = true; rightMaintainTime = millis();
    sendResponse("RIGHT_SET", "SAME");
  }
}

float readPressure(int pin) {
  int raw = analogRead(pin);
  float voltage = raw * (5.0 / 1023.0);
  float psi = (voltage - 0.5) * (200.0 / 4.0);
  return psi < 0 ? 0 : psi;
}

float getPressurePercent(float psi, bool isLeft) {
  float maxPsi = isLeft ? MAX_PRESSURE_PSI_L_CONTINUOUS : MAX_PRESSURE_PSI_R_CONTINUOUS;
  if (maxPsi <= 0) return 0;
  float percent = (psi / maxPsi) * 100.0;
  return percent > 100 ? 100 : (percent < 0 ? 0 : percent);
}

void sendResponse(String cmd, String msg) {
  float lp = readPressure(PRESSURE_LEFT), rp = readPressure(PRESSURE_RIGHT);
  Serial.print("{\"cmd\":\""); Serial.print(cmd);
  Serial.print("\",\"msg\":\""); Serial.print(msg);
  Serial.print("\",\"L\":"); Serial.print(lp);
  Serial.print(",\"R\":"); Serial.print(rp);
  Serial.print(",\"t\":"); Serial.print(millis());
  Serial.println("}");
}

void printStatus() {
  if (DEBUG) {
    float lp = readPressure(PRESSURE_LEFT), rp = readPressure(PRESSURE_RIGHT);
    Serial.print("L:"); Serial.print(leftCurrentLevel); Serial.print("%("); Serial.print(getPressurePercent(lp, true)); Serial.print(")");
    Serial.print(" R:"); Serial.print(rightCurrentLevel); Serial.print("%("); Serial.print(getPressurePercent(rp, false)); Serial.println(")");
  }
}