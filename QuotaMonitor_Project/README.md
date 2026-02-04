# 🚀 Antigravity Quota Monitor

ESP32-S3에서 Antigravity AI 모델의 quota 사용량을 실시간으로 모니터링하는 프로젝트입니다.

## 📋 기능

- ✅ 여러 AI 모델의 quota 사용량 표시
- ✅ 프로그레스 바로 시각화
- ✅ 상태별 색상 표시 (🟢 녹색 / 🟡 노란색 / 🔴 빨간색)
- ✅ 리셋 시간 카운트다운
- ✅ 스크롤 가능한 리스트
- ✅ 자동 갱신 (60초마다)
- ✅ 다크/라이트 모드 전환 (화면 터치)

## 🛠️ 하드웨어 요구사항

- ESP32-S3 개발 보드
- 3.5인치 IPS 터치스크린 (480x320)
- 8MB PSRAM, 16MB Flash

## 📦 설치 방법

### 1. API 서버 실행

```bash
cd ServerDashboard/quota-server
npm install
npm start
```

서버가 시작되면 로컬 IP 주소가 표시됩니다:

```
📡 ESP32에서 사용할 URL: http://192.168.0.185:3000/quota.json
```

### 2. ESP32 코드 수정

`QuotaMonitor.ino` 파일에서 다음 부분을 수정하세요:

```cpp
// Wi-Fi 설정
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API 서버 주소 (위에서 표시된 IP로 변경)
const char* quota_api_url = "http://192.168.0.185:3000/quota.json";
```

### 3. 빌드 및 업로드

```bash
cd QuotaMonitor
arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi,LoopCore=1,EventsCore=1 QuotaMonitor.ino
arduino-cli upload -p /dev/cu.usbmodem2101 --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi,LoopCore=1,EventsCore=1 QuotaMonitor.ino
```

## 🎨 UI 구성

```
┌─────────────────────────────────────────┐
│  🚀 Antigravity Quota Monitor           │
├─────────────────────────────────────────┤
│ 🟢 Claude Opus 4.5 (Thinking)           │
│    ████████████████ 100.0% → 4h 46m     │
├─────────────────────────────────────────┤
│ 🟢 Claude Sonnet 4.5                    │
│    ████████████████ 100.0% → 4h 46m     │
├─────────────────────────────────────────┤
│ 🟢 Gemini 3 Flash                       │
│    ████████         40.0% → 1m          │
├─────────────────────────────────────────┤
│ 🟡 Gemini 3 Pro (High)                  │
│    ████             20.0% → 2h 16m      │
├─────────────────────────────────────────┤
│                                         │
│ Last Sync: 2026-02-04T12:01:39.711Z     │
└─────────────────────────────────────────┘
```

## 🎯 사용 방법

1. **API 서버 실행**: 로컬 컴퓨터에서 quota-server 실행
2. **ESP32 연결**: ESP32가 같은 Wi-Fi 네트워크에 연결
3. **자동 갱신**: 60초마다 자동으로 quota 정보 갱신
4. **테마 전환**: 화면 터치로 다크/라이트 모드 전환

## 📝 주의사항

- ESP32와 API 서버가 같은 Wi-Fi 네트워크에 있어야 합니다
- 방화벽에서 포트 3000을 허용해야 할 수 있습니다
- 현재는 더미 데이터를 사용합니다 (실제 Antigravity API 연동은 추가 작업 필요)

## 🔧 문제 해결

### Wi-Fi 연결 실패

- SSID와 비밀번호가 올바른지 확인
- ESP32가 2.4GHz Wi-Fi를 지원하는지 확인

### API 연결 실패

- API 서버가 실행 중인지 확인
- IP 주소가 올바른지 확인
- 방화벽 설정 확인

### 화면이 표시되지 않음

- 디스플레이 연결 확인
- 시리얼 모니터에서 오류 메시지 확인

## 📄 라이선스

MIT License
