# BaegaePro 스마트 베개 통합 백엔드 API 명세서

## 📋 개요

BaegaePro 스마트 베개 IoT 기기와 모바일 앱을 위한 통합 백엔드 서버 API 명세서입니다. ESP32-C6 기기와의 실시간 통신, 수면 데이터 분석, 코골이 감지 및 베개 각도 자동 조절, NEXTION HMI 연동, FOTA 업데이트 등의 기능을 제공합니다.

## 🔬 베개 각도 조절 로직 (Anti-Snoring Algorithm)

### 코골이와 베개 각도의 과학적 근거

**연구 기반 사실:**
- 베개 높이 13cm 이상: 기도 압박으로 코골이 유발
- 베개 너무 낮음: 혀가 뒤로 처져 상기도 막힘
- 적정 각도: 7-10도 상승으로 기도 확장 효과
- 30분 주기 조절: 목 근육 피로 방지 및 혈액순환 개선

### 베개 각도 제어 알고리즘

```
1단계: 코골이 감지 (MEMS 마이크 + AI 분석)
2단계: 점진적 각도 상승 (1도씩 최대 12도까지)
3단계: 코골이 중단 확인 (30초 모니터링)
4단계: 안정화 유지 (15분간 현재 각도 유지)
5단계: 점진적 하강 (30분 후 1도씩 기본 각도로 복원)
```

**Pump Control Response:**
```json
{
  "pump": 80,           // 0-100 (0도-12도 각도)
  "duration": 30,       // 유지 시간(초)
  "mode": "gradual",    // gradual/immediate
  "target_angle": 8.5,  // 목표 각도
  "reason": "snore_detected"
}
```

## 🧠 수면 점수 계산 알고리즘 (100점 만점)

### 점수 구성 (가중치 기반)

**1. 수면 시간 (30%)**
- 최적 범위: 7.5-9시간 = 30점
- 부족/과다: 선형 감소
- 공식: `min(30, (sleep_hours / 8.0) * 30)`

**2. 수면 효율 (25%)**  
- 침대에 누운 시간 대비 실제 수면 시간
- 85% 이상: 25점
- 공식: `(sleep_efficiency / 85) * 25`

**3. 수면 안정성 (20%)**
- 각성 횟수 및 뒤척임 빈도
- 3회 이하 각성: 20점
- 공식: `max(0, 20 - (wakeup_count * 4))`

**4. 심박수 변화 (15%)**
- 수면 중 심박수가 안정시보다 10% 이상 감소: 15점
- 공식: `((resting_hr - sleep_hr) / resting_hr) * 150`

**5. 코골이 빈도 (10%)**
- 총 수면 시간 대비 코골이 시간
- 5% 미만: 10점
- 공식: `max(0, 10 - (snore_ratio * 200))`

### 최종 점수 산출

```javascript
const calculateSleepScore = (sleepData) => {
  const timeScore = Math.min(30, (sleepData.sleepHours / 8.0) * 30);
  const efficiencyScore = (sleepData.sleepEfficiency / 85) * 25;
  const stabilityScore = Math.max(0, 20 - (sleepData.wakeupCount * 4));
  const hrScore = Math.min(15, ((sleepData.restingHr - sleepData.sleepHr) / sleepData.restingHr) * 150);
  const snoreScore = Math.max(0, 10 - (sleepData.snoreRatio * 200));
  
  return Math.round(timeScore + efficiencyScore + stabilityScore + hrScore + snoreScore);
};
```

## 🏗️ 시스템 아키텍처

### 백엔드 구조
```
backend/
├── src/
│   ├── controllers/
│   │   ├── deviceController.js      # 기기 관리
│   │   ├── authController.js        # 인증/회원가입
│   │   ├── sleepController.js       # 수면 세션 관리
│   │   ├── dataController.js        # 센서 데이터
│   │   ├── soundController.js       # 오디오 처리
│   │   ├── alarmController.js       # 알람 기능
│   │   ├── analyticsController.js   # 수면 분석
│   │   └── homeController.js        # 대시보드
│   ├── services/
│   │   ├── snoreDetectionService.js # 코골이 AI 분석
│   │   ├── pumpControlService.js    # 베개 각도 제어
│   │   ├── sleepScoreService.js     # 수면 점수 계산
│   │   ├── weatherService.js        # 날씨 API 연동
│   │   └── notificationService.js   # FCM 알림
│   └── algorithms/
│       ├── antiSnoringAlgorithm.js  # 코골이 방지 로직
│       ├── sleepStageDetection.js   # 수면 단계 분석
│       └── pumpScheduler.js         # 베개 각도 스케줄링
```

### 데이터베이스 스키마 (MySQL 8.0)

**사용자 관리**
```sql
CREATE TABLE users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  email VARCHAR(255) UNIQUE NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  name VARCHAR(100) NOT NULL,
  phone VARCHAR(20),
  birth_date DATE,
  gender ENUM('M', 'F', 'OTHER'),
  fcm_token VARCHAR(255),
  timezone VARCHAR(50) DEFAULT 'Asia/Seoul',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_email (email),
  INDEX idx_phone (phone)
);

CREATE TABLE devices (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) UNIQUE NOT NULL,
  user_id BIGINT NOT NULL,
  device_name VARCHAR(100) DEFAULT '베개프로',
  device_type VARCHAR(50) DEFAULT 'baegaepro-v1',
  firmware_version VARCHAR(32) DEFAULT '1.0.0',
  status ENUM('registered', 'online', 'offline', 'updating', 'error') DEFAULT 'registered',
  location_city VARCHAR(100) DEFAULT 'Seoul',
  timezone VARCHAR(50) DEFAULT 'Asia/Seoul',
  pump_angle TINYINT DEFAULT 0,
  last_seen TIMESTAMP NULL,
  last_pump_action TIMESTAMP NULL,
  settings JSON,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_device_id (device_id),
  INDEX idx_user_id (user_id),
  INDEX idx_status (status),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);
```

**수면 세션 관리**
```sql
CREATE TABLE sleep_sessions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  session_id VARCHAR(36) UNIQUE NOT NULL,
  device_id VARCHAR(16) NOT NULL,
  user_id BIGINT NOT NULL,
  start_time TIMESTAMP NOT NULL,
  end_time TIMESTAMP NULL,
  local_start_time VARCHAR(30) NOT NULL,
  local_end_time VARCHAR(30) NULL,
  status ENUM('active', 'completed', 'interrupted', 'processing') DEFAULT 'active',
  total_sleep_minutes INT DEFAULT 0,
  sleep_score TINYINT DEFAULT 0,
  sleep_efficiency DECIMAL(5,2) DEFAULT 0,
  wakeup_count SMALLINT DEFAULT 0,
  snore_count SMALLINT DEFAULT 0,
  snore_duration_minutes INT DEFAULT 0,
  pump_activations SMALLINT DEFAULT 0,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  completed_at TIMESTAMP NULL,
  INDEX idx_session_id (session_id),
  INDEX idx_device_id (device_id),
  INDEX idx_user_id (user_id),
  INDEX idx_start_time (start_time),
  INDEX idx_status (status),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);
```

**센서 데이터 (TimescaleDB 권장)**
```sql
CREATE TABLE sensor_readings (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  session_id VARCHAR(36) NOT NULL,
  device_id VARCHAR(16) NOT NULL,
  timestamp TIMESTAMP(3) NOT NULL,
  data_type ENUM('gyro', 'heart_rate', 'spo2', 'temp', 'humidity', 'noise') NOT NULL,
  value DECIMAL(10,3) NOT NULL,
  unit VARCHAR(10),
  meta JSON,
  INDEX idx_session_timestamp (session_id, timestamp),
  INDEX idx_device_timestamp (device_id, timestamp),
  INDEX idx_data_type (data_type),
  FOREIGN KEY (session_id) REFERENCES sleep_sessions(session_id) ON DELETE CASCADE
);
```

**코골이 & 베개 제어 로그**
```sql
CREATE TABLE snore_events (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  session_id VARCHAR(36) NOT NULL,
  device_id VARCHAR(16) NOT NULL,
  detected_at TIMESTAMP(3) NOT NULL,
  confidence DECIMAL(4,2) NOT NULL,
  duration_seconds SMALLINT DEFAULT 0,
  pump_activated BOOLEAN DEFAULT FALSE,
  pump_angle_before TINYINT DEFAULT 0,
  pump_angle_after TINYINT DEFAULT 0,
  ai_analysis JSON,
  INDEX idx_session_detected (session_id, detected_at),
  INDEX idx_device_detected (device_id, detected_at),
  FOREIGN KEY (session_id) REFERENCES sleep_sessions(session_id) ON DELETE CASCADE
);

CREATE TABLE pump_actions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  session_id VARCHAR(36),
  device_id VARCHAR(16) NOT NULL,
  action_time TIMESTAMP(3) NOT NULL,
  action_type ENUM('raise', 'lower', 'maintain', 'reset') NOT NULL,
  from_angle TINYINT NOT NULL,
  to_angle TINYINT NOT NULL,
  trigger_reason ENUM('snore_detected', 'schedule', 'manual', 'comfort') NOT NULL,
  duration_seconds INT DEFAULT 0,
  success BOOLEAN DEFAULT TRUE,
  INDEX idx_session_action (session_id, action_time),
  INDEX idx_device_action (device_id, action_time),
  FOREIGN KEY (session_id) REFERENCES sleep_sessions(session_id) ON DELETE SET NULL
);
```

**알람 시스템**
```sql
CREATE TABLE alarms (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) NOT NULL,
  user_id BIGINT NOT NULL,
  alarm_time TIME NOT NULL,
  days_of_week VARCHAR(7) DEFAULT '1111100',
  enabled BOOLEAN DEFAULT TRUE,
  smart_wake BOOLEAN DEFAULT TRUE,
  wake_window_minutes TINYINT DEFAULT 30,
  sound_id VARCHAR(50) DEFAULT 'gentle_wake',
  volume TINYINT DEFAULT 70,
  vibration BOOLEAN DEFAULT TRUE,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_device_id (device_id),
  INDEX idx_user_id (user_id),
  INDEX idx_enabled (enabled),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE alarm_sessions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  alarm_id BIGINT NOT NULL,
  session_id VARCHAR(36),
  scheduled_time TIMESTAMP NOT NULL,
  actual_wake_time TIMESTAMP NULL,
  status ENUM('scheduled', 'triggered', 'snoozed', 'dismissed', 'missed') DEFAULT 'scheduled',
  snooze_count TINYINT DEFAULT 0,
  sleep_stage_at_wake ENUM('deep', 'light', 'rem', 'awake') NULL,
  user_rating TINYINT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_alarm_id (alarm_id),
  INDEX idx_session_id (session_id),
  INDEX idx_scheduled_time (scheduled_time),
  INDEX idx_status (status),
  FOREIGN KEY (alarm_id) REFERENCES alarms(id) ON DELETE CASCADE,
  FOREIGN KEY (session_id) REFERENCES sleep_sessions(session_id) ON DELETE SET NULL
);
```

## 🔐 인증 시스템

### 헤더 인증 방식
```
X-Device-Id: <device_unique_id>      # ESP32 기기 식별
Authorization: Bearer <jwt_token>    # 사용자/기기 인증
X-Timestamp: <ISO8601_timestamp>     # 요청 시각
```

### JWT 토큰 구조
```json
{
  "sub": "user_12345",
  "device_id": "A1B2C3D4",
  "type": "device|user",
  "exp": 1735689600,
  "iat": 1735603200
}
```

## 📡 API 엔드포인트 명세

### 🚀 사용자 인증 & 회원가입

#### POST /api/v1/auth/register
**Description:** 사용자 회원가입

**Request:**
```json
{
  "email": "user@example.com",
  "password": "securePassword123",
  "name": "홍길동",
  "phone": "010-1234-5678",
  "birth_date": "1990-01-15",
  "gender": "M"
}
```

**Response (201):**
```json
{
  "success": true,
  "message": "회원가입이 완료되었습니다",
  "data": {
    "user_id": "user_12345",
    "access_token": "eyJ0eXAiOiJKV1QiLCJhbGc...",
    "refresh_token": "dGhpcyBpcyBhIHJlZnJlc2g...",
    "expires_in": 3600
  }
}
```

#### POST /api/v1/auth/login
**Description:** 사용자 로그인

**Request:**
```json
{
  "email": "user@example.com",
  "password": "securePassword123",
  "fcm_token": "device_fcm_token_here"
}
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "user_id": "user_12345",
    "access_token": "eyJ0eXAiOiJKV1QiLCJhbGc...",
    "refresh_token": "dGhpcyBpcyBhIHJlZnJlc2g...",
    "user": {
      "name": "홍길동",
      "email": "user@example.com",
      "devices_count": 1
    }
  }
}
```

### 🎛️ 기기 관리 API

#### POST /api/v1/device/register
**Description:** 새 기기 등록 (사용자가 앱에서 실행)

**Headers:**
```
Authorization: Bearer <user_token>
Content-Type: application/json
```

**Request:**
```json
{
  "device_id": "A1B2C3D4",
  "device_name": "침실 베개프로",
  "location_city": "Seoul",
  "timezone": "Asia/Seoul"
}
```

**Response (201):**
```json
{
  "success": true,
  "message": "기기가 성공적으로 등록되었습니다",
  "data": {
    "device_id": "A1B2C3D4",
    "device_token": "device_jwt_token_here",
    "provisioning_code": "PROV-12345678",
    "expires_in": 300,
    "status": "registered"
  }
}
```

#### GET /api/v1/device/{deviceId}/status
**Description:** 앱에서 기기 등록 상태 폴링 확인

**Headers:**
```
Authorization: Bearer <user_token>
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "device_id": "A1B2C3D4",
    "status": "online",
    "firmware_version": "1.0.0",
    "last_seen": "2025-08-29T14:30:00+09:00",
    "wifi_rssi": -45,
    "battery_level": 85,
    "is_setup_complete": true
  }
}
```

#### PUT /api/v1/device/{deviceId}/settings
**Description:** 기기 상세 설정 업데이트

**Headers:**
```
Authorization: Bearer <user_token>
```

**Request:**
```json
{
  "device_name": "침실 베개프로",
  "location_city": "Busan",
  "timezone": "Asia/Seoul",
  "pump_sensitivity": 7,
  "max_pump_angle": 10,
  "auto_adjustment": true,
  "night_mode": {
    "enabled": true,
    "start_time": "22:00",
    "end_time": "07:00"
  }
}
```

#### DELETE /api/v1/device/{deviceId}
**Description:** 기기 삭제

**Headers:**
```
Authorization: Bearer <user_token>
```

**Response (200):**
```json
{
  "success": true,
  "message": "기기가 삭제되었습니다"
}
```

### 🏠 대시보드 & 홈 데이터 API

#### GET /api/v1/home
**Description:** NEXTION HMI 화면용 홈 데이터 (ESP32 요청)

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "currentTime": "2025-08-29T14:30:00+09:00",
    "weather": "맑음",
    "temperature": "22",
    "humidity": "65",
    "sleepScore": "85",
    "noiseLevel": "24",
    "alarmTime": "7:20",
    "status": "정상 작동",
    "pumpAngle": 3
  }
}
```

#### GET /api/v1/weather/initial
**Description:** 날씨 서비스 초기 설정 데이터

**Headers:**
```
Authorization: Bearer <user_token>
```

**Request Query:**
```
?city=Seoul&lat=37.5665&lon=126.9780
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "city": "Seoul",
    "country": "KR",
    "timezone": "Asia/Seoul",
    "current": {
      "temperature": 22.5,
      "humidity": 65,
      "weather": "맑음",
      "weather_code": "clear",
      "updated_at": "2025-08-29T14:30:00+09:00"
    },
    "forecast": [
      {
        "date": "2025-08-30",
        "temp_min": 18,
        "temp_max": 26,
        "weather": "구름조금"
      }
    ]
  }
}
```

### 😴 수면 세션 관리 API

#### POST /api/v1/sleep/start
**Description:** 수면 세션 시작

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
X-Timestamp: 2025-08-29T23:12:34+09:00
```

**Request:**
```json
{
  "session_local_start": "2025-08-29T23:12:34+09:00",
  "initial_pump_angle": 0,
  "room_temperature": 24.5,
  "room_humidity": 60
}
```

**Response (201):**
```json
{
  "success": true,
  "data": {
    "session_id": "sess-uuid-0001",
    "started_at": "2025-08-29T23:12:34+09:00",
    "status": "active"
  }
}
```

#### POST /api/v1/sleep/end
**Description:** 수면 세션 종료

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
```

**Request:**
```json
{
  "session_id": "sess-uuid-0001",
  "session_local_end": "2025-08-30T07:05:22+09:00",
  "final_pump_angle": 2,
  "total_snore_count": 12,
  "total_pump_activations": 8
}
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "session_id": "sess-uuid-0001",
    "ended_at": "2025-08-30T07:05:22+09:00",
    "total_sleep_minutes": 458,
    "sleep_score": 82,
    "processing_job_id": "job-uuid-1234",
    "status": "processing"
  }
}
```

#### GET /api/v1/sleep/{sessionId}
**Description:** 수면 세션 상세 조회

**Headers:**
```
Authorization: Bearer <user_token>
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "session_id": "sess-uuid-0001",
    "date": "2025-08-29",
    "start_time": "23:12:34",
    "end_time": "07:05:22",
    "total_sleep_minutes": 458,
    "sleep_score": 82,
    "sleep_efficiency": 89.2,
    "sleep_stages": {
      "deep_sleep_minutes": 105,
      "light_sleep_minutes": 285,
      "rem_sleep_minutes": 68,
      "awake_minutes": 15
    },
    "snoring": {
      "total_events": 12,
      "total_duration_minutes": 23,
      "severity": "mild"
    },
    "pump_actions": {
      "total_activations": 8,
      "max_angle_reached": 8.5,
      "average_response_time": 12.3
    },
    "vitals": {
      "avg_heart_rate": 58,
      "heart_rate_variability": 42,
      "avg_spo2": 97,
      "avg_temperature": 36.2
    }
  }
}
```

### 📊 센서 데이터 수집 API

#### POST /api/v1/sleep/data/{sessionId}
**Description:** 센서 데이터 배치 업로드

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
Content-Type: application/json
```

**Request:**
```json
{
  "samples": [
    {
      "ts": "2025-08-30T00:00:00+09:00",
      "gyro": {"x": 0.003, "y": -0.002, "z": 0.000},
      "heart_rate": 62,
      "spo2": 99,
      "skin_temp": 33.8,
      "room_temp": 24.2,
      "room_humidity": 58,
      "noise_level": 25.3
    },
    {
      "ts": "2025-08-30T00:01:00+09:00",
      "gyro": {"x": 0.001, "y": -0.001, "z": 0.000},
      "heart_rate": 61,
      "spo2": 98,
      "skin_temp": 33.6,
      "room_temp": 24.1,
      "room_humidity": 58,
      "noise_level": 26.1
    }
  ],
  "meta": {
    "sample_rate": 0.0167,
    "batch_sequence": 42,
    "compression": "none"
  }
}
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "inserted": 2,
    "session_id": "sess-uuid-0001",
    "next_expected_sequence": 43,
    "pump_command": {
      "pump": 65,
      "duration": 30,
      "mode": "gradual",
      "reason": "comfort_adjustment"
    }
  }
}
```

### 🎵 오디오 데이터 & 코골이 감지 API

#### POST /api/v1/sleep/sound
**Description:** 오디오 PCM 데이터 청크 업로드

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
X-Session-Id: sess-uuid-0001
X-Chunk-Index: 0
X-Chunk-Final: false
X-Sample-Rate: 16000
X-Sample-Format: I2S_32_LE_24
Content-Type: application/octet-stream
Content-Length: 64000
```

**Body:** Raw binary PCM data (int32 little-endian)

**Response (200):**
```json
{
  "success": true,
  "data": {
    "session_id": "sess-uuid-0001",
    "chunk_index": 0,
    "stored": true,
    "pump_command": {
      "pump": 75,
      "duration": 45,
      "mode": "gradual",
      "target_angle": 7.5,
      "reason": "snore_detected",
      "confidence": 0.87
    }
  }
}
```

**Response (Final Chunk):**
```json
{
  "success": true,
  "data": {
    "session_id": "sess-uuid-0001",
    "assembled_file_id": "audio-uuid-5678",
    "processing_job_id": "job-uuid-9999",
    "total_chunks": 125,
    "total_size_bytes": 8000000
  }
}
```

### ⏰ 알람 시스템 API

#### POST /api/v1/alarm
**Description:** 알람 설정

**Headers:**
```
Authorization: Bearer <user_token>
X-Device-Id: A1B2C3D4
```

**Request:**
```json
{
  "alarm_time": "07:20:00",
  "days_of_week": "1111100",
  "enabled": true,
  "smart_wake": true,
  "wake_window_minutes": 30,
  "sound_id": "gentle_birds",
  "volume": 70,
  "vibration": true
}
```

**Response (201):**
```json
{
  "success": true,
  "data": {
    "alarm_id": "alarm_001",
    "alarm_time": "07:20:00",
    "next_trigger": "2025-08-30T07:20:00+09:00",
    "days_of_week": "1111100",
    "enabled": true
  }
}
```

#### PUT /api/v1/alarm/{alarmId}/pause
**Description:** 알람 일시중지

**Response (200):**
```json
{
  "success": true,
  "message": "알람이 일시중지되었습니다",
  "paused_until": "2025-08-31T07:20:00+09:00"
}
```

#### PUT /api/v1/alarm/{alarmId}/skip-once
**Description:** 알람 하루만 끄기

**Response (200):**
```json
{
  "success": true,
  "message": "내일 알람이 비활성화되었습니다",
  "next_trigger": "2025-08-31T07:20:00+09:00"
}
```

#### GET /api/v1/alarm/sessions
**Description:** 알람 기록 조회 (월별)

**Query Parameters:**
```
?year=2025&month=8&device_id=A1B2C3D4
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "month": "2025-08",
    "total_alarms": 31,
    "successful_wakes": 28,
    "missed_alarms": 2,
    "average_snooze_count": 1.2,
    "sessions": [
      {
        "date": "2025-08-01",
        "scheduled_time": "07:20:00",
        "actual_wake_time": "07:18:32",
        "status": "dismissed",
        "snooze_count": 0,
        "sleep_stage_at_wake": "light",
        "user_rating": 4
      }
    ]
  }
}
```

### 📈 수면 분석 & 통계 API

#### GET /api/v1/analytics/dashboard
**Description:** 메인 대시보드 데이터 (앱 홈화면용)

**Headers:**
```
Authorization: Bearer <user_token>
```

**Query Parameters:**
```
?device_id=A1B2C3D4&period=7d
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "period": "7d",
    "summary": {
      "avg_sleep_score": 82.3,
      "avg_sleep_duration": "7시간 38분",
      "total_snore_events": 45,
      "pump_effectiveness": 91.2
    },
    "recent_session": {
      "date": "2025-08-29",
      "sleep_score": 85,
      "sleep_duration": "7시간 53분",
      "snore_count": 12,
      "pump_activations": 8
    },
    "trends": {
      "sleep_score_trend": "+3.2",
      "snoring_trend": "-12.5",
      "sleep_duration_trend": "+0.3"
    },
    "weather_impact": {
      "temperature": "22°C",
      "humidity": "65%",
      "sleep_correlation": 0.78
    }
  }
}
```

#### GET /api/v1/analytics/monthly/{year}/{month}
**Description:** 월별 상세 리포트

**Response (200):**
```json
{
  "success": true,
  "data": {
    "month": "2025-08",
    "sleep_nights": 29,
    "avg_sleep_score": 81.7,
    "sleep_efficiency": 88.4,
    "weekly_breakdown": [
      {
        "week": 1,
        "avg_score": 79.2,
        "sleep_debt": -45,
        "consistency": 0.82
      }
    ],
    "snoring_analysis": {
      "total_events": 156,
      "avg_per_night": 5.4,
      "severity_distribution": {
        "mild": 78,
        "moderate": 67,
        "severe": 11
      },
      "improvement_rate": "+23.1%"
    },
    "pump_performance": {
      "total_activations": 134,
      "success_rate": 92.5,
      "avg_response_time": 11.8,
      "comfort_score": 4.2
    }
  }
}
```

### 💾 하트비트 & 헬스체크 API

#### POST /api/v1/device/{deviceId}/heartbeat
**Description:** 기기 하트비트 (30초마다)

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
X-Timestamp: 2025-08-29T14:30:00+09:00
```

**Request:**
```json
{
  "uptime": 86400,
  "free_memory": 180000,
  "wifi_rssi": -45,
  "battery_level": 85,
  "pump_angle": 3,
  "room_temp": 24.2,
  "room_humidity": 58,
  "last_pump_action": "2025-08-29T03:25:12+09:00"
}
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "server_time": "2025-08-29T14:30:15+09:00",
    "next_home_update": "2025-08-29T14:35:00+09:00",
    "config_updates": {
      "pump_sensitivity": 8,
      "max_angle": 12,
      "night_mode_enabled": true
    },
    "commands": [
      {
        "type": "pump_adjust",
        "pump": 0,
        "reason": "comfort_schedule"
      }
    ]
  }
}
```

### 🚀 FOTA (Firmware Over The Air) API

#### GET /api/v1/firmware/check/{deviceId}
**Description:** 펌웨어 업데이트 확인

**Headers:**
```
Authorization: Bearer <device_token>
```

**Response (Update Available):**
```json
{
  "success": true,
  "data": {
    "version": "1.1.0",
    "download_url": "https://firmware.baegaepro.com/releases/baegaepro-v1.1.0.bin",
    "sha256": "a1b2c3d4e5f6789...",
    "file_size": 1048576,
    "release_notes": "베개 각도 제어 알고리즘 개선, 배터리 효율성 향상",
    "mandatory": false,
    "released_at": "2025-08-26T10:30:00+09:00",
    "min_battery_level": 50
  }
}
```

#### POST /api/v1/firmware/update/progress
**Description:** 업데이트 진행 상황 보고

**Headers:**
```
Authorization: Bearer <device_token>
X-Device-Id: A1B2C3D4
```

**Request:**
```json
{
  "status": "downloading",
  "progress_percent": 45,
  "current_step": "Downloading firmware",
  "estimated_remaining": 120
}
```

### 📎 파일 업로드 API

#### POST /api/v1/upload/attachment
**Description:** 파일 첨부 (수면 기록 관련 사진/메모 등)

**Headers:**
```
Authorization: Bearer <user_token>
Content-Type: multipart/form-data
```

**Request (Multipart):**
```
file: <binary_file_data>
type: "sleep_note"
session_id: "sess-uuid-0001"
description: "수면 환경 사진"
```

**Response (201):**
```json
{
  "success": true,
  "data": {
    "file_id": "file_uuid_001",
    "filename": "sleep_environment.jpg",
    "file_url": "https://cdn.baegaepro.com/uploads/file_uuid_001.jpg",
    "file_size": 2048576,
    "mime_type": "image/jpeg",
    "uploaded_at": "2025-08-29T14:30:00+09:00"
  }
}
```

## 🤖 베개 각도 제어 알고리즘 상세

### 실시간 코골이 감지 프로세스

1. **오디오 신호 전처리**
   - 16kHz 샘플링으로 수집된 PCM 데이터
   - 밴드패스 필터 적용 (20Hz - 2kHz)
   - 노이즈 게이트 적용 (임계값 30dB)

2. **AI 코골이 판별**
   - CNN 기반 스펙트로그램 분석
   - 신뢰도 임계값: 85% 이상
   - False Positive 방지: 연속 3초 이상 감지시만 판정

3. **베개 각도 조절 로직**
```javascript
const adjustPumpAngle = async (snoreEvent) => {
  const currentAngle = await getCurrentPumpAngle(deviceId);
  const targetAngle = Math.min(12, currentAngle + 2); // 최대 12도
  
  const pumpCommand = {
    pump: Math.round((targetAngle / 12) * 100), // 0-100 범위
    duration: 45, // 45초간 유지
    mode: "gradual", // 점진적 상승
    target_angle: targetAngle,
    reason: "snore_detected",
    confidence: snoreEvent.confidence
  };
  
  // 30분 후 자동 복원 스케줄링
  scheduleAngleReduction(deviceId, targetAngle, 30 * 60);
  
  return pumpCommand;
};
```

4. **컴포트 관리**
   - 연속 조절 방지: 최소 5분 간격
   - 수면 단계 고려: 깊은 수면 시 조절 강도 감소
   - 사용자 습관 학습: 효과적인 각도 패턴 기억

## 📊 대시보드 화면 매핑

### NEXTION HMI 페이지 구성

**Page 0 (설정/연결 모드)**
- t0: "기기 초기 설정이 필요합니다"
- t1: "BaegaePro 앱을 열어 설정해주세요"
- WiFi 연결 상태 및 프로비저닝 정보 표시

**Page 1 (홈 화면)**
- t0: 온도 ("22°C") 
- t1: 날씨 ("맑음")
- t2: 수면 점수 ("Sleep: 85 points")
- t3: 소음 레벨 ("Noise: 24dB")
- t4: 알람 시간 ("Alarm: 7:20")

### 모바일 앱 화면 데이터 매핑

**메인 대시보드**
- 지난 밤 수면 데이터: `/api/v1/analytics/dashboard?period=1d`
- 수면 점수: 65-100점 범위, 색상 코딩
- 수면 시간: "5시간 30분" 형태로 표시
- 최근 7일 트렌드: 막대 그래프

**월별 리포트**
- 캘린더 뷰: `/api/v1/analytics/monthly/{year}/{month}`
- 각 날짜별 수면 점수 표시
- 수면 부족/과다 인디케이터
- 일관성 점수 및 개선 제안

## 🔧 환경 설정

### 필수 환경 변수
```env
# 데이터베이스
DB_HOST=localhost
DB_PORT=3306
DB_NAME=baegaepro_prod
DB_USER=baegaepro_user
DB_PASS=secure_db_password

# Redis 캐싱
REDIS_URL=redis://localhost:6379
REDIS_PREFIX=bgpro:
DATA_CACHE_TTL=300

# JWT 인증
JWT_SECRET=your_jwt_signing_secret_key_here
JWT_EXPIRES_IN=24h
REFRESH_TOKEN_EXPIRES_IN=30d

# 외부 API
WEATHER_API_KEY=your_openweathermap_api_key
WEATHER_BASE_URL=https://api.openweathermap.org/data/2.5
WEATHER_CACHE_TTL=1800

# FCM 푸시 알림
FCM_SERVER_KEY=your_firebase_cloud_messaging_key
FCM_PROJECT_ID=baegaepro-app

# AWS S3 파일 스토리지
AWS_ACCESS_KEY_ID=your_aws_access_key
AWS_SECRET_ACCESS_KEY=your_aws_secret_key
S3_BUCKET_NAME=baegaepro-audio-storage
S3_REGION=ap-northeast-2

# 서버 설정
NODE_ENV=production
PORT=3000
CORS_ORIGIN=https://app.baegaepro.com
TIMEZONE=Asia/Seoul
LOG_LEVEL=info

# AI 분석 서비스
AI_SERVICE_URL=https://ai.baegaepro.com
AI_API_KEY=your_ai_service_api_key
SNORE_DETECTION_THRESHOLD=0.85
```

## 🚀 배포 및 스케일링

### Docker Compose 구성
```yaml
version: '3.8'
services:
  app:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=production
    depends_on:
      - mysql
      - redis
      - timescaledb
  
  mysql:
    image: mysql:8.0
    environment:
      MYSQL_ROOT_PASSWORD: root_password
      MYSQL_DATABASE: baegaepro_prod
    volumes:
      - mysql_data:/var/lib/mysql
  
  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
  
  timescaledb:
    image: timescale/timescaledb:latest-pg14
    environment:
      POSTGRES_DB: baegaepro_timeseries
    volumes:
      - timeseries_data:/var/lib/postgresql/data

volumes:
  mysql_data:
  redis_data:
  timeseries_data:
```

### 성능 최적화 전략

1. **데이터베이스 최적화**
   - 센서 데이터는 TimescaleDB 사용
   - 인덱싱 전략: compound index 활용
   - 파티셔닝: 월별 테이블 분할

2. **캐싱 전략**
   - 날씨 데이터: 30분 캐시
   - 사용자 세션: 24시간 캐시
   - 기기 상태: 5분 캐시

3. **실시간 처리**
   - WebSocket 연결: 기기별 채널 구독
   - 메시지 큐: Redis Streams 활용
   - 배치 처리: Cron 기반 데이터 집계

## 📋 개발 체크리스트
