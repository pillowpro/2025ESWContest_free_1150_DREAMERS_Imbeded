# BaegaePro Firmware

ESP32-C6 기반 IoT 디바이스용 WiFi 프로비저닝 및 클라우드 연동 펌웨어

## 프로젝트 구조

```
BaegaePro-firmware/
├── main/                           # 메인 애플리케이션
│   ├── main.c                     # 메인 소스코드
│   └── CMakeLists.txt             # 메인 빌드 설정
├── components/                     # 커스텀 컴포넌트들
│   ├── wifi_manager/              # WiFi 관리 컴포넌트
│   │   ├── include/
│   │   │   └── wifi_manager.h
│   │   ├── src/
│   │   │   ├── wifi_manager.c
│   │   │   ├── wifi_ap.c
│   │   │   └── wifi_sta.c
│   │   └── CMakeLists.txt
│   ├── device_config/             # 기기 설정 관리
│   │   ├── include/
│   │   │   └── device_config.h
│   │   ├── src/
│   │   │   └── device_config.c
│   │   └── CMakeLists.txt
│   ├── web_server/                # 프로비저닝 웹서버
│   │   ├── include/
│   │   │   └── web_server.h
│   │   ├── src/
│   │   │   └── web_server.c
│   │   └── CMakeLists.txt
│   ├── api_client/                # 백엔드 API 클라이언트
│   │   ├── include/
│   │   │   └── api_client.h
│   │   ├── src/
│   │   │   └── api_client.c
│   │   └── CMakeLists.txt
│   └── utils/                     # 유틸리티 함수들
│       ├── include/
│       │   └── utils.h
│       ├── src/
│       │   └── utils.c
│       └── CMakeLists.txt
├── include/                       # 글로벌 헤더 파일
├── docs/                          # 문서
│   └── backend-implementation-guide.md
├── test/                          # 테스트 코드
├── build/                         # 빌드 결과물 (자동생성)
├── CMakeLists.txt                 # 루트 빌드 설정
├── partitions.csv                 # 파티션 테이블
├── sdkconfig                      # ESP-IDF 설정
└── README.md                      # 이 파일
```

## 주요 기능

### 1. WiFi 프로비저닝
- ESP32가 AP 모드로 동작
- SSID: `BaeGaePRO-{4자리 MAC}`
- 비밀번호: `{4자리 MAC}PSWR`
- 웹 인터페이스를 통한 WiFi 설정

### 2. 기기 등록
- MAC 주소 기반 고유 기기 ID 생성
- 백엔드 서버에 자동 등록
- 토큰 기반 인증

### 3. 상태 모니터링
- 주기적 하트비트 전송
- 실시간 상태 업데이트
- 연결 상태 관리

## 빌드 및 플래시

### 개발 환경 설정
1. ESP-IDF v5.0 이상 설치
2. ESP32-C6 개발 보드 연결

### 빌드 명령어
```bash
# 프로젝트 디렉토리로 이동
cd BaegaePro-firmware

# 빌드
idf.py build

# 플래시 및 모니터링
idf.py -p [PORT] flash monitor
```

### 설정 변경
```bash
# menuconfig를 통한 설정 변경
idf.py menuconfig
```

## 컴포넌트 설명

### WiFi Manager (`components/wifi_manager`)
WiFi 연결 관리를 담당하는 컴포넌트
- AP 모드 운영
- STA 모드 연결
- 연결 상태 모니터링
- 이벤트 콜백 시스템

### Device Config (`components/device_config`)
기기 설정 및 저장 관리
- NVS 기반 설정 저장
- WiFi 자격증명 관리
- 프로비저닝 상태 관리
- 기기 ID 생성 및 저장

### Web Server (`components/web_server`)
프로비저닝용 HTTP 서버
- RESTful API 제공
- WiFi 설정 수신
- 상태 페이지 제공
- CORS 지원

### API Client (`components/api_client`)
백엔드 서버와의 통신 담당
- HTTPS 통신
- JSON 데이터 처리
- 인증 토큰 관리
- 에러 처리

### Utils (`components/utils`)
공통 유틸리티 함수들
- MAC 주소 처리
- 문자열 검증
- URL 인코딩
- JSON 파싱 보조

## API 엔드포인트

### 프로비저닝 API (ESP32 웹서버)
- `GET /` - 프로비저닝 웹페이지
- `POST /wifi-config` - WiFi 설정 수신
- `GET /status` - 기기 상태 조회

### 백엔드 API
- `POST /api/v1/device/new` - 기기 등록
- `POST /api/v1/device/{id}/heartbeat` - 하트비트
- `POST /api/v1/device/{id}/status` - 상태 업데이트

## 개발 가이드

### 새로운 컴포넌트 추가
1. `components/` 아래에 새 디렉토리 생성
2. `include/`, `src/` 디렉토리 생성
3. `CMakeLists.txt` 파일 작성
4. 헤더 파일과 소스 파일 작성
5. 메인 `CMakeLists.txt`에 의존성 추가

### 컴포넌트 간 통신
- 이벤트 시스템 사용 권장
- 콜백 함수를 통한 비동기 통신
- 상태 공유는 최소화

### 메모리 관리
- 동적 할당 최소화
- 스택 오버플로우 주의
- RTOS 태스크 스택 크기 조정

## 설정 변수

### WiFi 설정
```c
#define MAX_RETRY          5        // WiFi 연결 재시도 횟수
#define WIFI_TIMEOUT_MS    10000    // WiFi 연결 타임아웃
```

### API 설정
```c
#define API_BASE_URL "https://baegaepro.ncloud.sbs/api/v1"
#define MAX_HTTP_RESPONSE_BUFFER 1024
```

### 디바이스 설정
```c
#define DEVICE_ID_LENGTH   8        // 기기 ID 길이
#define HEARTBEAT_INTERVAL 30000    // 하트비트 간격 (ms)
```

## 트러블슈팅

### 컴파일 에러
1. ESP-IDF 버전 확인
2. 의존성 컴포넌트 확인
3. 헤더 파일 경로 확인

### WiFi 연결 실패
1. 자격증명 확인
2. 신호 강도 확인
3. 라우터 설정 확인

### 기기 등록 실패
1. 네트워크 연결 상태 확인
2. 서버 URL 확인
3. 토큰 유효성 확인

## 라이센스

MIT License

## 기여하기

1. Fork the project
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request
