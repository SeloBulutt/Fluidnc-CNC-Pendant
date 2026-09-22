/**
 * =====================================================
 *  CNC Pendant — Arduino Nano ESP32
 *  v7.0 wifi — Modüler yapı, non-blocking, iyileştirmeler
 * =====================================================
 *
 * DEĞIŞIKLIKLER (v6.0 → v7.0):
 *   #1  RUN durumunda jog engelleme (güvenlik)
 *   #4  Non-blocking popup (delay kaldırıldı)
 *   #5  Non-blocking WiFi connect (state machine)
 *   #6  Async WiFi scan (donma yok)
 *   #7  Char buffer ile heap fragmentation düzeltme
 *   #8  Float epsilon karşılaştırma
 *   #9  Pil okuması önbellekleme (10sn, 8x ortalama)
 *   #10 Flicker azaltma (partial update)
 *   #12 Modüler dosya yapısı
 *   #13 MachineState enum
 *   #14 fcSendRealtime merkezi kullanım
 *   #15 Override gauge birleştirme
 *   #17 Jog Cancel (0x85) — step motor güvenliği
 *   #18 Z Probe ekranı
 *   #19 PWM parlaklık kontrolü
 *   #20 Ayar kalıcılığı (Preferences)
 *
 * FluidNC config.yaml:
 *   uart1:
 *     txd_pin: gpio.38
 *     rxd_pin: gpio.39
 *     baud: 115200
 *     mode: 8N1
 *   uart_channel1:
 *     uart_num: 1
 *     report_interval_ms: 75
 *     message_level: Error
 *
 * BAĞLANTI: (aynı v2.8)
 *   Encoder CLK→D2, DT→D3, SW→A0
 *   Butonlar HOME→D4, ZERO→D5, AXIS→A6, SPEED→A7
 *   TFT CS→D10, DC→D6, RST→D7, BLK→A2
 *   UART RX1→D9, TX1→D8
 *
 * KÜTÜPHANEler: Adafruit ST7789, Adafruit GFX Library
 * =====================================================
 */

#include "config.h"
#include "input.h"
#include "fluidnc.h"
#include "wifi_mgr.h"
#include "display.h"

// ═══════════════════════════════════════════════════════
// ─── GLOBAL DEĞİŞKEN TANIMLARI ───────────────────────
// (Extern bildirimleri config.h'de)
// ═══════════════════════════════════════════════════════

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

Status cur  = {0, 0, 0, 0, 0, 0, 0, 0, ST_IDLE, false, 100, 100, 100};
Status prev = {0, 0, 0, 0, 0, 0, 0, 0, ST_IDLE, false, 100, 100, 100};

ScreenState scrState = SCR_MAIN;
bool needRedraw = true;
unsigned long lastActivity = 0;

uint8_t selAxis = 0;
uint8_t selStep = 0;     // 0.100 mm
uint8_t selFeed = 1;     // 2000 mm/dk

int menuIdx = 0;
uint8_t ovSelIdx = 0;    // Override seçim (0=Feed, 1=Spindle)
int spindleTarget = 0;
uint8_t coolantSel = 0;  // 0=OFF, 1=FLOOD, 2=MIST

unsigned long lastDataReceived = 0;
bool sleepWarningShown = false;
unsigned long lastJog = 0;

// #19 Parlaklık
uint8_t brightnessIdx = BRIGHTNESS_LEVELS - 1;  // Varsayılan tam parlaklık

// #18 Z Probe
int probeDepth = PROBE_DEPTH_DEFAULT;
int probeFeed = PROBE_FEED_DEFAULT;
int probeRetract = PROBE_RETRACT_DEFAULT;
uint8_t probeParamIdx = 0;  // 0=derinlik, 1=hız, 2=geri çekilme


// ═══════════════════════════════════════════════════════
// ─── EKRAN GEÇİŞ ─────────────────────────────────────
// ═══════════════════════════════════════════════════════

void switchScreen(ScreenState s) {
  scrState = s;
  needRedraw = true;
  lastActivity = millis();
}

// ═══════════════════════════════════════════════════════
// ─── DEEP SLEEP ──────────────────────────────────────
// ═══════════════════════════════════════════════════════

void enterDeepSleep() {
  Serial.println("[SLEEP] Entering deep sleep...");
  // #20 Uykuya geçmeden önce ayarları kaydet
  saveUserPrefs();

  // Ekranda uyku mesajı göster
  tft.fillScreen(C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(100, 40);
  tft.print("UYKU MODU");
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 80);
  tft.print("FluidNC veri yok - enerji tasarrufu");
  tft.setTextColor(C_GREEN);
  tft.setCursor(60, 110);
  tft.print("Uyandirmak icin HOME tusuna basin");
  delay(2000);

  // #19 Ekran arka ışığını PWM ile kapat
  analogWrite(TFT_BLK, 0);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  tft.enableDisplay(false);  // ST7789 uyku komutu

  // WiFi kapat
  if (wifiConnected) {
    tcpClient.stop();
    if (mdnsStarted) {
      MDNS.end();
      mdnsStarted = false;
    }
    WiFi.disconnect();
  }
  WiFi.mode(WIFI_OFF);

  // ÖNEMLİ: HOME butonunun bırakılmasını bekle
  while (digitalRead(BTN_HOME) == LOW) {
    delay(10);
  }
  delay(50);  // debounce

  // Uyku sırasında pull-up aktif tut
  rtc_gpio_pullup_en(BTN_HOME_GPIO);
  rtc_gpio_pulldown_dis(BTN_HOME_GPIO);

  // HOME butonu (GPIO7) ile uyanma ayarla (LOW = basılı)
  esp_sleep_enable_ext0_wakeup(BTN_HOME_GPIO, LOW);
  esp_deep_sleep_start();
}

// ═══════════════════════════════════════════════════════
// ─── SETUP ────────────────────────────────────────────
// ═══════════════════════════════════════════════════════

void setup() {
  // Encoder pin setup
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastClkVal = digitalRead(ENC_CLK);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT), encISR, CHANGE);

  // Buton pin setup
  for (int i = 0; i < 4; i++)
    pinMode(btns[i].pin, INPUT_PULLUP);

  Serial.begin(115200);

  // Uyanma sebebini kontrol et
  esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
  if (wakeup == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[PENDANT] Woke up from deep sleep (HOME button)");
  } else {
    Serial.println("[PENDANT] Starting (normal boot)...");
  }

  // ADC çözünürlüğü ESP32-S3'te 12-bit (0-4095)
  analogReadResolution(12);

  // TFT başlat
  tft.init(TFT_H, TFT_W);
  tft.enableDisplay(true);
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  tft.setTextWrap(false);

  // Ayarları yükle
  wifiLoadPrefs();
  loadUserPrefs();  // #20 step/feed/axis/brightness

  // #19 Parlaklık ayarla (PWM)
  analogWrite(TFT_BLK, BRIGHTNESS_VAL[brightnessIdx]);

  // UART başlat
  FLUIDNC.begin(FC_BAUD, SERIAL_8N1, D9, D8);

  // #5 WiFi auto-connect (non-blocking)
  if (wifiSSID.length() > 0) {
    wifiConnectStart();
  }

  Serial.print("[ENC_SW] Pin initial state: ");
  Serial.println(digitalRead(ENC_SW) ? "HIGH (released)" : "LOW (pressed)");

  // Splash ekranı
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(85, 35);
  tft.print("CNC PENDANT");
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(88, 65);
  tft.print("FluidNC Pendant v7.0");
  tft.setTextColor(C_GRAY);
  tft.setCursor(110, 82);
  tft.print("by VOLTveTORK");
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  delay(2500);

  // İlk pil okuması (#9)
  updateBattery();

  needRedraw = true;
  lastActivity = millis();
  lastDataReceived = millis();
}

// ═══════════════════════════════════════════════════════
// ─── LOOP ─────────────────────────────────────────────
// ═══════════════════════════════════════════════════════

void loop() {
  readFluidNC();
  checkEncClick();
  readBtns();

  // #4 Non-blocking popup güncelle
  updatePopup();

  // #9 Pil okuması (10 saniyede bir)
  updateBattery();

  // UART aktiflik kontrolü (500ms içerisinde veri geldiyse aktif)
  bool prevUartActive = uartActive;
  uartActive = (millis() - lastUartRx < UART_TIMEOUT_MS);
  // Veri gelirse uyku zamanlayıcısını sıfırla
  if (uartActive) lastDataReceived = millis();
  if (prevUartActive && !uartActive) {
    Serial.println("[UART] UART pasif, TCP'ye geciliyor...");
    needRedraw = true;
  } else if (!prevUartActive && uartActive) {
    Serial.println("[UART] UART aktif, TCP devre disi");
    needRedraw = true;
  }

  // UART'a periyodik ? sorgusu gönder (FluidNC rapor başlatsın)
  if (millis() - lastUartStatus > UART_STATUS_INTERVAL) {
    lastUartStatus = millis();
    FLUIDNC.println("?");
  }

  // #5 WiFi bağlantı state machine
  if (wcsState == WCS_CONNECTING) {
    WifiConnState st = wifiConnectUpdate();
    if (st == WCS_CONNECTED) {
      showPopup("WIFI BAGLANDI!", C_GREEN, 1000);
    } else if (st == WCS_FAILED) {
      showPopup("WIFI BASARISIZ!", C_RED, 1000);
    }
  }

  // TCP loop (gelen veri oku + bağlantı yönetimi)
  if (wifiConnected) {
    // TCP bağlantı durumunu kontrol et
    if (tcpConnected && !tcpClient.connected()) {
      tcpConnected = false;
      Serial.println("[TCP] Connection lost!");
      needRedraw = true;
    }
    // TCP bağlı değilse ve UART pasifse yeniden bağlanmayı dene
    unsigned long reconnectInterval = (tcpFailCount < 3)    ? 5000
                                    : (tcpFailCount < 10)   ? 15000
                                                            : 30000;
    if (!uartActive && !tcpConnected &&
        millis() - lastTcpReconnect > reconnectInterval) {
      lastTcpReconnect = millis();
      tcpStartConnection();
    }
    // TCP'den gelen veriyi her zaman oku
    if (tcpConnected) {
      tcpReadIncoming();
      // Status sorgusu sadece UART pasifse gönder
      if (!uartActive && millis() - lastTcpStatus > TCP_STATUS_INTERVAL) {
        tcpClient.println("?");
        lastTcpStatus = millis();
      }
    }
  }

  // WiFi bağlantı durumunu güncelle
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    tcpConnected = false;
    Serial.println("[WIFI] Connection lost!");
    needRedraw = true;
  }

  // Periyodik debug log
  if (millis() - lastDebugLog > DEBUG_LOG_INTERVAL) {
    lastDebugLog = millis();
    unsigned long sleepRemain = 0;
    if (millis() - lastDataReceived < SLEEP_TIMEOUT_MS)
      sleepRemain = (SLEEP_TIMEOUT_MS - (millis() - lastDataReceived)) / 1000;
    Serial.printf("[DBG] UART:%s  WiFi:%s  TCP:%s  IP:%d.%d.%d.%d:%d  "
                  "Sleep:%lus  Bat:%d%%\n",
                  uartActive ? "YES" : "NO", wifiConnected ? "YES" : "NO",
                  tcpConnected ? "YES" : "NO", wifiIP[0], wifiIP[1], wifiIP[2],
                  wifiIP[3], FLUIDNC_TCP_PORT, sleepRemain, cachedBatPct);
  }

  // ── DEEP SLEEP KONTROLÜ ──────────────────────────────
  unsigned long noDataDuration = millis() - lastDataReceived;
  // 10 saniye kala uyarı göster (#4 non-blocking)
  if (noDataDuration > SLEEP_WARNING_MS && !sleepWarningShown &&
      scrState == SCR_MAIN) {
    sleepWarningShown = true;
    showPopup("UYKU: 5 SN...", C_ORANGE, 500);
  }
  // 2 dakika doldu → deep sleep
  if (noDataDuration > SLEEP_TIMEOUT_MS) {
    enterDeepSleep();
  }
  // Veri gelirse uyarı flag'ini sıfırla
  if (uartActive || tcpConnected) {
    sleepWarningShown = false;
  }

  // Her detent 2 puls üretiyor → 2'ye böl, kalanı biriktir
  static int encAccum = 0;
  noInterrupts();
  encAccum += encDelta;
  encDelta = 0;
  interrupts();
  int delta = encAccum / 2;
  encAccum %= 2;  // kalan tek pulsu bir sonraki tura aktar

  if (delta != 0) lastActivity = millis();

  // HOME hariç, buton basıldıysa alt ekranlardan çık
  bool anyBtnExceptHome = btns[1].fired || btns[2].fired || btns[3].fired;
  // WiFi şifre/IP, Override, Probe ekranlarında butonlar içeride işleniyor
  if (anyBtnExceptHome && scrState != SCR_MAIN && scrState != SCR_WIFI_PASS &&
      scrState != SCR_WIFI_IP && scrState != SCR_WIFI_MENU &&
      scrState != SCR_WIFI_SCAN && scrState != SCR_FEED_OV &&
      scrState != SCR_SPINDLE_OV && scrState != SCR_OVERRIDE_SEL &&
      scrState != SCR_PROBE) {
    for (int i = 1; i < 4; i++) btns[i].fired = false;
    switchScreen(SCR_MAIN);
  }

  // 10 saniye timeout → ana ekrana dön
  if (scrState != SCR_MAIN && (millis() - lastActivity > MENU_TIMEOUT_MS)) {
    switchScreen(SCR_MAIN);
  }

  // ══════════════════════════════════════════════════════
  // ─── EKRAN DURUMUNA GÖRE İŞLEM ─────────────────────
  // ══════════════════════════════════════════════════════

  switch (scrState) {

  // ── ANA EKRAN ──────────────────────────────────────
  case SCR_MAIN: {
    // Uzun basma → ana ekranda işlevi yok
    if (encLongPress) encLongPress = false;

    // ── SPEED BUTONU: Duraklat / Devam Et (#14) ──
    if (btns[3].fired) {
      if (cur.state == ST_RUN || cur.state == ST_JOG) {
        btns[3].fired = false;
        fcSendRealtime('!');  // #14 merkezi fonksiyon
        showPopup("DURAKLANDI", C_YELLOW, 600);
        Serial.println("[PENDANT] Feed Hold sent");
        needRedraw = true;
      } else if (cur.state == ST_HOLD) {
        btns[3].fired = false;
        fcSendRealtime('~');  // #14 merkezi fonksiyon
        showPopup("DEVAM EDILIYOR", C_GREEN, 600);
        Serial.println("[PENDANT] Cycle Start sent");
        needRedraw = true;
      }
    }

    // ── ALARM DURUMU: Kurtarma (#13 enum) ──
    if (cur.state == ST_ALARM) {
      // ZERO butonu → Soft Reset (Ctrl+X) (#14)
      if (btns[1].fired) {
        btns[1].fired = false;
        fcSendRealtime(0x18);  // #14 merkezi fonksiyon
        showPopup("SOFT RESET!", C_ORANGE, 1000);
        Serial.println("[PENDANT] Soft Reset sent (0x18)");
        needRedraw = true;
      }
      // HOME butonu → Alarm kilidi aç ($X)
      if (btns[0].fired) {
        btns[0].fired = false;
        fcSend("$X");
        showPopup("KILIT ACILDI", C_GREEN, 800);
        Serial.println("[PENDANT] Alarm unlock ($X) sent");
        needRedraw = true;
      }
      // Alarm durumunda diğer butonları yoksay
      btns[2].fired = false;
      btns[3].fired = false;
      if (encClicked) encClicked = false;
      updateMainDisplay();
      break;
    }

    // Encoder tıkla → menüye gir
    if (encClicked) {
      encClicked = false;
      menuIdx = 0;
      switchScreen(SCR_MENU);
      break;
    }

    // Butonlar: normal çalışma
    if (btns[0].fired) {
      btns[0].fired = false;
      fcHome();
      showPopup("HOMING...", C_ORANGE, 300);
    }
    if (btns[1].fired) {
      btns[1].fired = false;
      fcZero(selAxis);
      char msg[10];
      snprintf(msg, sizeof(msg), "ZERO %s", AXIS_STR[selAxis]);
      showPopup(msg, C_GREEN, 400);
    }
    if (btns[2].fired) {
      btns[2].fired = false;
      // RUN/HOLD durumunda AXIS = Override kısayolu
      if (cur.state == ST_RUN || cur.state == ST_HOLD) {
        ovSelIdx = 0;
        switchScreen(SCR_OVERRIDE_SEL);
        break;
      } else {
        selAxis = (selAxis + 1) % 3;
        needRedraw = true;
      }
    }
    if (btns[3].fired) {
      btns[3].fired = false;
      selStep = (selStep + 1) % N_STEPS;
      if (selStep == 0) selFeed = (selFeed + 1) % N_FEEDS;
      needRedraw = true;
    }

    // Encoder → jog (v6 orijinal mantığı)
    if (delta != 0) {
      uint32_t now = millis();
      if (now - lastJog >= JOG_THROTTLE_MS) {
        lastJog = now;
        int8_t dir = delta > 0 ? 1 : -1;
        float totalDist = STEP_VAL[selStep] * abs(delta);
        fcJog(selAxis, dir, totalDist, FEED_VAL[selFeed]);
      }
    }

    updateMainDisplay();
    break;
  }

  // ── MENÜ EKRANI ────────────────────────────────────
  case SCR_MENU: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MAIN);
      break;
    }
    if (delta != 0) {
      int count = getMenuCount();
      menuIdx += (delta > 0) ? 1 : -1;
      if (menuIdx < 0) menuIdx = count - 1;
      if (menuIdx >= count) menuIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      bool runMode = (cur.state == ST_RUN || cur.state == ST_HOLD);
      if (runMode) {
        // RUN/HOLD menüsü
        switch (menuIdx) {
          case 0: switchScreen(SCR_SPINDLE_OV); break;
          case 1: switchScreen(SCR_FEED_OV);    break;
          case 2: switchScreen(SCR_JOG);        break;
          case 3: switchScreen(SCR_STEP);       break;
          case 4: switchScreen(SCR_COOLANT);    break;
        }
      } else {
        // IDLE menüsü (#18 Z Probe eklendi)
        switch (menuIdx) {
          case 0:
            spindleTarget = (int)cur.spindle;
            switchScreen(SCR_SPINDLE);
            break;
          case 1: switchScreen(SCR_JOG);        break;
          case 2: switchScreen(SCR_STEP);       break;
          case 3: switchScreen(SCR_COOLANT);    break;
          case 4: switchScreen(SCR_PROBE);      break;  // #18
          case 5:
            wifiMenuIdx = 0;
            switchScreen(SCR_WIFI_MENU);
            break;
        }
      }
      break;
    }
    if (needRedraw) drawMenuScreen();
    break;
  }

  // ── SPINDLE KONTROL ────────────────────────────────
  case SCR_SPINDLE: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    bool changed = false;
    if (delta != 0) {
      spindleTarget += delta * SPINDLE_STEP;
      if (spindleTarget < 0) spindleTarget = 0;
      if (spindleTarget > SPINDLE_MAX) spindleTarget = SPINDLE_MAX;
      changed = true;
    }
    if (encClicked) {
      encClicked = false;
      char cmd[16];
      if (spindleTarget > 0)
        snprintf(cmd, sizeof(cmd), "M3 S%d", spindleTarget);
      else
        snprintf(cmd, sizeof(cmd), "M5");
      fcSend(cmd);
      changed = true;
      lastActivity = millis();
    }
    // Spindle değeri FluidNC'den güncelleniyorsa da yeniden çiz
    static float prevDrawSpindle = -1;
    static int prevDrawTarget = -1;
    if (needRedraw || changed || cur.spindle != prevDrawSpindle ||
        spindleTarget != prevDrawTarget) {
      drawSpindleScreen();
      prevDrawSpindle = cur.spindle;
      prevDrawTarget = spindleTarget;
    }
    break;
  }

  // ── JOG HIZI ───────────────────────────────────────
  case SCR_JOG: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      selFeed += (delta > 0) ? 1 : -1;
      if ((int8_t)selFeed < 0) selFeed = N_FEEDS - 1;
      if (selFeed >= N_FEEDS) selFeed = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      saveUserPrefs();  // #20 kalıcılık
      showPopup("JOG HIZI SECILDI", C_GREEN, 300);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw) drawJogScreen();
    break;
  }

  // ── STEP BOYUTU ────────────────────────────────────
  case SCR_STEP: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      selStep += (delta > 0) ? 1 : -1;
      if ((int8_t)selStep < 0) selStep = N_STEPS - 1;
      if (selStep >= N_STEPS) selStep = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      saveUserPrefs();  // #20 kalıcılık
      showPopup("STEP SECILDI", C_GREEN, 300);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw) drawStepScreen();
    break;
  }

  // ── SOĞUTMA ────────────────────────────────────────
  case SCR_COOLANT: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      coolantSel += (delta > 0) ? 1 : -1;
      if ((int8_t)coolantSel < 0) coolantSel = 2;
      if (coolantSel > 2) coolantSel = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      switch (coolantSel) {
        case 0: fcSend("M9"); break;
        case 1: fcSend("M8"); break;
        case 2: fcSend("M7"); break;
      }
      showPopup(COOL_STR[coolantSel], C_DKGREEN, 500);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw) drawCoolantScreen();
    break;
  }

  // ── FEED OVERRIDE (#15 birleştirilmiş gauge) ──────
  case SCR_FEED_OV: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      // Her encoder adımı doğrudan ±10% komutu gönderir
      uint8_t cmd = (delta > 0) ? 0x91 : 0x92;
      int steps = abs(delta);
      for (int i = 0; i < steps; i++) {
        fcSendRealtime(cmd);
        delay(20);
      }
      Serial.printf("[OV] Feed %s%d0%% sent\n",
                    delta > 0 ? "+" : "-", steps);
      lastActivity = millis();
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      sendFeedOvReset();  // %100'e sıfırla
      showPopup("FEED %100 RESET", C_GREEN, 400);
      Serial.println("[OV] Feed override reset to 100%");
      lastActivity = millis();
      needRedraw = true;
    }
    // FluidNC'den güncel değer değiştiğinde ekranı güncelle
    static uint8_t prevFeedOv = 255;
    if (needRedraw || cur.feedOv != prevFeedOv) {
      drawOverrideGauge("FEED OVERRIDE", "FEED",
                        cur.feedOv, cur.state == ST_RUN);
      prevFeedOv = cur.feedOv;
    }
    break;
  }

  // ── SPINDLE OVERRIDE (#15 birleştirilmiş gauge) ───
  case SCR_SPINDLE_OV: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      uint8_t cmd = (delta > 0) ? 0x9A : 0x9B;
      int steps = abs(delta);
      for (int i = 0; i < steps; i++) {
        fcSendRealtime(cmd);
        delay(20);
      }
      Serial.printf("[OV] Spindle %s%d0%% sent\n",
                    delta > 0 ? "+" : "-", steps);
      lastActivity = millis();
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      sendSpnOvReset();  // %100'e sıfırla
      showPopup("SPINDLE %100 RESET", C_GREEN, 400);
      Serial.println("[OV] Spindle override reset to 100%");
      lastActivity = millis();
      needRedraw = true;
    }
    static uint8_t prevSpnOv = 255;
    if (needRedraw || cur.spindleOv != prevSpnOv) {
      drawOverrideGauge("SPINDLE OVERRIDE", "SPINDLE",
                        cur.spindleOv, cur.state == ST_RUN);
      prevSpnOv = cur.spindleOv;
    }
    break;
  }

  // ── OVERRIDE SEÇİM EKRANI ─────────────────────────
  case SCR_OVERRIDE_SEL: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MAIN);
      break;
    }
    if (delta != 0) {
      ovSelIdx = (ovSelIdx == 0) ? 1 : 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      if (ovSelIdx == 0) switchScreen(SCR_FEED_OV);
      else switchScreen(SCR_SPINDLE_OV);
      break;
    }
    if (needRedraw) drawOverrideSelScreen();
    break;
  }

  // ── WIFI MENÜ ──────────────────────────────────────
  case SCR_WIFI_MENU: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      wifiMenuIdx += (delta > 0) ? 1 : -1;
      if (wifiMenuIdx < 0) wifiMenuIdx = WIFI_MENU_COUNT - 1;
      if (wifiMenuIdx >= WIFI_MENU_COUNT) wifiMenuIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      switch (wifiMenuIdx) {
        case 0:  // Ağ Tara (#6 async)
          showPopup("Taraniyor...", C_CYAN, 200);
          wifiStartScanAsync();
          switchScreen(SCR_WIFI_SCAN);
          break;
        case 1:  // Şifre Gir
          charIdx = 0;
          passBuffer = wifiPass;  // mevcut şifreyi yükle
          switchScreen(SCR_WIFI_PASS);
          break;
        case 2: {  // Oto IP Bul (mDNS)
          if (!wifiConnected) {
            showPopup("ONCE WIFI BAGLA!", C_RED, 1000);
          } else {
            showPopup("IP Araniyor...", C_CYAN, 200);
            if (mdnsDiscoverFluidNC()) {
              char ipMsg[24];
              snprintf(ipMsg, sizeof(ipMsg), "IP: %d.%d.%d.%d",
                       wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
              showPopup(ipMsg, C_GREEN, 1500);
              tcpStartConnection();
            } else {
              showPopup("BULUNAMADI!", C_RED, 1000);
            }
          }
          needRedraw = true;
          break;
        }
        case 3:  // Bağlan / Kes (#5 non-blocking)
          if (wifiConnected) {
            wifiDisconnect();
            showPopup("BAGLANTI KESILDI", C_RED, 800);
          } else {
            showPopup("Baglaniyor...", C_CYAN, 500);
            wifiConnectStart();  // #5 non-blocking
          }
          needRedraw = true;
          break;
        case 4: {  // Parlaklık (#19)
          brightnessIdx = (brightnessIdx + 1) % BRIGHTNESS_LEVELS;
          analogWrite(TFT_BLK, BRIGHTNESS_VAL[brightnessIdx]);
          saveUserPrefs();  // #20 kalıcılık
          char bMsg[20];
          snprintf(bMsg, sizeof(bMsg), "PARLAKLIK: %d/%d",
                   brightnessIdx + 1, BRIGHTNESS_LEVELS);
          showPopup(bMsg, C_CYAN, 600);
          needRedraw = true;
          break;
        }
        case 5:  // Geri Dön
          switchScreen(SCR_MENU);
          break;
      }
      break;
    }
    if (needRedraw) drawWifiMenuScreen();
    break;
  }

  // ── WIFI TARAMA (#6 async) ────────────────────────
  case SCR_WIFI_SCAN: {
    if (encLongPress) {
      encLongPress = false;
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    // Async scan tamamlanma kontrolü
    if (wifiScanRunning) {
      if (wifiScanUpdate()) {
        needRedraw = true;  // Scan tamamlandı, ekranı güncelle
      }
    }
    if (delta != 0 && scanCount > 0 && !wifiScanRunning) {
      wifiScanIdx += (delta > 0) ? 1 : -1;
      if (wifiScanIdx < 0) wifiScanIdx = scanCount - 1;
      if (wifiScanIdx >= scanCount) wifiScanIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      if (scanCount > 0 && !wifiScanRunning) {
        wifiSSID = scanSSIDs[wifiScanIdx];
        Serial.printf("[WIFI] Selected SSID: %s\n", wifiSSID.c_str());
        char msg[32];
        snprintf(msg, sizeof(msg), "AG: %s",
                 wifiSSID.substring(0, 14).c_str());
        showPopup(msg, C_GREEN, 800);
        switchScreen(SCR_WIFI_MENU);
      }
      break;
    }
    if (needRedraw) drawWifiScanScreen();
    break;
  }

  // ── WIFI ŞİFRE GİRİŞ ──────────────────────────────
  case SCR_WIFI_PASS: {
    if (encLongPress) {
      encLongPress = false;
      // Uzun basma → son karakteri sil (backspace)
      if (passBuffer.length() > 0) {
        passBuffer.remove(passBuffer.length() - 1);
        needRedraw = true;
      }
      break;
    }
    if (delta != 0) {
      charIdx += delta;
      if (charIdx < 0) charIdx = CHAR_SET_LEN - 1;
      if (charIdx >= (int)CHAR_SET_LEN) charIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      // Kısa tık → karakteri ekle
      if ((int)passBuffer.length() < PASS_MAX_LEN) {
        passBuffer += CHAR_SET[charIdx];
        needRedraw = true;
      }
    }
    // AXIS butonu → onayla ve kaydet
    if (btns[2].fired) {
      btns[2].fired = false;
      wifiPass = passBuffer;
      wifiSavePrefs();
      showPopup("SIFRE KAYDEDILDI", C_GREEN, 800);
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (needRedraw) drawWifiPassScreen();
    break;
  }

  // ── WIFI IP GİRİŞ ──────────────────────────────────
  case SCR_WIFI_IP: {
    if (encLongPress) {
      encLongPress = false;
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (delta != 0) {
      int val = (int)wifiIP[ipEditIdx] + delta;
      if (val < 0) val = 255;
      if (val > 255) val = 0;
      wifiIP[ipEditIdx] = (uint8_t)val;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      // Sonraki oktet'e geç
      ipEditIdx = (ipEditIdx + 1) % 4;
      needRedraw = true;
    }
    // AXIS butonu → kaydet
    if (btns[2].fired) {
      btns[2].fired = false;
      wifiSavePrefs();
      // WiFi bağlıysa TCP'yi yeni IP ile yeniden başlat
      if (wifiConnected) {
        tcpStartConnection();
        Serial.println("[TCP] Reconnecting with new IP...");
      }
      char ipStr[20];
      snprintf(ipStr, sizeof(ipStr), "IP: %d.%d.%d.%d",
               wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
      showPopup(ipStr, C_GREEN, 1000);
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (needRedraw) drawWifiIPScreen();
    break;
  }

  // ── Z PROBE (#18) ──────────────────────────────────
  case SCR_PROBE: {
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      if (probeParamIdx == 0) {
        // Derinlik ayarla
        probeDepth += delta;
        probeDepth = constrain(probeDepth, PROBE_DEPTH_MIN, PROBE_DEPTH_MAX);
      } else if (probeParamIdx == 1) {
        // Hız ayarla (10'ar artış)
        probeFeed += delta * 10;
        probeFeed = constrain(probeFeed, PROBE_FEED_MIN, PROBE_FEED_MAX);
      } else {
        // Geri çekilme ayarla (1 mm artış)
        probeRetract += delta;
        probeRetract = constrain(probeRetract, PROBE_RETRACT_MIN, PROBE_RETRACT_MAX);
      }
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      // Parametre seçimi değiştir (derinlik ↔ hız ↔ geri çekilme)
      probeParamIdx = (probeParamIdx + 1) % 3;
      needRedraw = true;
    }
    // AXIS butonu → probe başlat
    if (btns[2].fired) {
      btns[2].fired = false;
      // Probe sadece IDLE durumunda çalıştırılabilir
      if (cur.state == ST_IDLE) {
        char probeCmd[48];
        // 1) Dokunma araması (göreceli modda Z aşağı)
        snprintf(probeCmd, sizeof(probeCmd), "G91 G38.2 Z%d F%d",
                 probeDepth, probeFeed);
        fcSend(probeCmd);
        // 2) Dokunulan temas yüzeyini Z=0 olarak sıfırla
        fcSend("G92 Z0");
        // 3) Geri çekilme (Z ekseninde yukarı çık) ve G90 mutlak moda dön
        snprintf(probeCmd, sizeof(probeCmd), "G90 G0 Z%d", probeRetract);
        fcSend(probeCmd);

        showPopup("PROBE BASLADI", C_GREEN, 1000);
        Serial.printf("[PROBE] Cycle: G91 G38.2 Z%d F%d -> G92 Z0 -> G90 G0 Z%d\n",
                      probeDepth, probeFeed, probeRetract);
      } else {
        showPopup("SADECE IDLE!", C_RED, 800);
      }
      needRedraw = true;
    }
    if (needRedraw) drawProbeScreen();
    break;
  }

  } // end switch(scrState)
}
