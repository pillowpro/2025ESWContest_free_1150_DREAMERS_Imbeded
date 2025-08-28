# BaegaePro 백엔드 서버 구현 가이드

## 개요
BaegaePro IoT 기기의 백엔드 서버 구현을 위한 가이드입니다. ESP32-C6 기기와의 통신을 위한 REST API를 제공합니다.

## 아키텍처

### 기본 구조
```
backend/
├── src/
│   ├── controllers/
│   │   ├── deviceController.js
│   │   ├── authController.js
│   │   └── statusController.js
│   ├── models/
│   │   ├── Device.js
│   │   ├── User.js
│   │   └── DeviceLog.js
│   ├── middleware/
│   │   ├── auth.js
│   │   ├── validation.js
│   │   └── rateLimit.js
│   ├── routes/
│   │   ├── device.js
│   │   ├── auth.js
│   │   └── api.js
│   ├── services/
│   │   ├── deviceService.js
│   │   ├── notificationService.js
│   │   └── analyticsService.js
│   └── utils/
│       ├── logger.js
│       ├── crypto.js
│       └── validator.js
├── config/
│   ├── database.js
│   ├── redis.js
│   └── app.js
├── tests/
├── docs/
└── package.json
```

## API 명세

### 기기 등록 API
**Endpoint:** `POST /api/v1/device/new`

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <user_token>
```

**Request Body:**
```json
{
  "deviceId": "A1B2C3D4"
}
```

**Response:**
```json
{
  "success": true,
  "message": "기기가 성공적으로 등록되었습니다",
  "data": {
    "deviceId": "A1B2C3D4",
    "status": "registered",
    "registeredAt": "2025-08-26T10:30:00Z"
  }
}
```

### 기기 상태 업데이트 API
**Endpoint:** `POST /api/v1/device/{deviceId}/status`

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <device_token>
```

**Request Body:**
```json
{
  "status": "online",
  "temperature": 25.3,
  "humidity": 60.2,
  "battery": 85,
  "timestamp": "2025-08-26T10:30:00Z"
}
```

### 하트비트 API
**Endpoint:** `POST /api/v1/device/{deviceId}/heartbeat`

**Request Headers:**
```
Content-Type: application/json
Authorization: Bearer <device_token>
```

**Request Body:**
```json
{
  "uptime": 3600,
  "freeMemory": 180000,
  "wifiRssi": -45
}
```

### 펌웨어 업데이트 체크 API ⭐ **NEW FOTA**
**Endpoint:** `GET /api/v1/firmware/check/{deviceId}`

**Description:** ESP32 기기가 새로운 펌웨어 업데이트를 확인합니다.

**Request Headers:**
```
Authorization: Bearer <device_token>
```

**Response:**
```json
{
  "success": true,
  "data": {
    "version": "1.1.0",
    "download_url": "https://firmware.baegaepro.com/releases/baegaepro-v1.1.0.bin",
    "sha256": "a1b2c3d4e5f6...",
    "file_size": 1048576,
    "release_notes": "Bug fixes and performance improvements",
    "mandatory": false,
    "released_at": "2025-08-26T10:30:00Z"
  }
}
```

**No Update Available Response:**
```json
{
  "success": true,
  "data": {
    "message": "No update available",
    "current_version": "1.0.0"
  }
}
```

### 홈 데이터 조회 API ⭐ **NEW**
**Endpoint:** `GET /api/v1/home`

**Description:** ESP32 기기가 NEXTION HMI 화면에 표시할 홈 정보를 요청합니다.

**Request Headers:**
```
Authorization: Bearer <device_token>
```

**Response:**
```json
{
  "success": true,
  "data": {
    "currentTime": "2025-01-27 14:30:00",
    "weather": "Clear",
    "temperature": "22",
    "humidity": "65",
    "sleepScore": "85",
    "noiseLevel": "24",
    "alarmTime": "7:20",
    "status": "Normal Operation"
  }
}
```

**응답 필드 설명 (NEXTION 화면 매핑):**
- `temperature`: 현재 온도 (Page 1, t0: "22°C")
- `weather`: 날씨 정보 (Page 1, t1: "Clear")
- `sleepScore`: 수면 점수 (Page 1, t2: "Sleep: 85 points")
- `noiseLevel`: 소음 레벨 (Page 1, t3: "Noise: 24dB")
- `alarmTime`: 알람 시간 (Page 1, t4: "Alarm: 7:20")
- `currentTime`: 현재 시간 (로그용)
- `humidity`: 습도 (확장 용도)
- `status`: 전체 상태 메시지

**Error Response:**
```json
{
  "success": false,
  "error": "Unauthorized",
  "message": "Invalid or expired token"
}
```

## NEXTION HMI 화면 연동 가이드 ⭐ **NEW**

### 화면 구성
ESP32-C6 기기는 UART2(RX:12, TX:13)로 NEXTION HMI 디스플레이와 통신합니다.

**Page 0 (설정/연결 모드):**
- t0: 메인 메시지 ("기기 초기 설정이 필요합니다" 등)
- t1: 서브 메시지 ("앱을 열어 설정해주세요" 등)

**Page 1 (홈 화면):** 
- t0: 온도 ("22°C")
- t1: 날씨 ("Clear")
- t2: 수면 점수 ("Sleep: 85 points")
- t3: 소음 레벨 ("Noise: 24dB")
- t4: 알람 시간 ("Alarm: 7:20")

### 데이터 업데이트 주기
- **초기 표시**: WiFi 연결 성공 후 즉시
- **자동 업데이트**: 10회 하트비트(5분)마다 자동 새로고침
- **수동 업데이트**: NEXTION 터치 이벤트로 가능

### 서버 구현 예시 코드

```javascript
// routes/home.js
const express = require('express');
const router = express.Router();
const moment = require('moment-timezone');
const axios = require('axios');

// 외부 API로 날씨 정보 가져오기
const getWeatherInfo = async () => {
  try {
    const response = await axios.get(
      `https://api.openweathermap.org/data/2.5/weather?q=Seoul&appid=${process.env.WEATHER_API_KEY}&units=metric`
    );
    return {
      weather: response.data.weather[0].description,
      temperature: Math.round(response.data.main.temp).toString(),
      humidity: response.data.main.humidity.toString()
    };
  } catch (error) {
    console.error('Weather API error:', error);
    return {
      weather: '날씨 정보 없음',
      temperature: '--',
      humidity: '--'
    };
  }
};

// GET /api/v1/home
router.get('/home', authMiddleware, async (req, res) => {
  try {
    const deviceId = req.deviceId;
    
    // 기기 상태 확인
    const device = await Device.findOne({ device_id: deviceId });
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }
    
    // 현재 시간 (한국 시간)
    const currentTime = moment().tz('Asia/Seoul').format('YYYY-MM-DD HH:mm:ss');
    
    // 날씨 정보 가져오기
    const weatherData = await getWeatherInfo();
    
    // 기기 상태 메시지 생성
    const statusMessage = generateStatusMessage(device);
    
    const homeData = {
      currentTime,
      weather: weatherData.weather,
      temperature: weatherData.temperature,
      humidity: weatherData.humidity,
      sleepScore: generateSleepScore(device),
      noiseLevel: generateNoiseLevel(),
      alarmTime: getNextAlarmTime(device),
      status: statusMessage
    };
    
    // 로그 기록
    await DeviceLog.create({
      device_id: deviceId,
      event_type: 'home_data_request',
      payload: homeData
    });
    
    res.json({
      success: true,
      data: homeData
    });
    
  } catch (error) {
    console.error('Home data error:', error);
    res.status(500).json({
      success: false,
      error: 'Internal server error',
      message: 'Failed to fetch home data'
    });
  }
});

// 기기 상태 메시지 생성
const generateStatusMessage = (device) => {
  const lastSeen = moment(device.last_seen);
  const now = moment();
  const diffMinutes = now.diff(lastSeen, 'minutes');
  
  if (diffMinutes < 2) {
    return 'Normal Operation';
  } else if (diffMinutes < 10) {
    return `Active ${diffMinutes}min ago`;
  } else {
    return 'Inactive';
  }
};

// 수면 점수 생성 (대체 데이터)
const generateSleepScore = (device) => {
  // 실제 구현에서는 수면 추적 데이터를 사용
  const scores = ['82', '85', '91', '78', '88'];
  return scores[Math.floor(Math.random() * scores.length)];
};

// 소음 레벨 생성
const generateNoiseLevel = () => {
  // 실제 구현에서는 IoT 센서 데이터를 사용
  const levels = ['24', '26', '22', '28', '30'];
  return levels[Math.floor(Math.random() * levels.length)];
};

// 다음 알람 시간 가져오기
const getNextAlarmTime = (device) => {
  // 실제 구현에서는 사용자 설정에서 가져오기
  return device.alarm_time || '7:20';
};

module.exports = router;
```

### FOTA 서버 구현 예시 ⭐ **NEW FOTA**

```javascript
// routes/firmware.js
const express = require('express');
const router = express.Router();
const semver = require('semver');
const crypto = require('crypto');
const fs = require('fs').promises;

// GET /api/v1/firmware/check/:deviceId
router.get('/check/:deviceId', authMiddleware, async (req, res) => {
  try {
    const deviceId = req.params.deviceId;
    
    // 기기 정보 조회
    const device = await Device.findOne({ device_id: deviceId });
    if (!device) {
      return res.status(404).json({
        success: false,
        error: 'Device not found'
      });
    }
    
    const currentVersion = device.firmware_version || '1.0.0';
    
    // 최신 펌웨어 조회
    const latestFirmware = await FirmwareRelease.findOne({
      device_type: 'baegaepro',
      status: 'stable',
      released_at: { $lte: new Date() }
    }).sort({ released_at: -1 });
    
    if (!latestFirmware) {
      return res.json({
        success: true,
        data: {
          message: 'No firmware available',
          current_version: currentVersion
        }
      });
    }
    
    // 버전 비교 (semver 사용)
    const updateAvailable = semver.gt(latestFirmware.version, currentVersion);
    
    if (!updateAvailable) {
      return res.json({
        success: true,
        data: {
          message: 'No update available',
          current_version: currentVersion,
          latest_version: latestFirmware.version
        }
      });
    }
    
    // 업데이트 가능한 경우
    const updateData = {
      version: latestFirmware.version,
      download_url: latestFirmware.file_url,
      sha256: latestFirmware.sha256_hash,
      file_size: latestFirmware.file_size,
      release_notes: latestFirmware.release_notes || 'Performance improvements and bug fixes',
      mandatory: latestFirmware.mandatory,
      released_at: latestFirmware.released_at
    };
    
    // FOTA 체크 로그 기록
    await DeviceLog.create({
      device_id: deviceId,
      event_type: 'fota_check',
      payload: {
        current_version: currentVersion,
        available_version: latestFirmware.version,
        update_available: true
      }
    });
    
    res.json({
      success: true,
      data: updateData
    });
    
  } catch (error) {
    console.error('FOTA check error:', error);
    res.status(500).json({
      success: false,
      error: 'Internal server error',
      message: 'Failed to check firmware updates'
    });
  }
});

// POST /api/v1/firmware/update/start
router.post('/update/start', authMiddleware, async (req, res) => {
  try {
    const deviceId = req.deviceId;
    const { target_version } = req.body;
    
    // 현재 업데이트 상태 확인
    const ongoingUpdate = await FirmwareUpdate.findOne({
      device_id: deviceId,
      status: { $in: ['started', 'downloading', 'verifying', 'installing'] }
    });
    
    if (ongoingUpdate) {
      return res.status(409).json({
        success: false,
        error: 'Update already in progress'
      });
    }
    
    const device = await Device.findOne({ device_id: deviceId });
    const currentVersion = device.firmware_version || '1.0.0';
    
    // 업데이트 기록 생성
    const updateRecord = await FirmwareUpdate.create({
      device_id: deviceId,
      from_version: currentVersion,
      to_version: target_version,
      status: 'started'
    });
    
    // 디바이스 상태 업데이트
    await Device.updateOne(
      { device_id: deviceId },
      { status: 'updating' }
    );
    
    res.json({
      success: true,
      message: 'Firmware update started',
      update_id: updateRecord.id
    });
    
    // 비동기로 모바일 앱에 알림 전송
    setImmediate(() => {
      sendDeviceAlert(
        deviceId,
        'fota_started',
        `기기 펌웨어 업데이트가 시작되었습니다. (${currentVersion} → ${target_version})`
      );
    });
    
  } catch (error) {
    console.error('FOTA start error:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to start firmware update'
    });
  }
});

// POST /api/v1/firmware/update/progress
router.post('/update/progress', authMiddleware, async (req, res) => {
  try {
    const deviceId = req.deviceId;
    const { status, progress_percent, error_message } = req.body;
    
    const updateRecord = await FirmwareUpdate.findOneAndUpdate(
      {
        device_id: deviceId,
        status: { $in: ['started', 'downloading', 'verifying', 'installing'] }
      },
      {
        status,
        progress_percent: progress_percent || 0,
        error_message: error_message || null,
        ...(status === 'completed' || status === 'failed' ? { completed_at: new Date() } : {})
      },
      { new: true }
    );
    
    if (!updateRecord) {
      return res.status(404).json({
        success: false,
        error: 'Update record not found'
      });
    }
    
    // 업데이트 완료 시 기기 정보 업데이트
    if (status === 'completed') {
      await Device.updateOne(
        { device_id: deviceId },
        {
          firmware_version: updateRecord.to_version,
          status: 'online'
        }
      );
      
      // 성공 알림
      await sendDeviceAlert(
        deviceId,
        'fota_completed',
        `펌웨어 업데이트가 성공적으로 완료되었습니다. (${updateRecord.to_version})`
      );
    } else if (status === 'failed') {
      await Device.updateOne(
        { device_id: deviceId },
        { status: 'error' }
      );
      
      // 실패 알림
      await sendDeviceAlert(
        deviceId,
        'fota_failed',
        `펌웨어 업데이트가 실패했습니다: ${error_message}`
      );
    }
    
    res.json({
      success: true,
      message: 'Progress updated'
    });
    
  } catch (error) {
    console.error('FOTA progress error:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to update progress'
    });
  }
});

// GET /api/v1/firmware/releases - 관리자용
router.get('/releases', adminAuthMiddleware, async (req, res) => {
  try {
    const releases = await FirmwareRelease.find()
      .sort({ created_at: -1 })
      .limit(20);
    
    res.json({
      success: true,
      data: releases
    });
    
  } catch (error) {
    res.status(500).json({
      success: false,
      error: 'Failed to fetch releases'
    });
  }
});

// POST /api/v1/firmware/releases - 새 펌웨어 릴리스 등록
router.post('/releases', adminAuthMiddleware, upload.single('firmware'), async (req, res) => {
  try {
    const { version, release_notes, mandatory, device_type } = req.body;
    const file = req.file;
    
    if (!file) {
      return res.status(400).json({
        success: false,
        error: 'Firmware file required'
      });
    }
    
    // 파일 SHA256 해시 계산
    const fileBuffer = await fs.readFile(file.path);
    const hash = crypto.createHash('sha256').update(fileBuffer).digest('hex');
    
    // S3 또는 파일 서버에 업로드 (예시)
    const fileUrl = await uploadFirmwareFile(file, version);
    
    const release = await FirmwareRelease.create({
      version,
      file_url: fileUrl,
      file_size: file.size,
      sha256_hash: hash,
      device_type: device_type || 'baegaepro',
      release_notes,
      mandatory: mandatory === 'true',
      status: 'stable',
      released_at: new Date()
    });
    
    res.json({
      success: true,
      message: 'Firmware release created',
      data: release
    });
    
  } catch (error) {
    console.error('Release creation error:', error);
    res.status(500).json({
      success: false,
      error: 'Failed to create release'
    });
  }
});

module.exports = router;
```

### FOTA 파일 관리 예시

```javascript
// services/firmwareService.js
const AWS = require('aws-sdk');
const s3 = new AWS.S3();

// S3에 펌웨어 파일 업로드
const uploadFirmwareFile = async (file, version) => {
  const key = `firmware/baegaepro/v${version}/baegaepro-${version}.bin`;
  
  const uploadParams = {
    Bucket: process.env.S3_FIRMWARE_BUCKET,
    Key: key,
    Body: fs.createReadStream(file.path),
    ContentType: 'application/octet-stream',
    ACL: 'private'
  };
  
  const result = await s3.upload(uploadParams).promise();
  
  // 임시 파일 삭제
  await fs.unlink(file.path);
  
  // 서명된 URL 반환 (다운로드 시 인증 필요)
  return s3.getSignedUrl('getObject', {
    Bucket: process.env.S3_FIRMWARE_BUCKET,
    Key: key,
    Expires: 3600 // 1시간 유효
  });
};

// 펌웨어 무결성 검증
const verifyFirmwareIntegrity = async (fileBuffer, expectedHash) => {
  const hash = crypto.createHash('sha256').update(fileBuffer).digest('hex');
  return hash === expectedHash;
};

// 펌웨어 보안 스캔 (예시)
const scanFirmwareSecurity = async (filePath) => {
  // 실제로는 바이러스 스캔, 서명 검증 등
  // 예: ClamAV, VirusTotal API 연동
  return { safe: true, threats: [] };
};

module.exports = {
  uploadFirmwareFile,
  verifyFirmwareIntegrity,
  scanFirmwareSecurity
};
```

### 환경 변수 설정
```env
# .env 파일
WEATHER_API_KEY=your_openweathermap_api_key
TIMEZONE=Asia/Seoul
DEFAULT_WEATHER_CITY=Seoul
```

### 캐싱 전략 (성능 최적화)
```javascript
// Redis로 날씨 데이터 캐싱 (5분마다 업데이트)
const getCachedWeatherInfo = async () => {
  const cacheKey = 'weather:seoul';
  const cached = await redis.get(cacheKey);
  
  if (cached) {
    return JSON.parse(cached);
  }
  
  const weatherData = await getWeatherInfo();
  await redis.setex(cacheKey, 300, JSON.stringify(weatherData)); // 5분 캐시
  return weatherData;
};  
```

### 패키지 설치 및 라이브러리
```bash
npm install moment-timezone axios redis
npm install --save-dev jest supertest
```

### ESP32 요청 패턴 및 서버 최적화

**하트비트 요청 네트워크 최적화:**
```javascript
// 네트워크 요청 최소화를 위한 체크
router.post('/device/:deviceId/heartbeat', authMiddleware, async (req, res) => {
  // 빠른 응답으로 ESP32 타임아웃 방지
  res.json({ success: true, message: 'heartbeat received' });
  
  // 비동기 처리
  setImmediate(async () => {
    try {
      await updateDeviceLastSeen(req.deviceId);
      await logHeartbeat(req.deviceId, req.body);
    } catch (error) {
      console.error('Heartbeat processing error:', error);
    }
  });
});
```

**에러 처리 및 폴백:**
```javascript
// ESP32가 서버 오류 시 대비할 수 있도록 대체 데이터 제공
const getHomeDataWithFallback = async (deviceId) => {
  try {
    const homeData = await getFullHomeData(deviceId);
    return homeData;
  } catch (error) {
    console.error(`Home data error for ${deviceId}:`, error);
    
    // 대체 데이터 반환
    return {
      currentTime: moment().tz('Asia/Seoul').format('YYYY-MM-DD HH:mm:ss'),
      weather: '오프라인 모드',
      temperature: '--',
      humidity: '--',
      status: '네트워크 오류'
    };
  }
};
```

### 실시간 알림 시스템
```javascript
// FCM 또는 WebSocket을 이용한 모바일 앱 알림
const sendDeviceAlert = async (deviceId, alertType, message) => {
  try {
    const device = await Device.findOne({ device_id: deviceId });
    if (!device) return;
    
    // 모바일 앱에 알림 전송
    await fcm.send({
      token: device.user_fcm_token,
      notification: {
        title: 'BaegaePro 알림',
        body: message
      },
      data: {
        deviceId,
        alertType,
        timestamp: new Date().toISOString()
      }
    });
    
  } catch (error) {
    console.error('Alert sending failed:', error);
  }
};

// 기기 비활성 상태 모니터링
const checkInactiveDevices = async () => {
  const threshold = moment().subtract(10, 'minutes');
  const inactiveDevices = await Device.find({
    last_seen: { $lt: threshold },
    status: { $ne: 'offline' }
  });
  
  for (const device of inactiveDevices) {
    await Device.updateOne(
      { device_id: device.device_id },
      { status: 'offline' }
    );
    
    await sendDeviceAlert(
      device.device_id,
      'offline',
      `기기 ${device.device_name}이 비활성 상태입니다.`
    );
  }
};

// 5분마다 비활성 기기 체크
setInterval(checkInactiveDevices, 5 * 60 * 1000);
```

## 데이터베이스 스키마

### devices 테이블
```sql
CREATE TABLE devices (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) UNIQUE NOT NULL,
  user_id BIGINT NOT NULL,
  device_name VARCHAR(100),
  device_type VARCHAR(50) DEFAULT 'baegaepro',
  status ENUM('registered', 'online', 'offline', 'error') DEFAULT 'registered',
  last_seen TIMESTAMP,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_device_id (device_id),
  INDEX idx_user_id (user_id),
  INDEX idx_status (status),
  FOREIGN KEY (user_id) REFERENCES users(id)
);
```

### device_logs 테이블
```sql
CREATE TABLE device_logs (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) NOT NULL,
  event_type VARCHAR(50) NOT NULL,
  payload JSON,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_device_id (device_id),
  INDEX idx_event_type (event_type),
  INDEX idx_created_at (created_at),
  FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

### device_tokens 테이블
```sql
CREATE TABLE device_tokens (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) NOT NULL,
  token_hash VARCHAR(255) NOT NULL,
  expires_at TIMESTAMP,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY unique_device_token (device_id),
  INDEX idx_token_hash (token_hash),
  FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

### firmware_releases 테이블 ⭐ **NEW FOTA**
```sql
CREATE TABLE firmware_releases (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  version VARCHAR(32) NOT NULL UNIQUE,
  file_url VARCHAR(512) NOT NULL,
  file_size BIGINT NOT NULL,
  sha256_hash VARCHAR(64) NOT NULL,
  device_type VARCHAR(50) DEFAULT 'baegaepro',
  release_notes TEXT,
  mandatory BOOLEAN DEFAULT FALSE,
  min_version VARCHAR(32), -- 최소 요구 버전
  max_version VARCHAR(32), -- 최대 지원 버전 
  status ENUM('draft', 'beta', 'stable', 'deprecated') DEFAULT 'stable',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  released_at TIMESTAMP,
  INDEX idx_version (version),
  INDEX idx_device_type (device_type),
  INDEX idx_status (status),
  INDEX idx_released_at (released_at)
);
```

### firmware_updates 테이블 ⭐ **NEW FOTA**
```sql
CREATE TABLE firmware_updates (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  device_id VARCHAR(16) NOT NULL,
  from_version VARCHAR(32),
  to_version VARCHAR(32) NOT NULL,
  status ENUM('started', 'downloading', 'verifying', 'installing', 'completed', 'failed', 'rolled_back') DEFAULT 'started',
  progress_percent TINYINT DEFAULT 0,
  error_message TEXT,
  started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  completed_at TIMESTAMP NULL,
  INDEX idx_device_id (device_id),
  INDEX idx_status (status),
  INDEX idx_started_at (started_at),
  FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

## 인증 시스템

### 기기 토큰 생성
1. 사용자가 모바일 앱에서 기기 등록 시 임시 토큰 생성
2. ESP32가 프로비저닝 과정에서 이 토큰을 받아 서버에 전송
3. 서버는 토큰 검증 후 기기 전용 장기 토큰 발급

### 토큰 검증 미들웨어
```javascript
const authMiddleware = (req, res, next) => {
  const token = req.headers.authorization?.split(' ')[1];
  
  if (!token) {
    return res.status(401).json({ error: 'Token required' });
  }
  
  // JWT 또는 해시 기반 토큰 검증
  const decoded = verifyToken(token);
  if (!decoded) {
    return res.status(401).json({ error: 'Invalid token' });
  }
  
  req.deviceId = decoded.deviceId;
  req.userId = decoded.userId;
  next();
};
```

## 실시간 통신

### WebSocket 연결
기기 상태 실시간 모니터링을 위한 WebSocket 서버 구현

```javascript
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', (ws, req) => {
  ws.on('message', (message) => {
    const data = JSON.parse(message);
    
    // 기기별 채널 구독
    if (data.type === 'subscribe') {
      ws.deviceId = data.deviceId;
      ws.userId = data.userId;
    }
  });
});

// 기기 상태 변경시 해당 사용자에게 브로드캐스트
const broadcastDeviceStatus = (deviceId, status) => {
  wss.clients.forEach(client => {
    if (client.deviceId === deviceId && client.readyState === WebSocket.OPEN) {
      client.send(JSON.stringify({
        type: 'device_status',
        deviceId,
        status,
        timestamp: new Date().toISOString()
      }));
    }
  });
};
```

## 보안 고려사항

### 1. Rate Limiting
```javascript
const rateLimit = require('express-rate-limit');

const deviceApiLimiter = rateLimit({
  windowMs: 1 * 60 * 1000, // 1분
  max: 30, // 최대 30회 요청
  message: 'Too many requests from this device'
});
```

### 2. 입력값 검증
```javascript
const { body, param, validationResult } = require('express-validator');

const deviceIdValidation = param('deviceId')
  .isAlphanumeric()
  .isLength({ min: 8, max: 8 })
  .withMessage('Device ID must be 8 alphanumeric characters');
```

### 3. HTTPS 강제
```javascript
app.use((req, res, next) => {
  if (req.header('x-forwarded-proto') !== 'https') {
    res.redirect(`https://${req.header('host')}${req.url}`);
  } else {
    next();
  }
});
```

## 모니터링 및 로깅

### 로깅 시스템
```javascript
const winston = require('winston');

const logger = winston.createLogger({
  level: 'info',
  format: winston.format.combine(
    winston.format.timestamp(),
    winston.format.json()
  ),
  transports: [
    new winston.transports.File({ filename: 'error.log', level: 'error' }),
    new winston.transports.File({ filename: 'combined.log' }),
    new winston.transports.Console()
  ]
});
```

### 메트릭 수집
- 기기 연결 수
- API 응답 시간
- 에러율
- 하트비트 간격

## 배포 환경

### Docker 구성
```dockerfile
FROM node:18-alpine

WORKDIR /app
COPY package*.json ./
RUN npm ci --only=production

COPY . .
EXPOSE 3000

CMD ["node", "src/server.js"]
```

### 환경 변수
```env
NODE_ENV=production
PORT=3000
DB_HOST=localhost
DB_USER=baegaepro
DB_PASS=secure_password
DB_NAME=baegaepro_db
REDIS_URL=redis://localhost:6379
JWT_SECRET=your_jwt_secret
CORS_ORIGIN=https://app.baegaepro.com

# ⭐ NEW: 홈 기능을 위한 추가 환경 변수
WEATHER_API_KEY=your_openweathermap_api_key
WEATHER_CITY=Seoul
TIMEZONE=Asia/Seoul
FCM_SERVER_KEY=your_fcm_server_key
DATA_CACHE_TTL=300
```

## 개발 시작하기

### 1. 프로젝트 초기화
```bash
mkdir baegaepro-backend
cd baegaepro-backend
npm init -y
npm install express mongoose redis socket.io jsonwebtoken bcryptjs
npm install moment-timezone axios helmet cors
npm install -D nodemon jest supertest
```

### 2. 기본 서버 구조
```javascript
const express = require('express');
const cors = require('cors');
const helmet = require('helmet');

const app = express();

app.use(helmet());
app.use(cors());
app.use(express.json());

// Routes
app.use('/api/v1/device', require('./routes/device'));
app.use('/api/v1/auth', require('./routes/auth'));

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
```

### 3. 테스트 코드 작성
```javascript
const request = require('supertest');
const app = require('../src/app');

describe('Device API', () => {
  test('POST /api/v1/device/new', async () => {
    const response = await request(app)
      .post('/api/v1/device/new')
      .send({ deviceId: 'A1B2C3D4' })
      .set('Authorization', 'Bearer valid_token');
      
    expect(response.status).toBe(200);
    expect(response.body.success).toBe(true);
  });
  
  // ⭐ NEW: 홈 데이터 API 테스트
  test('GET /api/v1/home', async () => {
    const response = await request(app)
      .get('/api/v1/home')
      .set('Authorization', 'Bearer device_token');
      
    expect(response.status).toBe(200);
    expect(response.body.success).toBe(true);
    expect(response.body.data).toHaveProperty('currentTime');
    expect(response.body.data).toHaveProperty('weather');
    expect(response.body.data).toHaveProperty('temperature');
    expect(response.body.data).toHaveProperty('status');
  });
  
  test('POST /api/v1/device/:deviceId/heartbeat', async () => {
    const response = await request(app)
      .post('/api/v1/device/A1B2C3D4/heartbeat')
      .send({ uptime: 3600, freeMemory: 180000 })
      .set('Authorization', 'Bearer device_token');
      
    expect(response.status).toBe(200);
    expect(response.body.success).toBe(true);
  });
});

// 모크 테스트
const mockWeatherAPI = () => {
  jest.mock('axios', () => ({
    get: jest.fn().mockResolvedValue({
      data: {
        weather: [{ description: '맑음' }],
        main: { temp: 22, humidity: 65 }
      }
    })
  }));
};
```

## 성능 최적화

### 1. 데이터베이스 인덱싱
- device_id에 고유 인덱스
- user_id에 인덱스
- created_at에 인덱스 (로그 조회용)

### 2. 캐싱 전략
```javascript
const redis = require('redis');
const client = redis.createClient();

const cacheDeviceStatus = async (deviceId, status) => {
  await client.setex(`device:${deviceId}:status`, 300, JSON.stringify(status));
};
```

### 3. Connection Pooling
```javascript
const mysql = require('mysql2/promise');

const pool = mysql.createPool({
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PASS,
  database: process.env.DB_NAME,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});
```

이 가이드를 참고하여 확장 가능하고 안정적인 IoT 백엔드 시스템을 구축할 수 있습니다.

## 구현 체크리스트 ⭐ **NEW**

### 필수 API 구현
- [ ] `POST /api/v1/device/new` - 기기 등록
- [ ] `POST /api/v1/device/{deviceId}/heartbeat` - 하트비트
- [ ] `GET /api/v1/home` - 홈 데이터 조회 (NEXTION 용)
- [ ] `POST /api/v1/device/{deviceId}/status` - 상태 업데이트
- [ ] `GET /api/v1/firmware/check/{deviceId}` - FOTA 체크 ⭐ **NEW**
- [ ] `POST /api/v1/firmware/update/start` - FOTA 시작 ⭐ **NEW**
- [ ] `POST /api/v1/firmware/update/progress` - FOTA 진행 상황 ⭐ **NEW**

### 외부 API 연동
- [ ] OpenWeatherMap API 연동
- [ ] 시간대 처리 (moment-timezone)
- [ ] FCM 푸시 알림 (선택사항)

### 데이터베이스 설정
- [ ] devices 테이블 생성
- [ ] device_logs 테이블 생성
- [ ] device_tokens 테이블 생성
- [ ] firmware_releases 테이블 생성 ⭐ **NEW FOTA**
- [ ] firmware_updates 테이블 생성 ⭐ **NEW FOTA**
- [ ] 인덱스 설정 (device_id, user_id, created_at, version, status)

### 캐싱 및 성능
- [ ] Redis 캐싱 설정
- [ ] 날씨 데이터 5분 캐싱
- [ ] 하트비트 비동기 처리
- [ ] Connection Pooling

### 보안 및 인증
- [ ] JWT 토큰 검증
- [ ] Rate Limiting
- [ ] HTTPS 강제
- [ ] 입력값 검증

### 모니터링 및 로깅
- [ ] Winston 로깅 설정
- [ ] 기기 상태 모니터링
- [ ] 비활성 기기 알림
- [ ] API 응답 시간 측정

### 테스트
- [ ] 단위 테스트 (Jest)
- [ ] API 통합 테스트
- [ ] 모킹 테스트 (외부 API)

### 배포
- [ ] Docker 컨테이너화
- [ ] 환경 변수 설정
- [ ] PM2 프로세스 매니저
- [ ] 로드 밸런싱 (선택사항)

### FOTA 파일 관리 ⭐ **NEW**
- [ ] AWS S3 또는 파일 서버 설정
- [ ] 펌웨어 파일 업로드 API
- [ ] SHA256 해시 검증
- [ ] 서명된 URL 생성 (보안)
- [ ] 파일 버전 관리 (semver)
- [ ] 파일 무결성 검증

### FOTA 보안 ⭐ **NEW**
- [ ] 펌웨어 디지털 서명
- [ ] 다운로드 URL 인증
- [ ] 펌웨어 보안 스캔
- [ ] 롱백 메커니즘
- [ ] 비상 대응 계획

### FOTA 모니터링 ⭐ **NEW**
- [ ] 업데이트 진행 상황 대시보드
- [ ] 실패율 모니터링
- [ ] 네트워크 사용량 추적
- [ ] 디바이스별 업데이트 로그
- [ ] 알림 시스템 (FCM/웹소켓)

### ESP32 펌웨어 연동 확인
- [ ] NEXTION Page 0 설정 모드 구성 (t0: 메인 메시지, t1: 서브 메시지)
- [ ] NEXTION Page 1 홈 모드 구성 (t0: 온도, t1: 날씨, t2: 수면점수, t3: 소음, t4: 알람)
- [ ] 5분 주기 홈 데이터 업데이트
- [ ] 1시간 주기 FOTA 체크 ⭐ **NEW**
- [ ] OTA 파티션 관리 ⭐ **NEW**
- [ ] 롤백 기능 구현 ⭐ **NEW**
- [ ] 네트워크 오류 시 폴백 처리
- [ ] NEXTION 터치 이벤트 처리 (설정, WiFi 목록, 새로고침, 초기화)
