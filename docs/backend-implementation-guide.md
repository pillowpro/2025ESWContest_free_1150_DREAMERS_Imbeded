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
```

## 개발 시작하기

### 1. 프로젝트 초기화
```bash
mkdir baegaepro-backend
cd baegaepro-backend
npm init -y
npm install express mongoose redis socket.io jsonwebtoken bcryptjs
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
});
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
