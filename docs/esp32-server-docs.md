# ESP32 BaegaePro Server API 명세서

## 개요

BaegaePro ESP32-C6 기기가 프로비저닝 모드에서 제공하는 REST API 명세서입니다. 모든 API는 JSON 형식으로 응답하며, CORS를 지원합니다.

## 기본 정보

- **Base URL**: `http://192.168.4.1` (AP 모드에서)
- **Content-Type**: `application/json`
- **CORS**: 모든 도메인 허용

## 인증

현재 버전에서는 별도의 인증을 요구하지 않습니다.

---

## API 엔드포인트

### 1. 웹 UI 제공

#### GET /

프로비저닝을 위한 웹 인터페이스를 제공합니다.

**Response:**
- Content-Type: `text/html`
- HTML 페이지 반환

---

### 2. WiFi 설정

#### POST /api/wifi-config

WiFi 연결 정보와 인증 토큰을 설정합니다.

**Request Body:**
```json
{
  "ssid": "MyWiFiNetwork",
  "password": "wifi_password",
  "token": "auth_token_from_backend",
  "provisioning_code": "A1B2C3D4"
}
```

**Parameters:**
- `ssid` (string, required): WiFi 네트워크 이름
- `password` (string, optional): WiFi 비밀번호 (오픈 네트워크인 경우 생략 가능)
- `token` (string, required): 백엔드에서 발급된 인증 토큰
- `provisioning_code` (string, required): 기기 프로비저닝을 위한 코드

**Response:**
```json
{
  "success": true,
  "message": "Configuration received, connecting to WiFi..."
}
```

**Error Response:**
```json
{
  "success": false,
  "message": "Missing required fields: ssid, token, provisioning_code"
}
```

**Status Codes:**
- `200 OK`: 설정이 성공적으로 수신됨
- `400 Bad Request`: 잘못된 요청 형식 또는 필수 필드 누락

---

### 3. 기기 상태 조회

#### GET /api/status

현재 기기의 상태 정보를 조회합니다.

**Response:**
```json
{
  "status": "provisioning",
  "device_id": "ESP32C6-A1B2",
  "firmware_version": "1.0.0",
  "uptime_ms": 123456
}
```

**Parameters:**
- `status` (string): 현재 기기 상태
  - `"provisioning"`: 프로비저닝 모드
  - `"connected"`: WiFi 연결됨
  - `"connecting"`: WiFi 연결 중
- `device_id` (string): MAC 기반 기기 고유 ID
- `firmware_version` (string): 펌웨어 버전
- `uptime_ms` (number): 부팅 후 경과 시간 (밀리초)

---

### 4. 상세 기기 정보

#### GET /api/device-info

기기의 상세한 하드웨어 및 소프트웨어 정보를 조회합니다.

**Response:**
```json
{
  "device_id": "ESP32C6-A1B2",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "chip_model": "ESP32-C6",
  "firmware_version": "1.0.0",
  "heap_free": 256000,
  "uptime_ms": 123456
}
```

**Parameters:**
- `device_id` (string): MAC 기반 기기 고유 ID
- `mac_address` (string): MAC 주소 (형식: XX:XX:XX:XX:XX:XX)
- `chip_model` (string): 칩 모델명
- `firmware_version` (string): 펌웨어 버전
- `heap_free` (number): 사용 가능한 힙 메모리 (바이트)
- `uptime_ms` (number): 부팅 후 경과 시간 (밀리초)

---

### 5. 기기 리셋

#### POST /api/reset

기기를 재시작합니다.

**Response:**
```json
{
  "success": true,
  "message": "Device will reset in 3 seconds"
}
```

**Note:** 응답 후 3초 뒤 기기가 재시작됩니다.

---

## CORS 지원

모든 API 엔드포인트는 CORS preflight 요청을 지원합니다.

#### OPTIONS /api/*

**Response Headers:**
```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 86400
```

---

## 에러 처리

### 일반적인 에러 응답 형식

```json
{
  "success": false,
  "message": "Error description"
}
```

### HTTP 상태 코드

- `200 OK`: 요청 성공
- `400 Bad Request`: 잘못된 요청
- `404 Not Found`: 엔드포인트 없음
- `405 Method Not Allowed`: 허용되지 않는 HTTP 메서드
- `408 Request Timeout`: 요청 시간 초과
- `500 Internal Server Error`: 서버 내부 오류

---

## 사용 예제

### JavaScript (Fetch API)

```javascript
// WiFi 설정
async function configureWiFi() {
    const response = await fetch('http://192.168.4.1/api/wifi-config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            ssid: 'MyWiFiNetwork',
            password: 'mypassword',
            token: 'auth_token_here',
            provisioning_code: 'A1B2C3D4'
        })
    });
    
    const result = await response.json();
    console.log(result);
}

// 기기 상태 확인
async function checkStatus() {
    const response = await fetch('http://192.168.4.1/api/status');
    const status = await response.json();
    console.log(status);
}

// 기기 리셋
async function resetDevice() {
    const response = await fetch('http://192.168.4.1/api/reset', {
        method: 'POST'
    });
    const result = await response.json();
    console.log(result);
}
```

### cURL

```bash
# WiFi 설정
curl -X POST http://192.168.4.1/api/wifi-config \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyWiFiNetwork","password":"mypassword","token":"auth_token_here","provisioning_code":"A1B2C3D4"}'

# 기기 상태 확인
curl http://192.168.4.1/api/status

# 기기 정보 확인
curl http://192.168.4.1/api/device-info

# 기기 리셋
curl -X POST http://192.168.4.1/api/reset
```

---

## BOOT 버튼 기능

### 물리적 버튼 동작

- **짧은 누름 (< 5초)**: 로그 출력만 (개발용)
- **긴 누름 (≥ 5초)**: 팩토리 리셋 수행
  - 모든 설정 초기화
  - WiFi 자격증명 삭제
  - 인증 토큰 삭제
  - NEXTION에 "Factory reset! Restarting..." 메시지 표시
  - 2초 후 자동 재시작

### 사용 시나리오

1. **정상 작동 중**: BOOT 버튼 5초 이상 누름 → 팩토리 리셋
2. **프로비저닝 모드**: BOOT 버튼으로 재시작하여 처음부터 다시 설정 가능

---

## 주의사항

1. **네트워크**: 프로비저닝 모드에서만 이 API들을 사용할 수 있습니다.
2. **시간 제한**: WiFi 연결 후 일정 시간이 지나면 AP 모드가 종료됩니다.
3. **동시 연결**: AP 모드에서는 최대 4개의 클라이언트만 동시 연결 가능합니다.
4. **보안**: 프로덕션 환경에서는 HTTPS 사용을 권장합니다.

---

## 버전 정보

- **API Version**: v1.0
- **문서 업데이트**: 2025-08-27
- **호환 펌웨어**: BaegaePro Firmware v1.0.0+