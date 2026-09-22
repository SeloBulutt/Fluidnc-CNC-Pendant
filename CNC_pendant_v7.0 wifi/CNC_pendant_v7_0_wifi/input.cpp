/**
 * input.cpp — Encoder ISR, kısa/uzun basma, buton debounce
 */
#include "input.h"

// ─── ENCODER DEĞİŞKENLERİ ────────────────────────────
volatile int encDelta = 0;
volatile int lastClkVal = HIGH;

static bool encSwPrev = HIGH;
static bool encSwStable = HIGH;
bool encClicked = false;
bool encLongPress = false;
bool encLongFired = false;
static unsigned long encPressStart = 0;
static unsigned long encSwChangeTime = 0;

// ─── BUTON DEĞİŞKENLERİ ──────────────────────────────
Btn btns[4] = {
  {BTN_HOME,  HIGH, false, 0},
  {BTN_ZERO,  HIGH, false, 0},
  {BTN_AXIS,  HIGH, false, 0},
  {BTN_SPEED, HIGH, false, 0}
};

unsigned long homePressStart = 0;
bool homeLongFired = false;

// ─── ENCODER ISR ──────────────────────────────────────
void IRAM_ATTR encISR() {
  int clk = digitalRead(ENC_CLK);
  int dt = digitalRead(ENC_DT);
  if (clk != lastClkVal) {
    encDelta += (dt != clk) ? 1 : -1;
    lastClkVal = clk;
  }
}

// ─── ENCODER TIKLA (Kısa + Uzun Basma) ───────────────
void checkEncClick() {
  bool sw = digitalRead(ENC_SW);
  // Debounce: pin değiştiyse zamanlayıcıyı sıfırla
  if (sw != encSwPrev) {
    encSwChangeTime = millis();
    encSwPrev = sw;
  }
  // Kararlı durum kontrolü (30ms boyunca aynı kaldıysa)
  if (sw != encSwStable && (millis() - encSwChangeTime > ENC_SW_STABLE_MS)) {
    bool prevStable = encSwStable;
    encSwStable = sw;
    if (sw == LOW && prevStable == HIGH) {
      // Basıldı → zamanı kaydet
      encPressStart = millis();
      encLongFired = false;
      lastActivity = millis();
    } else if (sw == HIGH && prevStable == LOW) {
      // Bırakıldı → kısa basma mı?
      if (!encLongFired) {
        encClicked = true;
        lastActivity = millis();
        Serial.println("[ENC] SHORT click");
      }
    }
  }
  // Basılı tutulurken uzun basma kontrolü
  if (encSwStable == LOW && !encLongFired &&
      (millis() - encPressStart > ENC_LONG_PRESS_MS)) {
    encLongPress = true;
    encLongFired = true;
    lastActivity = millis();
    Serial.println("[ENC] LONG press -> BACK");
  }
}

// ─── BUTON DEBOUNCE ───────────────────────────────────
void readBtns() {
  for (int i = 0; i < 4; i++) {
    bool s = digitalRead(btns[i].pin);
    if (s != btns[i].lastState) {
      if (millis() - btns[i].lastMs < DEBOUNCE_MS) continue;  // debounce
      btns[i].lastMs = millis();
      btns[i].lastState = s;

      if (s == LOW) {
        if (i == 0) { // HOME butonu
          homePressStart = millis();
          homeLongFired = false;
        } else {
          btns[i].fired = true; // Diğer butonlar eskisi gibi çalışır
        }
      } else if (s == HIGH && i == 0) {
        // HOME butonu bırakıldı
        if (!homeLongFired) {
          btns[0].fired = true; // Uzun basılmadıysa kısa tık olarak kaydet
        }
      }
    }

    // HOME butonuna basılı tutuluyorsa ve 1.5 saniyeyi geçtiyse uykuya geç
    if (i == 0 && s == LOW && !homeLongFired &&
        (millis() - homePressStart > HOME_LONG_PRESS_MS)) {
      homeLongFired = true;
      Serial.println("[BTN] HOME long press -> DEEP SLEEP");
      enterDeepSleep();  // config.h'de forward declare edilmiş
    }
  }
}
