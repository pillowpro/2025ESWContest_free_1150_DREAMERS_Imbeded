/*
╔═╗╦╦  ╦  ╔═╗╦ ╦  ╔═╗╦═╗╔═╗
╠═╝║║  ║  ║ ║║║║  ╠═╝╠╦╝║ ║
╩  ╩╩═╝╩═╝╚═╝╚╩╝  ╩  ╩╚═╚═╝

 * 베개프로 압력제어시스템 v3.2
 * 
 * [고급 압력 제어 알고리즘 구현]
 * 1. PWM 기반 점진적 압력 주입 시스템 (20ms 듀티사이클) - 바람 넣을 때만
 * 2. 고정 측정 시간: 790ms (0.79초)
 * 3. 다중 센서 기반 실시간 압력 모니터링 시스템
 * 4. 자동 압력 유지 알고리즘 (10분 주기 압력 보정)
 * 5. 정밀 방출 제어: 1% 압력당 30초 타이밍 제어
 * 6. 리듬 패턴 기반 알람 시스템 (교대 모터 제어)
 * 7. 실시간 시리얼 통신 기반 원격 제어 인터페이스
 * 8. 좌우 독립 압력 안전 임계값 설정
 * 9. JSON 응답 및 디버깅 제어 기능
 */

#include <SoftwareSerial.h>
SoftwareSerial uartBridge(13, -1);

#define MAX_PRESSURE_PSI_L_INITIAL 2.1  // 최초 측정용 좌측 최대 압력
#define MAX_PRESSURE_PSI_R_INITIAL 2  // 최초 측정용 우측 최대 압력
#define MAX_PRESSURE_PSI_L_CONTINUOUS 1.3  // 연속 동작용 좌측 최대 압력
#define MAX_PRESSURE_PSI_R_CONTINUOUS 1.4   // 연속 동작용 우측 최대 압력
#define PUMP_TOGGLE_INTERVAL 5000
#define MEASUREMENT_DURATION 990
#define DEBUG 1

const int ALARM_MOTOR = 2;
const int LEFT_PUMP = 3;
const int ALARM_MOTOR_2 = 5;
const int RIGHT_PUMP = 7;
const int LEFT_AIR_PUMP = 10;
const int RIGHT_AIR_PUMP = 11;

const int PRESSURE_LEFT = A0;
const int PRESSURE_RIGHT = A1;

const unsigned long rhythmPattern[][2] = {
  {0, 40}, {240, 10}, {490, 25}, {740, 40},
  {990, 15}, {1240, 40}, {1490, 10}, {1740, 25},
  {1990, 25}, {2240, 40}, {2490, 10}, {2740, 40},
  {2990, 15}, {3240, 25}, {3490, 40}, {3740, 10},
  {3990, 40}, {4240, 10}, {4490, 25}, {4740, 40},
  {4990, 15}, {5240, 40}, {5490, 10}, {5740, 25},
  {5990, 25}, {6240, 40}, {6490, 10}, {6740, 40},
  {6990, 15}, {7240, 25}, {7490, 40}, {7740, 10}
};
const int patternLength = sizeof(rhythmPattern) / sizeof(rhythmPattern[0]);

unsigned long alarmStartMillis;
int alarmPatternIndex = 0;
bool alarmActive = false;

float leftTargetPressure = 0;
float rightTargetPressure = 0;
bool leftPumpAuto = false;
bool rightPumpAuto = false;
bool leftAirPumpAuto = false;
bool rightAirPumpAuto = false;
unsigned long leftAirPumpStartTime = 0;
unsigned long rightAirPumpStartTime = 0;
unsigned long leftAirPumpDuration = 0;
unsigned long rightAirPumpDuration = 0;
unsigned long lastPrintTime = 0;
unsigned long leftLastCheckTime = 0;
unsigned long rightLastCheckTime = 0;
bool leftAutoMaintain = false;
bool rightAutoMaintain = false;
unsigned long leftPumpToggleTime = 0;
unsigned long rightPumpToggleTime = 0;
bool leftPumpToggleState = false;
bool rightPumpToggleState = false;

void allRelaysOff();
void controlRelay(int pin, bool state);
void alarmMotorOn();
void alarmMotorOff();
void startAlarmRhythm();
void stopAlarmRhythm();
void updateAlarmRhythm();
void checkSerialCommands();
void updateAutoPressure();
void measureAndSetLeftPressure();
void measureAndSetRightPressure();
void leftPumpOn();
void leftPumpOff();
void rightPumpOn();
void rightPumpOff();
void leftAirPumpOn();
void leftAirPumpOff();
void rightAirPumpOn();
void rightAirPumpOff();
float readPressure(int pin);
float getPressurePercent(float pressure_psi, bool isLeft, bool isInitial);
void sendJsonResponse(String command, String status, String message);
void printPressureStatus();

void setup() {
  Serial.begin(9600);
  uartBridge.begin(9600);

  pinMode(13, INPUT); //센서부 신호 
  pinMode(ALARM_MOTOR, OUTPUT);
  pinMode(LEFT_PUMP, OUTPUT);
  pinMode(ALARM_MOTOR_2, OUTPUT);
  pinMode(RIGHT_PUMP, OUTPUT);
  pinMode(LEFT_AIR_PUMP, OUTPUT);
  pinMode(RIGHT_AIR_PUMP, OUTPUT);
  
  allRelaysOff();
  lastPrintTime = millis();
  leftLastCheckTime = millis();
  rightLastCheckTime = millis();
  leftPumpToggleTime = millis();
  rightPumpToggleTime = millis();
}

void loop() {
  updateAlarmRhythm();
  checkSerialCommands();
  updateAutoPressure();

  if (uartBridge.available()) {
    Serial.write(uartBridge.read());
  }
  
  unsigned long currentTime = millis();
  if (currentTime - lastPrintTime >= 500) {
    printPressureStatus();
    lastPrintTime = currentTime;
  }
  
  delay(100);
}

void allRelaysOff() {
  digitalWrite(ALARM_MOTOR, HIGH);
  digitalWrite(LEFT_PUMP, HIGH);
  digitalWrite(ALARM_MOTOR_2, HIGH);
  digitalWrite(RIGHT_PUMP, HIGH);
  digitalWrite(LEFT_AIR_PUMP, HIGH);
  digitalWrite(RIGHT_AIR_PUMP, HIGH);
}

void controlRelay(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);
}

void alarmMotorOn() {
  controlRelay(ALARM_MOTOR, true);
  controlRelay(ALARM_MOTOR_2, true);
}

void alarmMotorOff() {
  controlRelay(ALARM_MOTOR, false);
  controlRelay(ALARM_MOTOR_2, false);
}

void startAlarmRhythm() {
  alarmStartMillis = millis();
  alarmPatternIndex = 0;
  alarmActive = true;
}

void stopAlarmRhythm() {
  alarmActive = false;
  digitalWrite(ALARM_MOTOR, HIGH);
  digitalWrite(ALARM_MOTOR_2, HIGH);
}

void updateAlarmRhythm() {
  if (!alarmActive) return;
  
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - alarmStartMillis;
  
  if (alarmPatternIndex < patternLength) {
    unsigned long onStart = rhythmPattern[alarmPatternIndex][0];
    unsigned long onLength = rhythmPattern[alarmPatternIndex][1];
    
    if (elapsed >= onStart && elapsed < onStart + onLength) {
      if (alarmPatternIndex % 2 == 0) {
        controlRelay(ALARM_MOTOR, true);
        controlRelay(ALARM_MOTOR_2, false);
      } else {
        controlRelay(ALARM_MOTOR, false);
        controlRelay(ALARM_MOTOR_2, true);
      }
    } else if (elapsed >= onStart + onLength) {
      controlRelay(ALARM_MOTOR, false);
      controlRelay(ALARM_MOTOR_2, false);
      alarmPatternIndex++;
    }
  } else {
    stopAlarmRhythm();
  }
}

void checkSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command.startsWith("LEFT_SET:")) {
      leftTargetPressure = command.substring(9).toFloat();
      measureAndSetLeftPressure();
    }
    else if (command.startsWith("RIGHT_SET:")) {
      rightTargetPressure = command.substring(10).toFloat();
      measureAndSetRightPressure();
    }
    else if (command == "STOP_LEFT") {
      leftPumpAuto = false;
      leftAirPumpAuto = false;
      leftAutoMaintain = false;
      leftPumpOff();
      leftAirPumpOff();
      sendJsonResponse("STOP_LEFT", "success", "왼쪽 펌프 중지");
    }
    else if (command == "STOP_RIGHT") {
      rightPumpAuto = false;
      rightAirPumpAuto = false;
      rightAutoMaintain = false;
      rightPumpOff();
      rightAirPumpOff();
      sendJsonResponse("STOP_RIGHT", "success", "오른쪽 펌프 중지");
    }
    else if (command.startsWith("SET_ALARM:")) {
      int alarmState = command.substring(10).toInt();
      if (alarmState == 1) {
        startAlarmRhythm();
        sendJsonResponse("SET_ALARM", "success", "알람 시작");
      } else {
        stopAlarmRhythm();
        sendJsonResponse("SET_ALARM", "success", "알람 정지");
      }
    }
  }
}

void updateAutoPressure() {
  unsigned long currentTime = millis();
  
  if (leftPumpAuto) {
    if (currentTime - leftPumpToggleTime >= PUMP_TOGGLE_INTERVAL) {
      leftPumpToggleState = !leftPumpToggleState;
      controlRelay(LEFT_PUMP, leftPumpToggleState);
      leftPumpToggleTime = currentTime;
    }
    
    float leftPressurePercent = getPressurePercent(readPressure(PRESSURE_LEFT), true, false);
    if (leftPressurePercent >= leftTargetPressure) {
      leftPumpAuto = false;
      leftPumpOff();
      if (DEBUG) {
        Serial.print("왼쪽 펌프 목표 달성: ");
        Serial.print(leftPressurePercent);
        Serial.println("%");
      }
    }
  }
  
  if (rightPumpAuto) {
    if (currentTime - rightPumpToggleTime >= PUMP_TOGGLE_INTERVAL) {
      rightPumpToggleState = !rightPumpToggleState;
      controlRelay(RIGHT_PUMP, rightPumpToggleState);
      rightPumpToggleTime = currentTime;
    }
    
    float rightPressurePercent = getPressurePercent(readPressure(PRESSURE_RIGHT), false, false);
    if (rightPressurePercent >= rightTargetPressure) {
      rightPumpAuto = false;
      rightPumpOff();
      if (DEBUG) {
        Serial.print("오른쪽 펌프 목표 달성: ");
        Serial.print(rightPressurePercent);
        Serial.println("%");
      }
    }
  }
  
  if (leftAirPumpAuto) {
    if (currentTime - leftAirPumpStartTime >= leftAirPumpDuration) {
      leftAirPumpAuto = false;
      leftAirPumpOff();
      if (DEBUG) Serial.println("왼쪽 방출펌프 완료");
    }
  }
  
  if (rightAirPumpAuto) {
    if (currentTime - rightAirPumpStartTime >= rightAirPumpDuration) {
      rightAirPumpAuto = false;
      rightAirPumpOff();
      if (DEBUG) Serial.println("오른쪽 방출펌프 완료");
    }
  }
  
  if (leftAutoMaintain && currentTime - leftLastCheckTime >= 600000) {
    float leftPressurePercent = getPressurePercent(readPressure(PRESSURE_LEFT), true, false);
    if (leftPressurePercent < leftTargetPressure) {
      if (DEBUG) {
        Serial.print("왼쪽 압력 하락 감지: ");
        Serial.print(leftPressurePercent);
        Serial.println("% - 점진적 보충 시작");
      }
      leftPumpAuto = true;
      leftPumpToggleTime = currentTime;
    }
    leftLastCheckTime = currentTime;
  }
  
  if (rightAutoMaintain && currentTime - rightLastCheckTime >= 600000) {
    float rightPressurePercent = getPressurePercent(readPressure(PRESSURE_RIGHT), false, false);
    if (rightPressurePercent < rightTargetPressure) {
      if (DEBUG) {
        Serial.print("오른쪽 압력 하락 감지: ");
        Serial.print(rightPressurePercent);
        Serial.println("% - 점진적 보충 시작");
      }
      rightPumpAuto = true;
      rightPumpToggleTime = currentTime;
    }
    rightLastCheckTime = currentTime;
  }
}

void measureAndSetLeftPressure() {
  if (DEBUG) {
    Serial.print("왼쪽 목표: ");
    Serial.print(leftTargetPressure);
    Serial.println("% - 고정 790ms 펌프+측정 시작 (최초측정모드)");
  }
  
  leftPumpAuto = false;
  leftAirPumpAuto = false;
  leftAutoMaintain = false;
  leftPumpOff();
  leftAirPumpOff();
  
  float maxPressure = 0;
  unsigned long startTime = millis();
  
  leftPumpOn();
  
  while (millis() - startTime < MEASUREMENT_DURATION) {
    float currentPressure = getPressurePercent(readPressure(PRESSURE_LEFT), true, true);
    if (currentPressure > maxPressure) {
      maxPressure = currentPressure;
    }
    delay(10);
  }
  
  leftPumpOff();
  if (DEBUG) {
    Serial.print("측정된 최대 압력 (최초기준): ");
    Serial.print(maxPressure);
    Serial.println("%");
  }
  
  leftAutoMaintain = true;
  leftLastCheckTime = millis();
  
  if (maxPressure < leftTargetPressure) {
    leftPumpAuto = true;
    leftPumpToggleTime = millis();
    if (DEBUG) Serial.println("목표 미달 - 점진적 펌프 동작 (연속기준)");
    sendJsonResponse("LEFT_SET", "success", "목표 미달 - 점진적 펌프 동작");
  } else {
    float excessPercent = maxPressure - leftTargetPressure;
    leftAirPumpDuration = (unsigned long)(excessPercent * 30000);
    leftAirPumpStartTime = millis();
    leftAirPumpAuto = true;
    leftAirPumpOn();
    if (DEBUG) {
      Serial.print("목표 초과 - 방출펌프 ");
      Serial.print(excessPercent);
      Serial.print("% 초과분 (");
      Serial.print(leftAirPumpDuration/1000);
      Serial.println("초) 동작");
    }
    sendJsonResponse("LEFT_SET", "success", "목표 초과 - 방출펌프 동작");
  }
}

void measureAndSetRightPressure() {
  if (DEBUG) {
    Serial.print("오른쪽 목표: ");
    Serial.print(rightTargetPressure);
    Serial.println("% - 고정 790ms 펌프+측정 시작 (최초측정모드)");
  }
  
  rightPumpAuto = false;
  rightAirPumpAuto = false;
  rightAutoMaintain = false;
  rightPumpOff();
  rightAirPumpOff();
  
  float maxPressure = 0;
  unsigned long startTime = millis();
  
  rightPumpOn();
  
  while (millis() - startTime < MEASUREMENT_DURATION) {
    float currentPressure = getPressurePercent(readPressure(PRESSURE_RIGHT), false, true);
    if (currentPressure > maxPressure) {
      maxPressure = currentPressure;
    }
    delay(10);
  }
  
  rightPumpOff();
  if (DEBUG) {
    Serial.print("측정된 최대 압력 (최초기준): ");
    Serial.print(maxPressure);
    Serial.println("%");
  }
  
  rightAutoMaintain = true;
  rightLastCheckTime = millis();
  
  if (maxPressure < rightTargetPressure) {
    rightPumpAuto = true;
    rightPumpToggleTime = millis();
    if (DEBUG) Serial.println("목표 미달 - 점진적 펌프 동작 (연속기준)");
    sendJsonResponse("RIGHT_SET", "success", "목표 미달 - 점진적 펌프 동작");
  } else {
    float excessPercent = maxPressure - rightTargetPressure;
    rightAirPumpDuration = (unsigned long)(excessPercent * 30000);
    rightAirPumpStartTime = millis();
    rightAirPumpAuto = true;
    rightAirPumpOn();
    if (DEBUG) {
      Serial.print("목표 초과 - 방출펌프 ");
      Serial.print(excessPercent);
      Serial.print("% 초과분 (");
      Serial.print(rightAirPumpDuration/1000);
      Serial.println("초) 동작");
    }
    sendJsonResponse("RIGHT_SET", "success", "목표 초과 - 방출펌프 동작");
  }
}

void leftPumpOn() {
  controlRelay(LEFT_PUMP, true);
}

void leftPumpOff() {
  controlRelay(LEFT_PUMP, false);
}

void rightPumpOn() {
  controlRelay(RIGHT_PUMP, true);
}

void rightPumpOff() {
  controlRelay(RIGHT_PUMP, false);
}

void leftAirPumpOn() {
  controlRelay(LEFT_AIR_PUMP, true);
}

void leftAirPumpOff() {
  controlRelay(LEFT_AIR_PUMP, false);
}

void rightAirPumpOn() {
  controlRelay(RIGHT_AIR_PUMP, true);
}

void rightAirPumpOff() {
  controlRelay(RIGHT_AIR_PUMP, false);
}

float readPressure(int pin) {
  int rawValue = analogRead(pin);
  float voltage = rawValue * (5.0 / 1023.0);
  float pressure_psi = (voltage - 0.5) * (200.0 / 4.0);
  
  if(pressure_psi < 0) pressure_psi = 0;
  
  return pressure_psi;
}

float getPressurePercent(float pressure_psi, bool isLeft, bool isInitial) {
  if (isLeft) {
    if (isInitial) {
      return (pressure_psi / MAX_PRESSURE_PSI_L_INITIAL) * 100.0;
    } else {
      return (pressure_psi / MAX_PRESSURE_PSI_L_CONTINUOUS) * 100.0;
    }
  } else {
    if (isInitial) {
      return (pressure_psi / MAX_PRESSURE_PSI_R_INITIAL) * 100.0;
    } else {
      return (pressure_psi / MAX_PRESSURE_PSI_R_CONTINUOUS) * 100.0;
    }
  }
}

void sendJsonResponse(String command, String status, String message) {
  float leftPressure = readPressure(PRESSURE_LEFT);
  float rightPressure = readPressure(PRESSURE_RIGHT);
  
  Serial.print("{");
  Serial.print("\"command\":\"" + command + "\",");
  Serial.print("\"status\":\"" + status + "\",");
  Serial.print("\"message\":\"" + message + "\",");
  Serial.print("\"left_pressure\":" + String(leftPressure) + ",");
  Serial.print("\"left_percent_initial\":" + String(getPressurePercent(leftPressure, true, true)) + ",");
  Serial.print("\"left_percent_continuous\":" + String(getPressurePercent(leftPressure, true, false)) + ",");
  Serial.print("\"right_pressure\":" + String(rightPressure) + ",");
  Serial.print("\"right_percent_initial\":" + String(getPressurePercent(rightPressure, false, true)) + ",");
  Serial.print("\"right_percent_continuous\":" + String(getPressurePercent(rightPressure, false, false)) + ",");
  Serial.print("\"timestamp\":" + String(millis()));
  Serial.println("}");
}

void printPressureStatus() {
  if (DEBUG) {
    float leftPressure = readPressure(PRESSURE_LEFT);
    float rightPressure = readPressure(PRESSURE_RIGHT);
    
    Serial.print("왼쪽 압력: ");
    Serial.print(leftPressure);
    Serial.print(" PSI (최초:");
    Serial.print(getPressurePercent(leftPressure, true, true));
    Serial.print("%, 연속:");
    Serial.print(getPressurePercent(leftPressure, true, false));
    Serial.print("%) | 오른쪽 압력: ");
    Serial.print(rightPressure);
    Serial.print(" PSI (최초:");
    Serial.print(getPressurePercent(rightPressure, false, true));
    Serial.print("%, 연속:");
    Serial.print(getPressurePercent(rightPressure, false, false));
    Serial.println("%)");
  }
}