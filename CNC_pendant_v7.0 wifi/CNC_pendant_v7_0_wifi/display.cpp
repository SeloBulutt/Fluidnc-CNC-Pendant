/**
 * display.cpp — Tüm TFT ekran çizim fonksiyonları
 * #4  Non-blocking popup
 * #8  Float epsilon karşılaştırma
 * #9  Pil okuması önbellekleme (8x ortalama)
 * #10 Flicker azaltma (partial update)
 * #15 Override gauge birleştirme
 * #18 Z Probe ekranı
 */
#include "display.h"
#include "wifi_mgr.h"  // wifiConnected, tcpConnected, wifiIP, scan vb.
#include "fluidnc.h"   // uartActive

// ═══════════════════════════════════════════════════════
// ─── POPUP (#4 Non-blocking) ─────────────────────────
// ═══════════════════════════════════════════════════════
PopupState popupState = {false, 0, 0};

void showPopup(const char *msg, uint16_t bgCol, uint32_t dur) {
  tft.fillRoundRect(30, 60, 260, 50, 8, bgCol);
  tft.setTextSize(2);
  tft.setTextColor(C_BG);
  int tx = 30 + (260 - (int)strlen(msg) * 12) / 2;
  tft.setCursor(tx < 34 ? 34 : tx, 75);
  tft.print(msg);
  popupState = {true, millis(), dur};
}

bool updatePopup() {
  if (popupState.active && millis() - popupState.startMs > popupState.durMs) {
    popupState.active = false;
    needRedraw = true;
    return true;  // popup bitti, ekran yeniden çizilecek
  }
  return false;
}

// ═══════════════════════════════════════════════════════
// ─── BATTERY (#9 Önbellekleme + Gürültü filtresi) ───
// ═══════════════════════════════════════════════════════
int cachedBatPct = 100;
static unsigned long lastBatRead = 0;

void updateBattery() {
  // Jog veya buton hareketi varken (pil yük altındayken) ölçüm yapma
  if (millis() - lastActivity < 4000) return;
  if (millis() - lastBatRead > BAT_READ_INTERVAL) {
    lastBatRead = millis();
    long sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
      sum += analogRead(BAT_PIN);
      delayMicroseconds(100);
    }
    float voltage = (sum / (float)BAT_SAMPLES / 4095.0f) * 3.3f * BAT_R_RATIO;
    int newPct = constrain((int)((voltage - 3.0f) / 1.2f * 100.0f), 0, 100);
    static float filtPct = -1.0f;
    if (filtPct < 0) filtPct = newPct;
    else filtPct = filtPct * 0.8f + newPct * 0.2f;
    cachedBatPct = (int)(filtPct + 0.5f);
  }
}

// ═══════════════════════════════════════════════════════
// ─── DURUM YARDIMCILARI (#13 Enum kullanımı) ────────
// ═══════════════════════════════════════════════════════

uint16_t stateColor(uint8_t s) {
  switch (s) {
    case ST_IDLE:  return C_GREEN;
    case ST_RUN:   return C_CYAN;
    case ST_HOLD:  return C_YELLOW;
    case ST_ALARM: return C_RED;
    case ST_HOME:  return C_ORANGE;
    case ST_JOG:   return C_CYAN;
    case ST_DOOR:  return C_RED;
    default:       return C_GRAY;
  }
}

const char *stateStr(uint8_t s) {
  switch (s) {
    case ST_IDLE:  return "IDLE ";
    case ST_RUN:   return "RUN  ";
    case ST_HOLD:  return "HOLD ";
    case ST_ALARM: return "ALARM";
    case ST_HOME:  return "HOME ";
    case ST_JOG:   return "JOG  ";
    case ST_DOOR:  return "DOOR ";
    default:       return "?????";
  }
}

// ─── MENÜ ETİKETLERİ ─────────────────────────────────
static const char *MENU_LABELS[] = {
  "Spindle Kontrolu", "Jog Hizi", "Step Boyutu",
  "Sogutma", "Sifira Git", "Z Probe",
  "SD Kart", "WiFi Ayarlari"
};
static const char *MENU_RUN_LABELS[] = {
  "Is Durumu", "Spindle Override", "Feed Override", "Jog Hizi",
  "Step Boyutu", "Sogutma"
};
static const char *WIFI_MENU_LABELS[] = {
  "Ag Tara", "Sifre Gir", "Oto IP Bul",
  "Baglan / Kes", "Parlaklik", "Geri Don"
};

int getMenuCount() {
  bool runMode = (cur.state == ST_RUN || cur.state == ST_HOLD);
  return runMode ? MENU_RUN_COUNT : MENU_IDLE_COUNT;
}

// ═══════════════════════════════════════════════════════
// ─── ANA EKRAN ────────────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawHeader() {
  tft.fillRect(0, 0, TFT_W, 19, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(stateColor(cur.state));
  tft.setCursor(4, 5);
  tft.print("[");
  if (cur.state == ST_ALARM && cur.alarmCode > 0) {
    char alarmBuf[12];
    snprintf(alarmBuf, sizeof(alarmBuf), "ALARM:%d", cur.alarmCode);
    tft.print(alarmBuf);
  } else {
    tft.print(stateStr(cur.state));
  }
  tft.print("]");
  tft.setTextColor(C_CYAN);
  tft.setCursor(70, 5);
  tft.print("CNC PENDANT");
  tft.setTextColor(cur.homed ? C_GREEN : C_RED);
  tft.setCursor(160, 5);
  tft.print(cur.homed ? "[HOMED]" : "[NO HOME]");

  // Pil Durumu (#9 cached)
  tft.setCursor(227, 5);
  if (cachedBatPct > 20)      tft.setTextColor(C_GREEN);
  else if (cachedBatPct > 5)  tft.setTextColor(C_YELLOW);
  else                        tft.setTextColor(C_RED);
  tft.print("["); tft.print(cachedBatPct); tft.print("%]");

  // Bağlantı durumu ikonu (UART öncelikli)
  tft.setCursor(260, 5);
  if (uartActive) {
    tft.setTextColor(C_GREEN);
    tft.print("[UART]");
  } else if (tcpConnected) {
    tft.setTextColor(C_GREEN);
    tft.print("[WiFi]");
  } else if (wifiConnected) {
    tft.setTextColor(C_YELLOW);
    tft.print("[WiFi]");
  } else {
    tft.setTextColor(C_RED);
    tft.print("[]");
  }
  tft.fillRect(296, 1, 22, 17, C_ROWHL);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(301, 5);
  tft.print(AXIS_STR[selAxis]);
}

void drawAxisRows() {
  const float mpos[3] = {cur.x, cur.y, cur.z};
  const float wpos[3] = {cur.x - cur.wco_x, cur.y - cur.wco_y,
                          cur.z - cur.wco_z};
  char buf[12];
  for (int i = 0; i < 3; i++) {
    int y0 = 30 + i * 38;
    bool active = (i == (int)selAxis);
    tft.fillRect(0, y0, TFT_W, 38, active ? C_ROWHL : C_BG);
    tft.setTextSize(2);
    tft.setTextColor(active ? C_YELLOW : C_GRAY);
    tft.setCursor(2, y0 + 11);
    tft.print(AXIS_STR[i]);
    snprintf(buf, sizeof(buf), "%+8.3f", mpos[i]);
    tft.setTextColor(active ? C_YELLOW : C_WHITE);
    tft.setCursor(22, y0 + 11);
    tft.print(buf);
    tft.drawFastVLine(158, y0 + 1, 36, C_GRAY);
    snprintf(buf, sizeof(buf), "%+8.3f", wpos[i]);
    tft.setTextColor(active ? C_YELLOW : C_GREEN);
    tft.setCursor(162, y0 + 11);
    tft.print(buf);
  }
}

void updateFooterProgress() {
  int y0 = 144;
  int barW = (int)(126.0f * (cur.sdPercent / 100.0f));
  if (barW < 0) barW = 0;
  if (barW > 126) barW = 126;
  uint16_t col = (cur.state == ST_HOLD) ? C_YELLOW : C_CYAN;
  if (barW > 0) tft.fillRect(6, y0 + 17, barW, 5, col);
  if (barW < 126) tft.fillRect(6 + barW, y0 + 17, 126 - barW, 5, C_PANEL);

  tft.setCursor(138, y0 + 16);
  tft.setTextSize(1);
  tft.setTextColor(col, C_PANEL);
  char pb[28];
  snprintf(pb, sizeof(pb), "%%%5.1f L:%-5u %-4s", cur.sdPercent, cur.sdLine, (cur.state == ST_HOLD ? "HOLD" : "RUN"));
  tft.print(pb);
  tft.setCursor(256, y0 + 16);
  tft.setTextColor(C_GRAY, C_PANEL);
  tft.print("[ENC]Menu");
}

void drawFooter() {
  int y0 = 144;
  tft.fillRect(0, y0, TFT_W, TFT_H - y0, C_PANEL);
  tft.drawFastHLine(0, y0, TFT_W, C_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(C_ORANGE);
  tft.setCursor(4, y0 + 5);
  tft.print("F:");
  char fb[12];
  snprintf(fb, sizeof(fb), "%.0f", cur.feed);
  tft.print(fb);
  // Override yüzdesi göster (%100 değilse)
  if (cur.feedOv != 100) {
    char ovBuf[8];
    snprintf(ovBuf, sizeof(ovBuf), "[%d%%]", cur.feedOv);
    tft.setTextColor(C_YELLOW);
    tft.print(ovBuf);
  }
  tft.setTextColor(C_GREEN);
  tft.setCursor(85, y0 + 5);
  tft.print("S:");
  char sb[12];
  snprintf(sb, sizeof(sb), "%.0f", cur.spindle);
  tft.print(sb);
  if (cur.spindleOv != 100) {
    char ovBuf[8];
    snprintf(ovBuf, sizeof(ovBuf), "[%d%%]", cur.spindleOv);
    tft.setTextColor(C_YELLOW);
    tft.print(ovBuf);
  }
  tft.setTextColor(C_CYAN);
  tft.setCursor(165, y0 + 5);
  tft.print("STEP:");
  tft.print(STEP_STR[selStep]);
  // FluidNC IP adresi
  tft.setCursor(240, y0 + 5);
  tft.setTextColor(C_ORANGE);
  char fncIP[22];
  snprintf(fncIP, sizeof(fncIP), "%d.%d.%d.%d",
           wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
  tft.print(fncIP);
  tft.setTextColor(C_GRAY);
  tft.setCursor(4, y0 + 19);
  // Duruma göre bağlamsal yardım metni (#13 enum)
  if (cur.state == ST_ALARM) {
    tft.setTextColor(C_RED);
    if (cur.alarmCode > 0) {
      char almBuf[40];
      snprintf(almBuf, sizeof(almBuf), "A%d: %s", cur.alarmCode, getAlarmMsg(cur.alarmCode));
      tft.print(almBuf);
    } else {
      tft.print("[ZERO] Reset  [HOME] Kilit Ac");
    }
  } else if (cur.state == ST_HOLD || cur.state == ST_RUN) {
    tft.drawRoundRect(4, y0 + 16, 130, 7, 2, C_GRAY);
    updateFooterProgress();
  } else if (cur.state == ST_JOG) {
    tft.setTextColor(C_CYAN);
    tft.setCursor(4, y0 + 18);
    tft.print("[SPEED] Duraklat");
  } else {
    tft.setTextColor(C_GRAY);
    tft.setCursor(4, y0 + 18);
    tft.print("[HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu");
  }
}

void drawMainAll() {
  tft.fillScreen(C_BG);
  tft.drawFastHLine(0, 19, TFT_W, C_GRAY);
  tft.drawFastHLine(0, 144, TFT_W, C_GRAY);
  drawHeader();
  tft.fillRect(0, 19, TFT_W, 11, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(40, 21);
  tft.print("  MACHINE POS");
  tft.setCursor(185, 21);
  tft.print("   WORK POS");
  tft.drawFastHLine(0, 30, TFT_W, C_GRAY);
  tft.drawFastVLine(158, 19, 125, C_GRAY);
  drawAxisRows();
  drawFooter();
}

// #8 Float epsilon + #10 Partial update (flicker azaltma)
void updateMainDisplay() {
  if (popupState.active) return;
  if (needRedraw) {
    drawMainAll();
    needRedraw = false;
    prev = cur;
    return;
  }
  // #8 Float epsilon karşılaştırma
  bool posChg = fabsf(cur.x - prev.x) > POS_EPSILON ||
                fabsf(cur.y - prev.y) > POS_EPSILON ||
                fabsf(cur.z - prev.z) > POS_EPSILON ||
                fabsf(cur.wco_x - prev.wco_x) > POS_EPSILON ||
                fabsf(cur.wco_y - prev.wco_y) > POS_EPSILON ||
                fabsf(cur.wco_z - prev.wco_z) > POS_EPSILON;
  bool stChg = cur.state != prev.state || cur.homed != prev.homed;
  bool fdChg = fabsf(cur.feed - prev.feed) > 0.5f ||
               fabsf(cur.spindle - prev.spindle) > 0.5f ||
               cur.feedOv != prev.feedOv ||
               cur.spindleOv != prev.spindleOv;
  // #10 Sadece değişen bölgeleri güncelle (flicker azaltma)
  if (posChg) drawAxisRows();
  if (stChg) {
    drawHeader();
    drawFooter();
    needRedraw = true;
  }
  if (fdChg) drawFooter();
  bool sdChg = fabsf(cur.sdPercent - prev.sdPercent) > 0.05f || (cur.sdLine != prev.sdLine);
  if (sdChg && (cur.state == ST_RUN || cur.state == ST_HOLD)) {
    updateFooterProgress();
  }
  prev = cur;
}

// ═══════════════════════════════════════════════════════
// ─── MENÜ EKRANI ──────────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawMenuScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(80, 3);
  tft.print("CNC MENU");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  bool runMode = (cur.state == ST_RUN || cur.state == ST_HOLD);
  int count = runMode ? MENU_RUN_COUNT : MENU_IDLE_COUNT;

  // Sayfa / Indeks gostergesi (Orn: 1/8)
  char idxBuf[10];
  snprintf(idxBuf, sizeof(idxBuf), "%d/%d", menuIdx + 1, count);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(275, 7);
  tft.print(idxBuf);

  // Ekranda ayni anda 5 oge gosterilir (kaydirma penceresi)
  const int VISIBLE_ITEMS = 5;
  static int menuTopIdx = 0;
  if (count <= VISIBLE_ITEMS) {
    menuTopIdx = 0;
  } else {
    if (menuIdx < menuTopIdx) {
      menuTopIdx = menuIdx;
    } else if (menuIdx >= menuTopIdx + VISIBLE_ITEMS) {
      menuTopIdx = menuIdx - VISIBLE_ITEMS + 1;
    }
  }

  int itemH = 24;
  int numToShow = (count < VISIBLE_ITEMS) ? count : VISIBLE_ITEMS;
  for (int vi = 0; vi < numToShow; vi++) {
    int idx = menuTopIdx + vi;
    if (idx >= count) break;
    int y = 26 + vi * itemH;
    bool sel = (idx == menuIdx);
    if (sel) tft.fillRoundRect(6, y, 300, itemH - 2, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 22 : 14, y + 4);
    if (sel) tft.print("> ");
    tft.print(runMode ? MENU_RUN_LABELS[idx] : MENU_LABELS[idx]);
  }

  // Dikey kaydirma cubugu (scrollbar)
  if (count > VISIBLE_ITEMS) {
    int barX = 312;
    int barY = 26;
    int barH = 118;
    int barW = 4;
    tft.fillRoundRect(barX, barY, barW, barH, 2, C_PANEL);
    int thumbH = barH * VISIBLE_ITEMS / count;
    int maxTop = count - VISIBLE_ITEMS;
    int thumbY = barY + (barH - thumbH) * menuTopIdx / maxTop;
    tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, C_CYAN);
  }

  // Alt bilgi cubugu
  tft.drawFastHLine(0, 150, TFT_W, C_GRAY);
  tft.fillRect(0, 151, TFT_W, 19, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(30, 156);
  tft.print("Encoder: Sec  |  Tikla: Gir  |  Uzun: Geri");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── GAUGE ÇİZİM YARDIMCILARI ───────────────────────
// ═══════════════════════════════════════════════════════

static void drawThickArc(int cx, int cy, int r, float startDeg,
                         float sweepDeg, int thick, uint16_t color) {
  int ht = thick / 2;
  for (float a = 0; a <= sweepDeg; a += 2.5f) {
    float rad = (startDeg + a) * PI / 180.0f;
    int x = cx + (int)(r * cosf(rad));
    int y = cy + (int)(r * sinf(rad));
    tft.fillCircle(x, y, ht, color);
  }
}

static void drawGaugeTick(int cx, int cy, int r, float deg,
                          int len, uint16_t col) {
  float rad = deg * PI / 180.0f;
  float cs = cosf(rad), sn = sinf(rad);
  tft.drawLine(cx + (int)((r - len) * cs), cy + (int)((r - len) * sn),
               cx + (int)((r + 2) * cs), cy + (int)((r + 2) * sn), col);
}

// ═══════════════════════════════════════════════════════
// ─── SPINDLE GAUGE EKRANI ────────────────────────────
// ═══════════════════════════════════════════════════════

void drawSpindleScreen() {
  tft.fillScreen(C_BG);
  // Header
  tft.fillRect(0, 0, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 5);
  tft.print("SPINDLE KONTROLU");
  bool running = (cur.spindle > 0);
  tft.setTextColor(running ? C_GREEN : C_RED);
  tft.setCursor(240, 5);
  tft.print(running ? "[M3 AKTIF]" : "[M5 KAPALI]");

  int cx = 120, cy = 92, r = 52;
  // Arka plan ark (270°, alt açık)
  drawThickArc(cx, cy, r, 135.0f, 270.0f, 10, C_GRAY);
  // Değer arkı
  float valSweep = (cur.spindle / 20000.0f) * 270.0f;
  valSweep = constrain(valSweep, 0.0f, 270.0f);
  uint16_t arcCol = (cur.spindle < 7000)    ? C_GREEN
                  : (cur.spindle < 14000)   ? C_YELLOW
                                            : C_RED;
  if (valSweep > 0) drawThickArc(cx, cy, r, 135.0f, valSweep, 10, arcCol);
  // Tick işaretleri (0, 5K, 10K, 15K, 20K)
  for (int i = 0; i <= 4; i++) {
    float ta = 135.0f + i * 67.5f;
    drawGaugeTick(cx, cy, r + 4, ta, 10, C_WHITE);
  }
  // Tick etiketleri
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(cx - r - 18, cy + r - 8); tft.print("0");
  tft.setCursor(cx - r - 18, cy - 20);    tft.print("5K");
  tft.setCursor(cx - 12, cy - r - 14);    tft.print("10K");
  tft.setCursor(cx + r + 4, cy - 20);     tft.print("15K");
  tft.setCursor(cx + r + 4, cy + r - 8);  tft.print("20K");

  // RPM ortada
  char rpm[8];
  snprintf(rpm, sizeof(rpm), "%.0f", cur.spindle);
  tft.setTextSize(3); tft.setTextColor(C_WHITE);
  int tw = strlen(rpm) * 18;
  tft.setCursor(cx - tw / 2, cy - 12);
  tft.print(rpm);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(cx - 9, cy + 12);
  tft.print("RPM");

  // Sağ panel: Hedef hız
  int px = 210;
  tft.drawFastVLine(200, 20, 130, C_GRAY);
  tft.setTextSize(1); tft.setTextColor(C_ORANGE);
  tft.setCursor(px, 28);
  tft.print("HEDEF HIZ:");
  char tb[8];
  snprintf(tb, sizeof(tb), "%d", spindleTarget);
  tft.setTextSize(3); tft.setTextColor(C_YELLOW);
  int ttw = strlen(tb) * 18;
  tft.setCursor(px + (110 - ttw) / 2, 48);
  tft.print(tb);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(px + 35, 78);
  tft.print("RPM");

  // Durum göstergesi
  tft.setTextSize(2);
  tft.setTextColor(running ? C_GREEN : C_RED);
  tft.setCursor(px + 10, 100);
  tft.print(running ? "CALISIYOR" : " KAPALI ");

  // Alt bar
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(20, 157);
  tft.print("Cevir: Hiz Ayarla  |  Tikla: M3 Gonder");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── OVERRIDE GAUGE (#15 Birleştirilmiş) ────────────
// ═══════════════════════════════════════════════════════

void drawOverrideGauge(const char *title, const char *label,
                       uint8_t value, bool isRunning) {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 5);
  tft.print(title);
  tft.setTextColor(isRunning ? C_GREEN : C_YELLOW);
  tft.setCursor(250, 5);
  tft.print(isRunning ? "[RUN]" : "[HOLD]");

  int cx = 160, cy = 88, r = 58;
  drawThickArc(cx, cy, r, 135.0f, 270.0f, 10, C_GRAY);
  float valSweep = (value / 200.0f) * 270.0f;
  valSweep = constrain(valSweep, 0.0f, 270.0f);
  uint16_t arcCol;
  if (value < 80)        arcCol = C_RED;
  else if (value < 100)  arcCol = C_ORANGE;
  else if (value == 100) arcCol = C_GREEN;
  else if (value <= 150) arcCol = C_YELLOW;
  else                   arcCol = C_RED;
  if (valSweep > 0)
    drawThickArc(cx, cy, r, 135.0f, valSweep, 10, arcCol);
  for (int i = 0; i <= 4; i++) {
    float ta = 135.0f + i * 67.5f;
    drawGaugeTick(cx, cy, r + 4, ta, 10, C_WHITE);
  }
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(cx - r - 22, cy + r - 8); tft.print("0%");
  tft.setCursor(cx - r - 22, cy - 20);    tft.print("50%");
  tft.setCursor(cx - 15, cy - r - 14);    tft.print("100%");
  tft.setCursor(cx + r + 6, cy - 20);     tft.print("150%");
  tft.setCursor(cx + r + 6, cy + r - 8);  tft.print("200%");

  // Ortada büyük yüzde değeri
  char pctStr[8];
  snprintf(pctStr, sizeof(pctStr), "%d%%", value);
  tft.setTextSize(3); tft.setTextColor(C_WHITE);
  int tw = strlen(pctStr) * 18;
  tft.setCursor(cx - tw / 2, cy - 14);
  tft.print(pctStr);
  tft.setTextSize(1); tft.setTextColor(C_GREEN);
  int labelLen = strlen(label);
  tft.setCursor(cx - labelLen * 3, cy + 14);
  tft.print(label);

  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(15, 157);
  tft.print("Cevir: +-10% Aninda  |  Tikla: %100 Reset");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── OVERRIDE SEÇİM EKRANI ─────────────────────────
// ═══════════════════════════════════════════════════════

void drawOverrideSelScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(50, 3);
  tft.print("OVERRIDE SECIMI");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  const char *ovLabels[2] = {"Feed Ovr.", "Spindle Ovr."};
  int ovVals[2] = {(int)cur.feedOv, (int)cur.spindleOv};
  for (int i = 0; i < 2; i++) {
    int y = 50 + i * 32;
    bool sel = (i == (int)ovSelIdx);
    if (sel) tft.fillRoundRect(30, y, 260, 28, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 6);
    if (sel) tft.print("> ");
    tft.print(ovLabels[i]);
    char pct[8];
    snprintf(pct, sizeof(pct), "[%d%%]", ovVals[i]);
    tft.setTextColor(ovVals[i] == 100 ? C_GREEN : C_YELLOW);
    tft.setCursor(240, y + 6);
    tft.print(pct);
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(40, 157);
  tft.print("Encoder: Sec  |  Tikla: Onayla");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── JOG HIZI EKRANI ─────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawJogScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(70, 3);
  tft.print("JOG HIZI");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  for (int i = 0; i < N_FEEDS; i++) {
    int y = 28 + i * 24;
    bool sel = (i == (int)selFeed);
    if (sel) tft.fillRoundRect(30, y, 260, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 3);
    if (sel) tft.print("> ");
    tft.print(FEED_STR[i]);
    tft.print(" mm/dk");
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Cevir: Sec  |  Tikla: Onayla");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── STEP BOYUTU EKRANI ──────────────────────────────
// ═══════════════════════════════════════════════════════

void drawStepScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(60, 3);
  tft.print("STEP BOYUTU");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  for (int i = 0; i < N_STEPS; i++) {
    int y = 28 + i * 24;
    bool sel = (i == (int)selStep);
    if (sel) tft.fillRoundRect(30, y, 260, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 3);
    if (sel) tft.print("> ");
    tft.print(STEP_STR[i]);
    tft.print(" mm");
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Cevir: Sec  |  Tikla: Onayla");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── SOĞUTMA EKRANI ──────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawCoolantScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(80, 3);
  tft.print("SOGUTMA");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  for (int i = 0; i < 3; i++) {
    int y = 40 + i * 30;
    bool sel = (i == (int)coolantSel);
    if (sel) tft.fillRoundRect(30, y, 260, 26, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 5);
    if (sel) tft.print("> ");
    tft.print(COOL_STR[i]);
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(40, 157);
  tft.print("Cevir: Sec  |  Tikla: Gonder & Geri");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── WIFI MENÜ EKRANI ────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawWifiMenuScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(60, 3);
  tft.print("WIFI AYARLARI");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  // Bağlantı durumu sağ üstte
  tft.setTextSize(1);
  tft.setTextColor(wifiConnected ? C_GREEN : C_RED);
  tft.setCursor(260, 6);
  tft.print(wifiConnected ? "[ON]" : "[OFF]");

  for (int i = 0; i < WIFI_MENU_COUNT; i++) {
    int y = 26 + i * 20;
    bool sel = (i == wifiMenuIdx);
    if (sel) tft.fillRoundRect(8, y, 304, 18, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 24 : 16, y + 1);
    if (sel) tft.print("> ");
    tft.print(WIFI_MENU_LABELS[i]);
  }

  tft.fillRect(0, 150, TFT_W, 20, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(20, 155);
  if (wifiSSID.length() > 0) {
    tft.print("AG: ");
    tft.setTextColor(C_GREEN);
    tft.print(wifiSSID);
  } else {
    tft.print("Henuz ag secilmedi");
  }
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── WIFI TARAMA EKRANI ─────────────────────────────
// ═══════════════════════════════════════════════════════

void drawWifiScanScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 20, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 6);
  tft.print("WIFI AG TARA");

  // #6 Async scan: taranıyorsa göster
  if (wifiScanRunning) {
    tft.setTextSize(2); tft.setTextColor(C_YELLOW);
    tft.setCursor(60, 70);
    tft.print("Taraniyor...");
  } else {
    tft.setTextColor(C_GRAY);
    tft.setCursor(200, 6);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d ag bulundu", scanCount);
    tft.print(cnt);
    tft.drawFastHLine(0, 20, TFT_W, C_GRAY);

    if (scanCount == 0) {
      tft.setTextSize(2); tft.setTextColor(C_RED);
      tft.setCursor(60, 70);
      tft.print("Ag bulunamadi!");
    } else {
      for (int i = 0; i < scanCount; i++) {
        int y = 24 + i * 16;
        bool sel = (i == wifiScanIdx);
        if (sel) tft.fillRect(0, y, TFT_W, 16, C_ROWHL);
        tft.setTextSize(1);
        tft.setTextColor(sel ? C_YELLOW : C_WHITE);
        tft.setCursor(4, y + 4);
        if (sel) tft.print("> ");
        // SSID (max 20 karakter)
        String ssidDisp = scanSSIDs[i].substring(0, 20);
        tft.print(ssidDisp);
        // Sinyal gücü
        tft.setCursor(240, y + 4);
        tft.setTextColor(scanRSSI[i] > -65 ? C_GREEN
                       : (scanRSSI[i] > -80 ? C_YELLOW : C_RED));
        tft.print(rssiIcon(scanRSSI[i]));
        // dBm
        char db[8];
        snprintf(db, sizeof(db), "%ddB", (int)scanRSSI[i]);
        tft.setCursor(280, y + 4);
        tft.setTextColor(C_GRAY);
        tft.print(db);
      }
    }
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(30, 157);
  tft.print("Cevir: Sec  |  Tikla: Ag Sec  |  Uzun: Geri");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── WIFI ŞİFRE GİRİŞ EKRANI ───────────────────────
// ═══════════════════════════════════════════════════════

void drawWifiPassScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 20, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 6);
  tft.print("SIFRE GIR");
  tft.setTextColor(C_ORANGE);
  tft.setCursor(160, 6);
  tft.print("AG: ");
  tft.print(wifiSSID.substring(0, 14));
  tft.drawFastHLine(0, 20, TFT_W, C_GRAY);

  // Girilen şifre
  tft.setTextSize(1); tft.setTextColor(C_WHITE);
  tft.setCursor(4, 28);
  tft.print("Sifre: ");
  // Şifreyi maskeli göster (son 3 karakter açık)
  int pLen = passBuffer.length();
  for (int i = 0; i < pLen; i++) {
    if (i >= pLen - 3) tft.print(passBuffer.charAt(i));
    else tft.print('*');
  }
  tft.setTextColor(C_YELLOW);
  tft.print("_"); // cursor
  tft.setTextColor(C_GRAY);
  char lenBuf[10];
  snprintf(lenBuf, sizeof(lenBuf), " (%d/%d)", pLen, PASS_MAX_LEN);
  tft.print(lenBuf);

  // Aktif karakter (büyük gösterim)
  tft.drawRoundRect(100, 50, 120, 50, 6, C_CYAN);
  tft.setTextSize(4);
  tft.setTextColor(C_YELLOW);
  char ch[2] = {CHAR_SET[charIdx], 0};
  int chW = strlen(ch) * 24;
  tft.setCursor(160 - chW / 2, 60);
  tft.print(ch);

  // Karakter şeridi (önceki ve sonraki)
  tft.setTextSize(2);
  for (int offset = -5; offset <= 5; offset++) {
    if (offset == 0) continue;
    int ci = (charIdx + offset + CHAR_SET_LEN) % CHAR_SET_LEN;
    int xp = 160 + offset * 22;
    if (xp < 5 || xp > 310) continue;
    tft.setTextColor(abs(offset) <= 2 ? C_WHITE : C_GRAY);
    char cc[2] = {CHAR_SET[ci], 0};
    tft.setCursor(xp - 6, 110);
    tft.print(cc);
  }

  // Alt kısım
  tft.drawFastHLine(0, 135, TFT_W, C_GRAY);
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(10, 140);
  tft.print("Cevir: Karakter  Tikla: Ekle  Uzun: Sil");
  tft.setTextColor(C_GREEN);
  tft.setCursor(10, 157);
  tft.print("[AXIS] btn = ONAYLA & KAYDET");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── WIFI IP EKRANI ──────────────────────────────────
// ═══════════════════════════════════════════════════════

void drawWifiIPScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 20, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 6);
  tft.print("FLUIDNC IP ADRESI");
  tft.drawFastHLine(0, 20, TFT_W, C_GRAY);

  // IP adresi büyük gösterim
  for (int i = 0; i < 4; i++) {
    int xp = 30 + i * 75;
    bool active = (i == ipEditIdx);

    if (active) tft.fillRoundRect(xp - 5, 50, 65, 40, 6, C_ROWHL);
    else        tft.drawRoundRect(xp - 5, 50, 65, 40, 6, C_GRAY);

    tft.setTextSize(3);
    tft.setTextColor(active ? C_YELLOW : C_WHITE);
    char octet[4];
    snprintf(octet, sizeof(octet), "%3d", wifiIP[i]);
    tft.setCursor(xp, 58);
    tft.print(octet);

    // Nokta ayırıcı
    if (i < 3) {
      tft.setTextSize(3); tft.setTextColor(C_GRAY);
      tft.setCursor(xp + 58, 58);
      tft.print(".");
    }
  }

  // Altında ok göstergeleri
  int axp = 30 + ipEditIdx * 75;
  tft.setTextSize(2); tft.setTextColor(C_CYAN);
  tft.setCursor(axp + 15, 95);
  tft.print("^^");

  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(10, 140);
  tft.print("Cevir: Deger  Tikla: Sonraki Oktet");
  tft.setCursor(10, 157);
  tft.setTextColor(C_GREEN);
  tft.print("[AXIS] btn = KAYDET  |  Uzun bas = Geri");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── Z PROBE EKRANI (#18) ───────────────────────────
// ═══════════════════════════════════════════════════════

void drawProbeScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(80, 3);
  tft.print("Z PROBE");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  // Mevcut Z pozisyonu (WCS)
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(20, 24);
  tft.print("Mevcut Z: ");
  tft.setTextColor(C_WHITE);
  char zBuf[12];
  snprintf(zBuf, sizeof(zBuf), "%.3f mm", cur.z - cur.wco_z);
  tft.print(zBuf);

  // 1. Parametre: Derinlik (probeParamIdx == 0)
  int y1 = 34;
  bool selDepth = (probeParamIdx == 0);
  if (selDepth) tft.fillRoundRect(15, y1, 290, 22, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selDepth ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y1 + 3);
  if (selDepth) tft.print("> ");
  tft.print("Derinlik: ");
  char dBuf[10];
  snprintf(dBuf, sizeof(dBuf), "%d mm", probeDepth);
  tft.setTextColor(selDepth ? C_YELLOW : C_GREEN);
  tft.print(dBuf);

  // 2. Parametre: Hiz (probeParamIdx == 1)
  int y2 = 58;
  bool selFeedP = (probeParamIdx == 1);
  if (selFeedP) tft.fillRoundRect(15, y2, 290, 22, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selFeedP ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y2 + 3);
  if (selFeedP) tft.print("> ");
  tft.print("Hiz: ");
  char fBuf[14];
  snprintf(fBuf, sizeof(fBuf), "%d mm/dk", probeFeed);
  tft.setTextColor(selFeedP ? C_YELLOW : C_GREEN);
  tft.print(fBuf);

  // 3. Parametre: Geri Cekilme (probeParamIdx == 2)
  int y3 = 82;
  bool selRetract = (probeParamIdx == 2);
  if (selRetract) tft.fillRoundRect(15, y3, 290, 22, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selRetract ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y3 + 3);
  if (selRetract) tft.print("> ");
  tft.print("Geri Cek: ");
  char rBuf[10];
  snprintf(rBuf, sizeof(rBuf), "+%d mm", probeRetract);
  tft.setTextColor(selRetract ? C_YELLOW : C_GREEN);
  tft.print(rBuf);

  // 4. Parametre: Plaka Kalinligi (probeParamIdx == 3)
  int y4 = 106;
  bool selPlate = (probeParamIdx == 3);
  if (selPlate) tft.fillRoundRect(15, y4, 290, 22, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selPlate ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y4 + 3);
  if (selPlate) tft.print("> ");
  tft.print("Plaka: ");
  char pBuf[10];
  snprintf(pBuf, sizeof(pBuf), "%d mm", probePlate);
  tft.setTextColor(selPlate ? C_YELLOW : C_GREEN);
  tft.print(pBuf);

  // Islem Ozeti
  tft.setTextSize(1); tft.setTextColor(C_ORANGE);
  tft.setCursor(20, 134);
  tft.print("Islem: ");
  tft.setTextColor(C_WHITE);
  char infoBuf[48];
  snprintf(infoBuf, sizeof(infoBuf), "Dokun -> Z=%dmm -> Cekil (+%dmm)", probePlate, probeRetract);
  tft.print(infoBuf);
  // Yardım satırı
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  // Alt bar
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setCursor(10, 157);
  tft.setTextColor(C_GRAY);
  tft.print("Cevir:Deger Tikla:Param ");
  tft.setTextColor(C_GREEN);
  tft.print("[AXIS]=BASLAT");
  needRedraw = false;
}


// ====================================================
// --- SD KART DOSYA LISTESI (#21) ---
// ====================================================

void drawSdListScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(80, 3);
  tft.print("SD KART");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  // Dosya sayisi
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(230, 8);
  char cntBuf[12];
  snprintf(cntBuf, sizeof(cntBuf), "%d dosya", sdFileCount);
  tft.print(cntBuf);

  if (sdListPending) {
    tft.setTextSize(2); tft.setTextColor(C_YELLOW);
    tft.setCursor(60, 70);
    tft.print("Yukleniyor...");
  } else if (sdFileCount == 0) {
    tft.setTextSize(2); tft.setTextColor(C_RED);
    tft.setCursor(40, 70);
    tft.print("Dosya bulunamadi!");
  } else {
    for (int i = 0; i < sdFileCount; i++) {
      int y = 26 + i * 15;
      if (y > 140) break;
      bool sel = (i == sdSelIdx);
      if (sel) tft.fillRect(0, y, TFT_W, 15, C_ROWHL);
      tft.setTextSize(1);
      tft.setTextColor(sel ? C_YELLOW : C_WHITE);
      tft.setCursor(4, y + 3);
      if (sel) tft.print("> ");
      // Dosya adini max 35 karakter goster
      String fname = sdFiles[i].substring(0, 35);
      tft.print(fname);
    }
  }

  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(20, 157);
  tft.print("Cevir: Sec  |  Tikla: Onizle  |  Uzun: Geri");
  needRedraw = false;
}


// ====================================================
// ====================================================
// --- SD KART DOSYA ONIZLEME EKRANI ---
// ====================================================

void drawSdPreviewScreen() {
  tft.fillScreen(C_BG);

  // Ust panel (0..22)
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(10, 3);
  tft.print("DOSYA ONIZLEME");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  // Durum gostergesi (ust sag)
  tft.setTextSize(1);
  if (sdShowPending) {
    tft.setTextColor(C_YELLOW);
    tft.setCursor(225, 7);
    tft.print("Okunuyor...");
  } else {
    tft.setTextColor(C_GREEN);
    tft.setCursor(240, 7);
    char statBuf[16];
    snprintf(statBuf, sizeof(statBuf), "%d satir", sdPreviewLineCount);
    tft.print(statBuf);
  }

  // Dosya adi & Kaydirma satiri (y=26)
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(6, 26);
  tft.print("Dosya: ");
  tft.setTextColor(C_YELLOW);
  String displayFileName = sdSelectedFile;
  if (displayFileName.length() > 26) {
    displayFileName = displayFileName.substring(0, 26);
  }
  tft.print(displayFileName);

  // Satir araligi bilgisi (sag taraf)
  if (sdPreviewLineCount > 0) {
    tft.setTextColor(C_GRAY);
    tft.setCursor(235, 26);
    int endLine = sdPreviewScroll + 6;
    if (endLine > sdPreviewLineCount) endLine = sdPreviewLineCount;
    char rangeBuf[16];
    snprintf(rangeBuf, sizeof(rangeBuf), "%d-%d/%d", sdPreviewScroll + 1, endLine, sdPreviewLineCount);
    tft.print(rangeBuf);
  }

  // G-Code terminal kutusu (y=38..148, w=298, h=110)
  tft.fillRoundRect(4, 38, 298, 110, 4, 0x0842);
  tft.drawRoundRect(4, 38, 298, 110, 4, 0x31A6);

  // Dikey Scrollbar (x=306, y=38, w=10, h=110)
  tft.fillRoundRect(306, 38, 10, 110, 3, 0x18C3);
  if (sdPreviewLineCount > 6) {
    int thumbH = 110 * 6 / sdPreviewLineCount;
    if (thumbH < 15) thumbH = 15;
    int maxScroll = sdPreviewLineCount - 6;
    int thumbY = 38 + ((110 - thumbH) * sdPreviewScroll) / maxScroll;
    tft.fillRoundRect(307, thumbY, 8, thumbH, 3, C_CYAN);
  } else {
    tft.fillRoundRect(307, 39, 8, 108, 3, 0x31A6);
  }

  if (sdShowPending && sdPreviewLineCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(C_YELLOW);
    tft.setCursor(55, 82);
    tft.print("G-code Okunuyor...");
  } else if (!sdShowPending && sdPreviewLineCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(C_RED);
    tft.setCursor(35, 82);
    tft.print("G-code Okunamadi!");
  } else {
    // G-code satirlarini listele (sdPreviewScroll'dan itibaren 6 satir)
    tft.setTextSize(1);
    for (int i = 0; i < 6 && (sdPreviewScroll + i) < sdPreviewLineCount; i++) {
      int lineIdx = sdPreviewScroll + i;
      int lineY = 43 + i * 17;

      // Satir no
      tft.setTextColor(C_GRAY);
      tft.setCursor(8, lineY);
      char numBuf[8];
      snprintf(numBuf, sizeof(numBuf), "%d:", lineIdx + 1);
      tft.print(numBuf);

      // G-code metni
      tft.setTextColor(0x07E0);
      int textX = (lineIdx + 1 >= 100) ? 36 : ((lineIdx + 1 >= 10) ? 30 : 24);
      tft.setCursor(textX, lineY);
      tft.print(sdPreviewLines[lineIdx]);
    }
  }

  // Alt kontrol cubugu (y=152..170)
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.drawFastHLine(0, 151, TFT_W, C_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(C_GREEN);
  tft.setCursor(10, 156);
  tft.print("[AXIS] BASLAT");
  tft.setTextColor(C_GRAY);
  tft.print("  |  ");
  tft.setTextColor(C_CYAN);
  tft.print("[Encoder] KAYDIR");
  tft.setTextColor(C_GRAY);
  tft.print("  |  ");
  tft.setTextColor(C_ORANGE);
  tft.print("[HOME] GERI");

  needRedraw = false;
}

// ====================================================
// --- CANLI IS ILERLEME VE TAKIP EKRANI ---
// ====================================================
// --- CANLI IS ILERLEME VE TAKIP EKRANI ---
// ====================================================

static uint32_t lastDrawnJobLine = 0xFFFFFFFF;

void drawJobProgressGCodeLines(uint32_t activeLine) {
  // 4 satir G-code goster (y=72..142)
  uint32_t startLine = 1;
  if (activeLine > 2) {
    startLine = activeLine - 1;
  }

  for (int i = 0; i < 4; i++) {
    uint32_t lineNo = startLine + i;
    int lineY = 73 + i * 18;
    bool isActive = (lineNo == activeLine);

    // Satir arka plani
    if (isActive) {
      tft.fillRoundRect(6, lineY - 2, 308, 17, 3, C_ROWHL);
    } else {
      tft.fillRect(6, lineY - 2, 308, 17, 0x0842);
    }

    tft.setTextSize(1);
    char lineBuf[64];
    if (lineNo <= (uint32_t)sdPreviewLineCount && lineNo > 0) {
      snprintf(lineBuf, sizeof(lineBuf), "%s%3u: %s",
               isActive ? "> " : "  ",
               lineNo,
               sdPreviewLines[lineNo - 1].c_str());
    } else {
      if (isActive) {
        snprintf(lineBuf, sizeof(lineBuf), "> %3u: [G-Code Isleniyor...] (F:%.0f)", lineNo, cur.feed);
      } else {
        snprintf(lineBuf, sizeof(lineBuf), "  %3u: [G-Code]", lineNo);
      }
    }

    uint16_t rowBg = isActive ? C_ROWHL : 0x0842;
    uint16_t rowFg = isActive ? ((cur.state == ST_HOLD) ? C_YELLOW : C_GREEN) : C_GRAY;
    tft.setTextColor(rowFg, rowBg);
    tft.setCursor(8, lineY + 2);
    tft.print(lineBuf);
  }
}

void drawJobProgressScreen() {
  tft.fillScreen(C_BG);
  lastDrawnJobLine = 0xFFFFFFFF;

  bool isStopped = lastStoppedJob.valid && (cur.state == ST_IDLE);

  // 1. Ust panel (0..22) - Duruma gore renkli
  uint16_t hdrCol = (cur.state == ST_HOLD) ? 0x6300 : ((cur.state == ST_ALARM || isStopped) ? 0x7800 : 0x0320);
  tft.fillRect(0, 0, TFT_W, 22, hdrCol);
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);

  tft.setTextSize(2);
  if (cur.state == ST_RUN) {
    tft.setTextColor(C_GREEN, hdrCol);
    tft.setCursor(10, 3);
    tft.print("IS CALISIYOR");
  } else if (cur.state == ST_HOLD) {
    tft.setTextColor(C_YELLOW, hdrCol);
    tft.setCursor(10, 3);
    tft.print("DURAKLATILDI");
  } else if (cur.state == ST_ALARM) {
    tft.setTextColor(C_RED, hdrCol);
    tft.setCursor(10, 3);
    tft.print("ALARM DURUMU!");
  } else if (isStopped) {
    tft.setTextColor(C_RED, hdrCol);
    tft.setCursor(10, 3);
    tft.print("IS DURDURULDU");
  } else {
    tft.setTextColor(C_CYAN, hdrCol);
    tft.setCursor(10, 3);
    tft.print("IS TAKIBI");
  }

  // 2. Ilerleme cubugu cercevesi (y=25..36, w=308, h=12)
  tft.drawRoundRect(6, 25, 308, 12, 3, C_GRAY);

  // 3. Bilgi Seridi: Dosya Adi & Hizlar (y=39..54, h=15)
  tft.fillRect(4, 39, 312, 15, C_PANEL);
  tft.drawRect(4, 39, 312, 15, C_GRAY);
  tft.setTextSize(1);

  // Dosya adi & Feed & Spindle
  tft.setCursor(8, 43);
  tft.setTextColor(C_GRAY, C_PANEL);
  tft.print("Dosya: ");
  String fn = sdSelectedFile.length() > 0 ? sdSelectedFile : (lastStoppedJob.valid ? lastStoppedJob.fileName : "SD Kart");
  if (fn.length() > 18) fn = fn.substring(0, 18);
  tft.setTextColor(C_YELLOW, C_PANEL);
  tft.print(fn);

  tft.setCursor(160, 43);
  tft.setTextColor(C_GRAY, C_PANEL);
  tft.print("F:");
  char fb[16];
  snprintf(fb, sizeof(fb), "%-4.0f", cur.feed);
  tft.setTextColor(C_ORANGE, C_PANEL);
  tft.print(fb);
  if (cur.feedOv != 100) {
    char ov[8]; snprintf(ov, sizeof(ov), "[%d%%]", cur.feedOv);
    tft.setTextColor(C_YELLOW, C_PANEL);
    tft.print(ov);
  }

  tft.setCursor(240, 43);
  tft.setTextColor(C_GRAY, C_PANEL);
  tft.print("S:");
  char sb[16];
  snprintf(sb, sizeof(sb), "%-5.0f", cur.spindle);
  tft.setTextColor(C_GREEN, C_PANEL);
  tft.print(sb);

  // 4. Ana Kart (y=56..145, h=90): OKUNAN G-CODE SATIRLARI veya DURDURULAN NOKTA KARTI
  uint16_t boxBorder = (cur.state == ST_HOLD) ? C_YELLOW : ((cur.state == ST_ALARM || isStopped) ? C_RED : 0x31A6);
  tft.fillRect(4, 56, 312, 90, 0x0842);
  tft.drawRect(4, 56, 312, 90, boxBorder);

  if (cur.state == ST_ALARM || isStopped) {
    // --- DURDURULAN / ALARM KAYIT BILGISI KUTUSU ---
    tft.setTextSize(1);
    tft.setTextColor(boxBorder, 0x0842);
    tft.setCursor(8, 60);
    if (cur.state == ST_ALARM) {
      char aHdr[64];
      snprintf(aHdr, sizeof(aHdr), "[!] ALARM %d: %s (KAYDEDILDI)", cur.alarmCode, getAlarmMsg(cur.alarmCode));
      tft.print(aHdr);
    } else {
      tft.print("[!] IS TAMAMEN DURDURULDU (FLASH'A KAYDEDILDI):");
    }

    tft.setCursor(8, 75);
    tft.setTextColor(C_YELLOW, 0x0842);
    char sBuf[64];
    snprintf(sBuf, sizeof(sBuf), "Kaldigi Satir : #%u  (%%%0.1f)",
             lastStoppedJob.valid ? lastStoppedJob.line : cur.sdLine,
             lastStoppedJob.valid ? lastStoppedJob.percent : cur.sdPercent);
    tft.print(sBuf);

    tft.setCursor(8, 90);
    tft.setTextColor(C_WHITE, 0x0842);
    char gBuf[64];
    const char* gcText = lastStoppedJob.gcode[0] ? lastStoppedJob.gcode : (lastStoppedJob.line <= (uint32_t)sdPreviewLineCount && lastStoppedJob.line > 0 ? sdPreviewLines[lastStoppedJob.line - 1].c_str() : "[G-Code]");
    snprintf(gBuf, sizeof(gBuf), "Durdurulan Kod: %s", gcText);
    tft.print(gBuf);

    tft.setCursor(8, 105);
    tft.setTextColor(C_GREEN, 0x0842);
    char cBuf[64];
    snprintf(cBuf, sizeof(cBuf), "Konum (WPos)  : X:%.3f  Y:%.3f  Z:%.3f",
             lastStoppedJob.valid ? lastStoppedJob.x : (cur.x - cur.wco_x),
             lastStoppedJob.valid ? lastStoppedJob.y : (cur.y - cur.wco_y),
             lastStoppedJob.valid ? lastStoppedJob.z : (cur.z - cur.wco_z));
    tft.print(cBuf);

    tft.setTextColor(C_CYAN, 0x0842);
    tft.setCursor(8, 121);
    tft.print("-> Bu satir NVS hafizada saklandi.");

    tft.setTextColor(C_GRAY, 0x0842);
    tft.setCursor(8, 134);
    tft.print("Buradan baslatmak icin dosyayi bu satirdan acin.");
  } else {
    // --- OKUNAN G-CODE SATIRLARI ---
    tft.setTextSize(1);
    tft.setTextColor(C_CYAN, 0x0842);
    tft.setCursor(8, 60);
    tft.print("OKUNAN G-CODE SATIRLARI:");

    tft.setCursor(215, 60);
    tft.setTextColor(C_YELLOW, 0x0842);
    char curLineStr[20];
    snprintf(curLineStr, sizeof(curLineStr), "Aktif: #%-5u", cur.sdLine);
    tft.print(curLineStr);

    tft.drawFastHLine(6, 70, 308, 0x18C3);

    uint32_t activeLine = (cur.sdLine > 0) ? cur.sdLine : 1;
    drawJobProgressGCodeLines(activeLine);
    lastDrawnJobLine = activeLine;
  }

  // 5. Alt kontrol cubugu (y=148..170, h=22)
  tft.fillRect(0, 148, TFT_W, 22, C_PANEL);
  tft.drawFastHLine(0, 147, TFT_W, C_GRAY);
  tft.setTextSize(1);

  if (cur.state == ST_RUN) {
    tft.setCursor(15, 154);
    tft.setTextColor(C_YELLOW, C_PANEL);
    tft.print("[SPEED] Duraklat");

    tft.setCursor(215, 154);
    tft.setTextColor(C_CYAN, C_PANEL);
    tft.print("[HOME] Ana Menu");
  } else if (cur.state == ST_HOLD) {
    tft.setCursor(15, 154);
    tft.setTextColor(C_GREEN, C_PANEL);
    tft.print("[SPEED] Devam Et (~)");

    tft.setCursor(215, 154);
    tft.setTextColor(C_CYAN, C_PANEL);
    tft.print("[HOME] Ana Menu");
  } else {
    tft.setCursor(15, 154);
    tft.setTextColor(C_RED, C_PANEL);
    tft.print("[ZERO] Reset");

    tft.setCursor(215, 154);
    tft.setTextColor(C_CYAN, C_PANEL);
    tft.print("[HOME] Ana Menu");
  }

  // Ilk degerleri ciz
  updateJobProgressDisplay();
  needRedraw = false;
}

// ====================================================
// --- CANLI IS ILERLEME KISMI GUNCELLEME (FLICKER-FREE) ---
// ====================================================

void updateJobProgressDisplay() {
  static uint8_t lastJobState = 255;
  if (cur.state != lastJobState) {
    lastJobState = cur.state;
    needRedraw = true;
    return;
  }

  bool isStopped = lastStoppedJob.valid && (cur.state == ST_IDLE);
  uint16_t hdrCol = (cur.state == ST_HOLD) ? 0x6300 : ((cur.state == ST_ALARM || isStopped) ? 0x7800 : 0x0320);

  // 1. Sag ust yuzde & satir bilgisi
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE, hdrCol);
  tft.setCursor(205, 7);
  char pBuf[24];
  snprintf(pBuf, sizeof(pBuf), "%%%5.1f | L:%-5u", cur.sdPercent, cur.sdLine);
  tft.print(pBuf);

  // 2. Ilerleme cubugu dolgusu (Flicker-Free)
  int barW = (int)(304.0f * (cur.sdPercent / 100.0f));
  if (barW < 0) barW = 0;
  if (barW > 304) barW = 304;
  uint16_t barCol = (cur.state == ST_HOLD) ? C_YELLOW : ((cur.state == ST_ALARM || isStopped) ? C_RED : C_CYAN);
  if (barW > 0) tft.fillRect(8, 27, barW, 8, barCol);
  if (barW < 304) tft.fillRect(8 + barW, 27, 304 - barW, 8, C_BG);

  // 3. Hiz ve Spindle Degerleri
  tft.setTextSize(1);
  tft.setCursor(172, 43);
  char fb[16];
  snprintf(fb, sizeof(fb), "%-4.0f", cur.feed);
  tft.setTextColor(C_ORANGE, C_PANEL);
  tft.print(fb);

  tft.setCursor(252, 43);
  char sb[16];
  snprintf(sb, sizeof(sb), "%-5.0f", cur.spindle);
  tft.setTextColor(C_GREEN, C_PANEL);
  tft.print(sb);

  // 4. Okunan G-Code Satirlari (Sadece RUN veya HOLD iken satir degistiginde guncelle)
  if (cur.state == ST_RUN || cur.state == ST_HOLD) {
    uint32_t activeLine = (cur.sdLine > 0) ? cur.sdLine : 1;
    if (activeLine != lastDrawnJobLine) {
      lastDrawnJobLine = activeLine;

      // Ust basliktaki aktif satir no guncelle
      tft.setCursor(257, 60);
      tft.setTextColor(C_YELLOW, 0x0842);
      char curLineStr[10];
      snprintf(curLineStr, sizeof(curLineStr), "%-5u", activeLine);
      tft.print(curLineStr);

      // G-code satirlarini ciz
      drawJobProgressGCodeLines(activeLine);
    }
  }
}
