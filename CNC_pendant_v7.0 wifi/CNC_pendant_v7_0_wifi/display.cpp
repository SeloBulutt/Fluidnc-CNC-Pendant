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
  "Sogutma", "Z Probe", "WiFi Ayarlari"
};
static const char *MENU_RUN_LABELS[] = {
  "Spindle Override", "Feed Override", "Jog Hizi",
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
  tft.print("["); tft.print(stateStr(cur.state)); tft.print("]");
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
    tft.print("[ZERO] Reset  [HOME] Kilit Ac");
  } else if (cur.state == ST_HOLD) {
    tft.setTextColor(C_YELLOW);
    tft.print("[SPEED] Devam Et");
  } else if (cur.state == ST_RUN || cur.state == ST_JOG) {
    tft.setTextColor(C_CYAN);
    tft.print("[SPEED] Duraklat");
  } else {
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
  tft.setCursor(90, 3);
  tft.print("CNC MENU");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  bool runMode = (cur.state == ST_RUN || cur.state == ST_HOLD);
  int count = runMode ? MENU_RUN_COUNT : MENU_IDLE_COUNT;
  // Spacing: 5 item = 24px, 6 item = 20px
  int itemH = (count <= 5) ? 24 : 20;
  for (int i = 0; i < count; i++) {
    int y = 26 + i * itemH;
    bool sel = (i == menuIdx);
    if (sel) tft.fillRoundRect(8, y, 304, itemH - 2, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 24 : 16, y + 3);
    if (sel) tft.print("> ");
    tft.print(runMode ? MENU_RUN_LABELS[i] : MENU_LABELS[i]);
  }
  tft.drawFastHLine(0, 150, TFT_W, C_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Encoder: Sec  |  Tikla: Gir");
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
  tft.setCursor(20, 26);
  tft.print("Mevcut Z: ");
  tft.setTextColor(C_WHITE);
  char zBuf[12];
  snprintf(zBuf, sizeof(zBuf), "%.3f mm", cur.z - cur.wco_z);
  tft.print(zBuf);

  // 1. Parametre: Derinlik (probeParamIdx == 0)
  int y1 = 38;
  bool selDepth = (probeParamIdx == 0);
  if (selDepth) tft.fillRoundRect(15, y1, 290, 24, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selDepth ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y1 + 4);
  if (selDepth) tft.print("> ");
  tft.print("Derinlik: ");
  char dBuf[10];
  snprintf(dBuf, sizeof(dBuf), "%d mm", probeDepth);
  tft.setTextColor(selDepth ? C_YELLOW : C_GREEN);
  tft.print(dBuf);

  // 2. Parametre: Hız (probeParamIdx == 1)
  int y2 = 64;
  bool selFeedP = (probeParamIdx == 1);
  if (selFeedP) tft.fillRoundRect(15, y2, 290, 24, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selFeedP ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y2 + 4);
  if (selFeedP) tft.print("> ");
  tft.print("Hiz: ");
  char fBuf[14];
  snprintf(fBuf, sizeof(fBuf), "%d mm/dk", probeFeed);
  tft.setTextColor(selFeedP ? C_YELLOW : C_GREEN);
  tft.print(fBuf);

  // 3. Parametre: Geri Çekilme (probeParamIdx == 2)
  int y3 = 90;
  bool selRetract = (probeParamIdx == 2);
  if (selRetract) tft.fillRoundRect(15, y3, 290, 24, 4, C_ROWHL);
  tft.setTextSize(2);
  tft.setTextColor(selRetract ? C_YELLOW : C_WHITE);
  tft.setCursor(25, y3 + 4);
  if (selRetract) tft.print("> ");
  tft.print("Geri Cek: ");
  char rBuf[10];
  snprintf(rBuf, sizeof(rBuf), "+%d mm", probeRetract);
  tft.setTextColor(selRetract ? C_YELLOW : C_GREEN);
  tft.print(rBuf);

  // İşlem Özeti
  tft.setTextSize(1); tft.setTextColor(C_ORANGE);
  tft.setCursor(20, 118);
  tft.print("Islem: ");
  tft.setTextColor(C_WHITE);
  char infoBuf[48];
  snprintf(infoBuf, sizeof(infoBuf), "Dokun -> Sifirla (Z0) -> Cekil (+%dmm)", probeRetract);
  tft.print(infoBuf);

  // Yardım satırı
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(10, 134);
  tft.print("Cevir: Deger  |  Tikla: Parametre Sec");

  // Alt bar
  tft.fillRect(0, 148, TFT_W, 22, C_PANEL);
  tft.setCursor(10, 153);
  tft.setTextColor(C_GREEN);
  tft.print("[AXIS] = PROBE BASLAT  |  Uzun: Geri");
  needRedraw = false;
}
