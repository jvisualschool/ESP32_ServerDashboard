# ESP32-S3 Server Dashboard

2개 서버의 실시간 상태를 모니터링하는 ESP32-S3 대시보드

## 📋 프로젝트 개요

ESP32-S3 디바이스를 사용하여 여러 서버의 상태를 실시간으로 모니터링하고 시각화하는 프로젝트입니다.

## ✨ 주요 기능

### 🖥️ 2x2 그리드 레이아웃

- **CPU Load (5m)**: 5분 평균 CPU 로드
- **Memory Usage**: 메모리 사용률
- **SSD Usage**: 디스크 사용률
- **Uptime**: 서버 가동 시간 (일 단위)

### 🔄 멀티 서버 모니터링

- **jvibeschool.com**: 메인 서버
- **jvibeschool.org**: 서브 서버
- 좌우 스와이프로 서버 전환
- 서버별 배지(.com/.org) 표시

### 🎨 테마 시스템

- **다크 모드**: 기본 테마 (검정 배경)
- **라이트 모드**: 밝은 테마 (흰색 배경)
- 화면 탭으로 전환

### 📊 실시간 업데이트

- 30초마다 자동 갱신
- LED 깜빡임으로 데이터 수신 표시
- Last Sync 시간 표시

## 🛠️ 하드웨어

- **디바이스**: ESP32-S3 (3.5" IPS 터치스크린)
- **해상도**: 480x320
- **메모리**: 8M PSRAM, 16M Flash
- **연결**: Wi-Fi

## 📦 의존성

- Arduino IDE / arduino-cli
- LVGL 8.3.9
- ArduinoJson 7.4.2
- WiFi, HTTPClient, WiFiClientSecure

## 🚀 설치 및 사용

### 1. Wi-Fi 설정

`ServerDashboard.ino` 파일에서 Wi-Fi 정보 수정:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 2. 컴파일 및 업로드

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi ServerDashboard.ino
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi ServerDashboard.ino
```

### 3. 에뮬레이터

브라우저에서 `emulator.html` 열기:

- 실제 서버 데이터 연동
- 버튼 또는 키보드 화살표로 서버 전환
- 클릭으로 테마 전환

## 📡 서버 API

각 서버는 다음 형식의 JSON을 제공해야 합니다:

```json
{
  "status": "online",
  "updated_at": "2026-02-04 15:20:01",
  "hostname": "jvibeschool.com",
  "uptime": "24 weeks, 6 days, 18 hours, 28 minutes",
  "cpu_load": {
    "1min": 0.0,
    "5min": 0.007,
    "15min": 0.0
  },
  "memory": {
    "total_mb": 945,
    "used_mb": 522,
    "percent": 55.2
  },
  "disk": {
    "percent": 36
  }
}
```

### 엔드포인트

- jvibeschool.com: `https://jvibeschool.com/status.json`
- jvibeschool.org: `https://jvibeschool.org/status.json.php`

## 🎮 사용 방법

### ESP32-S3 기기

- **좌우 스와이프**: 서버 전환
- **화면 탭**: 다크/라이트 모드 전환
- **LED**: 데이터 수신 시 깜빡임

### 에뮬레이터

- **버튼 클릭**: 서버 전환
- **키보드 ← →**: 서버 전환
- **화면 클릭**: 테마 전환

## 📝 버전 히스토리

### v1.0 (2026-02-04)

- ✅ 2x2 그리드 레이아웃
- ✅ 2개 서버 모니터링
- ✅ 좌우 스와이프 서버 전환
- ✅ 서버 배지(.com/.org) 표시
- ✅ 다크/라이트 테마 전환
- ✅ 업타임 일수 변환 (주+일 → 총 일수)
- ✅ LED 깜빡임 효과
- ✅ 에뮬레이터 동기화

## 📄 라이선스

MIT License

## 👤 작성자

정진호 (Jinho Jung)
