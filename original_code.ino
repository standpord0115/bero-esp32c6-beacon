  #include <Arduino.h>
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #include <BLEAdvertising.h>

  // ======================= 설정값 =======================
  static const char*  kDeviceName = "BeRo-C6";
  static const uint16_t kCompanyId = 0xFFFF;
  static const uint32_t kTagId = 10321;

  // 광고 시간: 60초
  static const uint32_t kAdvDurationMs     = 60UL * 1000UL;
  // 종료 10초 전부터 점멸
  static const uint32_t kBlinkBeforeEndMs  = 10UL * 1000UL;

  // ======================= 핀 설정 =======================
  // "빨간 LED"가 연결된 핀으로 맞추기
  static const int  LED_RED_PIN = 8;
  static const bool LED_ACTIVE_LOW = false;

  static inline void redLedWrite(bool on) {
    if (LED_ACTIVE_LOW) digitalWrite(LED_RED_PIN, on ? LOW : HIGH);
    else                digitalWrite(LED_RED_PIN, on ? HIGH : LOW);
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

  // ======================= 상태 머신 =======================
  enum class State { ADVERTISING, EXPIRED };
  static State gState = State::ADVERTISING;
  static uint32_t gAdvStartMs = 0;

  void startOneMinuteCycle() {
    start_ble_advertising();
    gAdvStartMs = millis();
    gState = State::ADVERTISING;
    redLedWrite(false);
    Serial.println("[STATE] 1-minute advertising cycle started");
  }

  // ======================= Arduino =======================
  void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(LED_RED_PIN, OUTPUT);
    redLedWrite(false);

    Serial.println();
    Serial.println("=== BeRo: power/reset -> 1min adv, blink last 10s, then red blink forever ===");

    // 전원/리셋 시 자동 시작
    startOneMinuteCycle();
  }

  void loop() {
    const uint32_t now = millis();

    if (gState == State::ADVERTISING) {
      const uint32_t elapsed = now - gAdvStartMs;
      const uint32_t remain  = (elapsed >= kAdvDurationMs) ? 0 : (kAdvDurationMs - elapsed);

      if (remain == 0) {
        stop_ble_advertising();
        gState = State::EXPIRED;
        Serial.println("[STATE] Expired -> red blink forever (press RESET to restart)");
        return;
      }

      // 종료 10초 전: 0.5초 점멸
      if (remain <= kBlinkBeforeEndMs) {
        static uint32_t lastBlinkMs = 0;
        static bool ledState = false;
        if (now - lastBlinkMs >= 500) {
          lastBlinkMs = now;
          ledState = !ledState;
          redLedWrite(ledState);
        }
      } else {
        // 그 전엔 꺼둠
        redLedWrite(false);
      }

      delay(10);
      return;
    }

    // EXPIRED: 빨간 LED 무한 점멸 (0.5초 간격)
    {
      static uint32_t lastBlinkMs = 0;
      static bool ledState = false;
      if (now - lastBlinkMs >= 500) {
        lastBlinkMs = now;
        ledState = !ledState;
        redLedWrite(ledState);
      }
      delay(10);
    }
  }
