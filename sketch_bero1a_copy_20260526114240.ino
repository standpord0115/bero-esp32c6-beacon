#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <esp_sleep.h>

// ======================= 설정값 =======================
static const char*  kDeviceName = "BeRo-C6";
static const uint16_t kCompanyId = 0xFFFF;
static const uint32_t kTagId = 10321;

// 광고 -> light sleep -> 광고 ... 반복 (약 1초 주기, 초당 광고 1회)
// 깨어나서 광고 패킷을 내보내는 시간
static const uint32_t kAdvOnTimeMs     = 100;
// light sleep 시간: (1초 주기) - (광고 시간)
static const uint64_t kSleepDurationUs = (1000ULL - kAdvOnTimeMs) * 1000ULL;

// ======================= 핀 설정 =======================
// "파란 LED"가 연결된 핀으로 맞추기
static const int  LED_BLUE_PIN = 8;
static const bool LED_ACTIVE_LOW = false;

static inline void blueLedWrite(bool on) {
  if (LED_ACTIVE_LOW) digitalWrite(LED_BLUE_PIN, on ? LOW : HIGH);
  else                digitalWrite(LED_BLUE_PIN, on ? HIGH : LOW);
}

BLEAdvertising* gAdvertising = nullptr;

// ======================= 제조사 데이터 =======================
std::string buildManufacturerData() {
  std::string d;
  d.reserve(2 + 1 + 1 + 4 + 1);

  d.push_back((char)(kCompanyId & 0xFF));
  d.push_back((char)((kCompanyId >> 8) & 0xFF));

  const uint8_t type  = 0x01;
  const uint8_t ver   = 0x01;
  const uint8_t flags = 0x5A;

  d.push_back((char)type);
  d.push_back((char)ver);

  d.push_back((char)(kTagId & 0xFF));
  d.push_back((char)((kTagId >> 8) & 0xFF));
  d.push_back((char)((kTagId >> 16) & 0xFF));
  d.push_back((char)((kTagId >> 24) & 0xFF));

  d.push_back((char)flags);
  return d;
}

// ======================= advertising 시작/정지 =======================
// light sleep은 RAM을 유지하므로 BLEDevice::init은 (inited 가드 덕분에) 최초 1회만 실행됨.
void start_ble_advertising() {
  static bool inited = false;
  if (!inited) {
    BLEDevice::init(kDeviceName);
    BLEServer* server = BLEDevice::createServer();
    (void)server;
    inited = true;
  }

  gAdvertising = BLEDevice::getAdvertising();
  gAdvertising->stop();

  BLEAdvertisementData advData;
  BLEAdvertisementData scanResp;

  advData.setName(kDeviceName);
  scanResp.setName(kDeviceName);

  std::string mfg = buildManufacturerData();
  String mfgStr(mfg.data(), (unsigned int)mfg.size());
  advData.setManufacturerData(mfgStr);

  gAdvertising->setAdvertisementData(advData);
  gAdvertising->setScanResponseData(scanResp);

  gAdvertising->start();
  Serial.println("[BLE] Advertising started");
}

void stop_ble_advertising() {
  if (!gAdvertising) return;
  gAdvertising->stop();
  Serial.println("[BLE] Advertising stopped");
}

// ======================= Arduino =======================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_BLUE_PIN, OUTPUT);
  blueLedWrite(false);

  Serial.println();
  Serial.println("=== BeRo: advertise -> light sleep -> repeat (~1s cycle) ===");
}

// light sleep은 깨어난 뒤 다음 줄부터 계속 실행되므로 loop()로 반복한다.
void loop() {
  // 광고 중에만 파란 LED ON
  blueLedWrite(true);
  start_ble_advertising();

  // 광고 패킷이 나갈 시간을 잠깐 확보
  delay(kAdvOnTimeMs);

  // 광고 정지 + 파란 LED OFF
  stop_ble_advertising();
  blueLedWrite(false);

  // 남은 시간 동안 light sleep (RAM/BLE 컨트롤러 유지) 후 다시 광고
  esp_sleep_enable_timer_wakeup(kSleepDurationUs);
  esp_light_sleep_start();
}
