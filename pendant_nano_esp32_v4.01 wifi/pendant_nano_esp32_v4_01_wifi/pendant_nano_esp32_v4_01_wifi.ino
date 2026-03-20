/**
 * =====================================================
 *  CNC Pendant — Arduino Nano ESP32
 *  v4.01 wifi — Wifi bağlantısı — Soft reset ve duraklama eklendi
 * =====================================================
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
 *   Encoder CLK→D2, DT→D3, SW→D12
 *   Butonlar HOME→D4, ZERO→D5, AXIS→A6, SPEED→A7
 *   TFT CS→D10, DC→D6, RST→D7
 *   UART RX1→D9, TX1→D8
 *
 * Alarm Durumunda:
 *   Home: Kilidi Aç
 *   Zero: Soft Reset
 *
 * Hareket Halinde:
 *   Speed: Duraklatma
 *
 *
 *
 * KÜTÜPHANEler: Adafruit ST7789, Adafruit GFX Library
 * =====================================================
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
// TCP client ile FluidNC Data port 23'e baglaniyoruz
#include <WiFi.h>

// ─── PIN TANIMLARI ────────────────────────────────────
#define ENC_CLK D2
#define ENC_DT D3
#define ENC_SW A0

#define BTN_HOME D4
#define BTN_ZERO D5
#define BTN_AXIS A6
#define BTN_SPEED A7

#define TFT_CS D10
#define TFT_DC D6
#define TFT_RST D7

// ─── TFT ──────────────────────────────────────────────
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
#define TFT_W 320
#define TFT_H 170

// ─── RENKLER ──────────────────────────────────────────
#define C_BG 0x0000
#define C_WHITE 0xFFFF
#define C_CYAN 0x07FF
#define C_GREEN 0x07E0
#define C_RED 0xF800
#define C_YELLOW 0xFFE0
#define C_ORANGE 0xFD20
#define C_GRAY 0x4208
#define C_PANEL 0x1082
#define C_ROWHL 0x0841
#define C_DKGREEN 0x03E0

// ─── FLUIDNC UART ─────────────────────────────────────
#define FLUIDNC Serial1
#define FC_BAUD 115200

// ─── MAKİNA DURUMU ────────────────────────────────────
struct Status {
  float x, y, z;
  float wco_x, wco_y, wco_z;
  float feed;
  float spindle;
  uint8_t state;
  bool homed;
};

Status cur = {0, 0, 0, 0, 0, 0, 0, 0, 0, false};
Status prev = {0, 0, 0, 0, 0, 0, 0, 0, 0, false};

// ─── PENDANT AYARLARI ─────────────────────────────────
static const char *AXIS_STR[] = {"X", "Y", "Z"};
uint8_t selAxis = 0;

static const float STEP_VAL[] = {0.1f, 0.5f, 1.0f};
static const char *STEP_STR[] = {"0.100", "0.500", "1.000"};
static const uint8_t N_STEPS = 3;
uint8_t selStep = 0; // 0.100 mm

static const float FEED_VAL[] = {1000.0f, 2000.0f, 3000.0f};
static const char *FEED_STR[] = {"1000", "2000", "3000"};
static const uint8_t N_FEEDS = 3;
uint8_t selFeed = 1; // 2000 mm/dk

// ─── EKRAN DURUMU (MENÜ SİSTEMİ) ─────────────────────
enum ScreenState {
  SCR_MAIN,
  SCR_MENU,
  SCR_SPINDLE,
  SCR_JOG,
  SCR_STEP,
  SCR_COOLANT,
  SCR_WIFI_MENU,
  SCR_WIFI_SCAN,
  SCR_WIFI_PASS,
  SCR_WIFI_IP
};
ScreenState scrState = SCR_MAIN;
unsigned long lastActivity = 0;
#define MENU_TIMEOUT_MS 10000

bool needRedraw = true;

// Menü
#define MENU_COUNT 5
int menuIdx = 0;
static const char *MENU_LABELS[] = {
    "Spindle Kontrolu", "Jog Hizi", "Step Boyutu", "Sogutma", "WiFi Ayarlari",
};

// Spindle kontrol
int spindleTarget = 0;
#define SPINDLE_STEP 500
#define SPINDLE_MAX 20000

// Coolant
uint8_t coolantSel = 0; // 0=OFF, 1=FLOOD, 2=MIST

// ─── WIFI DEĞİŞKENLERİ ─────────────────────────────
Preferences prefs;
String wifiSSID = "";
String wifiPass = "";
uint8_t wifiIP[4] = {192, 168, 1, 1};
bool wifiConnected = false;

// ─── TCP CLIENT DEĞİŞKENLERİ ────────────────────────
WiFiClient tcpClient;
bool tcpConnected = false;
String tcpLine = ""; // TCP'den gelen veri tamponu
unsigned long lastTcpStatus = 0;
#define TCP_STATUS_INTERVAL 250 // ms -- TCP uzerinden ? sorgusu
#define FLUIDNC_TCP_PORT 23     // FluidNC Data port (Telnet)
unsigned long lastTcpReconnect = 0;
#define TCP_RECONNECT_INTERVAL 5000 // ms -- yeniden baglanti denemesi
unsigned long lastDebugLog = 0;
#define DEBUG_LOG_INTERVAL 3000 // ms -- periyodik debug log

// WiFi tarama
#define WIFI_MAX_SCAN 8
String scanSSIDs[WIFI_MAX_SCAN];
int32_t scanRSSI[WIFI_MAX_SCAN];
int scanCount = 0;
int wifiScanIdx = 0;

// WiFi alt menü
#define WIFI_MENU_COUNT 5
int wifiMenuIdx = 0;
static const char *WIFI_MENU_LABELS[] = {
    "Ag Tara", "Sifre Gir", "IP Adresi Ayarla", "Baglan / Kes", "Geri Don"};

// Karakter tekerleği (şifre girişi)
static const char CHAR_SET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%_-.";
#define CHAR_SET_LEN (sizeof(CHAR_SET) - 1)
int charIdx = 0;
String passBuffer = "";
#define PASS_MAX_LEN 32

// IP girişi
uint8_t ipEditIdx = 0; // hangi oktet düzenleniyor (0-3)

// ─── FLUIDNC PARSE ────────────────────────────────────
String fcLine = "";
bool fcInPkt = false;

// ─── UART AKTİFLİK TAKİBİ ────────────────────────────
unsigned long lastUartRx = 0;
#define UART_TIMEOUT_MS 500 // 500ms icerisinde UART'tan veri gelmezse pasif say
bool uartActive = false;
unsigned long lastUartStatus = 0;
#define UART_STATUS_INTERVAL 250 // ms -- UART uzerinden ? sorgusu

// Jog throttle
unsigned long lastJog = 0;
#define JOG_THROTTLE_MS 60

// ─── ENCODER ──────────────────────────────────────────
volatile int encDelta = 0;
volatile int lastClkVal = HIGH;

void IRAM_ATTR encISR() {
  int clk = digitalRead(ENC_CLK);
  int dt = digitalRead(ENC_DT);
  if (clk != lastClkVal) {
    encDelta += (dt != clk) ? 1 : -1;
    lastClkVal = clk;
  }
}

// ─── ENCODER TIKLA (Kısa + Uzun Basma) ───────────────
bool encSwPrev = HIGH;
bool encSwStable = HIGH;
bool encClicked = false;
bool encLongPress = false;
bool encLongFired = false; // uzun basma zaten tetiklendi mi
unsigned long encPressStart = 0;
unsigned long encSwChangeTime = 0;
#define ENC_SW_STABLE_MS 30
#define ENC_LONG_PRESS_MS 500 // 500ms = uzun basma

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
    encLongFired = true; // tekrar tetikleme
    lastActivity = millis();
    Serial.println("[ENC] LONG press → BACK");
  }
}

// ─── BUTON DEBOUNCE ───────────────────────────────────
struct Btn {
  uint8_t pin;
  bool lastState;
  bool fired;
  unsigned long lastMs;
};
Btn btns[4] = {{BTN_HOME, HIGH, false, 0},
               {BTN_ZERO, HIGH, false, 0},
               {BTN_AXIS, HIGH, false, 0},
               {BTN_SPEED, HIGH, false, 0}};
#define DEBOUNCE_MS 50

void readBtns() {
  for (int i = 0; i < 4; i++) {
    bool s = digitalRead(btns[i].pin);
    if (s != btns[i].lastState && (millis() - btns[i].lastMs > DEBOUNCE_MS)) {
      btns[i].lastMs = millis();
      btns[i].lastState = s;
      if (s == LOW) {
        btns[i].fired = true;
      }
    }
  }
}

// ─── FLUIDNC KOMUTLAR ─────────────────────────────────
void fcSend(const char *cmd) {
  if (uartActive) {
    FLUIDNC.println(cmd); // UART aktifse sadece UART'a gonder
  } else if (tcpConnected) {
    tcpClient.println(cmd); // UART yoksa TCP'ye gonder
  }
}

void fcJog(uint8_t axis, int8_t dir, float dist, float feed) {
  char buf[48];
  char axCh = (axis == 0) ? 'X' : (axis == 1) ? 'Y' : 'Z';
  snprintf(buf, sizeof(buf), "$J=G91 G21 %c%.4f F%.0f", axCh, dist * dir, feed);
  fcSend(buf);
}

void fcHome() { fcSend("$H"); }
void fcZero(uint8_t ax) {
  const char *cmds[] = {"G92 X0", "G92 Y0", "G92 Z0"};
  fcSend(cmds[ax]);
}

// ─── FLUIDNC PARSE ────────────────────────────────────
void parseStatus(const String &s) {
  if (s.startsWith("Idle"))
    cur.state = 0;
  else if (s.startsWith("Run"))
    cur.state = 1;
  else if (s.startsWith("Hold"))
    cur.state = 2;
  else if (s.startsWith("Alarm"))
    cur.state = 3;
  else if (s.startsWith("Home")) {
    cur.state = 4;
    cur.homed = true;
  } else if (s.startsWith("Jog"))
    cur.state = 5;
  else if (s.startsWith("Door"))
    cur.state = 6;

  int idx = s.indexOf("MPos:");
  if (idx >= 0) {
    String v = s.substring(idx + 5);
    int c1 = v.indexOf(','), c2 = v.indexOf(',', c1 + 1), e = v.indexOf('|');
    if (c1 > 0 && c2 > c1) {
      cur.x = v.substring(0, c1).toFloat();
      cur.y = v.substring(c1 + 1, c2).toFloat();
      cur.z = v.substring(c2 + 1, e > 0 ? e : (int)v.length()).toFloat();
    }
  }

  idx = s.indexOf("FS:");
  if (idx >= 0) {
    String fs = s.substring(idx + 3);
    cur.feed = fs.toFloat();
    int comma = fs.indexOf(',');
    if (comma >= 0)
      cur.spindle = fs.substring(comma + 1).toFloat();
  }

  idx = s.indexOf("WCO:");
  if (idx >= 0) {
    String w = s.substring(idx + 4);
    int c1 = w.indexOf(','), c2 = w.indexOf(',', c1 + 1), e = w.indexOf('|');
    if (c1 > 0 && c2 > c1) {
      cur.wco_x = w.substring(0, c1).toFloat();
      cur.wco_y = w.substring(c1 + 1, c2).toFloat();
      cur.wco_z = w.substring(c2 + 1, e > 0 ? e : (int)w.length()).toFloat();
    }
  }
}

void readFluidNC() {
  while (FLUIDNC.available()) {
    lastUartRx = millis(); // UART'tan veri geldi, zamani guncelle
    char c = (char)FLUIDNC.read();
    if (c == '<') {
      fcLine = "";
      fcInPkt = true;
    } else if (c == '>') {
      if (fcInPkt)
        parseStatus(fcLine);
      fcInPkt = false;
      fcLine = "";
    } else if (fcInPkt)
      fcLine += c;
  }
}

// ═══════════════════════════════════════════════════════
// ─── ÇİZİM FONKSİYONLARI ─────────────────────────────
// ═══════════════════════════════════════════════════════

static uint16_t stateColor(uint8_t s) {
  switch (s) {
  case 0:
    return C_GREEN;
  case 1:
    return C_CYAN;
  case 2:
    return C_YELLOW;
  case 3:
    return C_RED;
  case 4:
    return C_ORANGE;
  case 5:
    return C_CYAN;
  case 6:
    return C_RED;
  default:
    return C_GRAY;
  }
}

static const char *stateStr(uint8_t s) {
  switch (s) {
  case 0:
    return "IDLE ";
  case 1:
    return "RUN  ";
  case 2:
    return "HOLD ";
  case 3:
    return "ALARM";
  case 4:
    return "HOME ";
  case 5:
    return "JOG  ";
  case 6:
    return "DOOR ";
  default:
    return "?????";
  }
}

// ─── ANA EKRAN ────────────────────────────────────────
void drawHeader() {
  tft.fillRect(0, 0, TFT_W, 19, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(stateColor(cur.state));
  tft.setCursor(4, 5);
  tft.print("[");
  tft.print(stateStr(cur.state));
  tft.print("]");
  tft.setTextColor(C_CYAN);
  tft.setCursor(80, 5);
  tft.print("CNC PENDANT");
  tft.setTextColor(cur.homed ? C_GREEN : C_RED);
  tft.setCursor(180, 5);
  tft.print(cur.homed ? "[HOMED]" : "[NO HOME]");
  // WiFi + TCP durum ikonu
  tft.setCursor(250, 5);
  if (tcpConnected) {
    tft.setTextColor(C_GREEN);
    tft.print("[TCP]");
  } else if (wifiConnected) {
    tft.setTextColor(C_YELLOW);
    tft.print("[Wifi]");
  } else {
    tft.setTextColor(C_RED);
    tft.print("[]");
  }
  tft.fillRect(286, 1, 32, 17, C_ROWHL);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(295, 5);
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
  char fb[8];
  snprintf(fb, sizeof(fb), "%.0f", cur.feed);
  tft.print(fb);
  tft.setTextColor(C_GREEN);
  tft.setCursor(55, y0 + 5);
  tft.print("SPD:");
  char sb[8];
  snprintf(sb, sizeof(sb), "%.0f", cur.spindle);
  tft.print(sb);
  tft.setTextColor(C_CYAN);
  tft.setCursor(130, y0 + 5);
  tft.print("STEP:");
  tft.print(STEP_STR[selStep]);
  // FluidNC IP adresi
  tft.setCursor(210, y0 + 5);
  tft.setTextColor(C_ORANGE);
  char fncIP[22];
  snprintf(fncIP, sizeof(fncIP), "FNC:%d.%d.%d.%d", wifiIP[0], wifiIP[1],
           wifiIP[2], wifiIP[3]);
  tft.print(fncIP);
  tft.setTextColor(C_GRAY);
  tft.setCursor(4, y0 + 19);
  // Duruma göre bağlamsal yardım metni
  if (cur.state == 3) { // ALARM
    tft.setTextColor(C_RED);
    tft.print("[ZERO] Reset  [HOME] Kilit Ac");
  } else if (cur.state == 2) { // HOLD
    tft.setTextColor(C_YELLOW);
    tft.print("[SPEED] Devam Et");
  } else if (cur.state == 1 || cur.state == 5) { // RUN / JOG
    tft.setTextColor(C_CYAN);
    tft.print("[SPEED] Durakla");
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

void updateMainDisplay() {
  if (needRedraw) {
    drawMainAll();
    needRedraw = false;
    prev = cur;
    return;
  }
  bool posChg = cur.x != prev.x || cur.y != prev.y || cur.z != prev.z ||
                cur.wco_x != prev.wco_x || cur.wco_y != prev.wco_y ||
                cur.wco_z != prev.wco_z;
  bool stChg = cur.state != prev.state || cur.homed != prev.homed;
  bool fdChg = cur.feed != prev.feed || cur.spindle != prev.spindle;
  if (posChg)
    drawAxisRows();
  if (stChg) {
    drawHeader();
    drawFooter();
    needRedraw = true;
  }
  if (fdChg)
    drawFooter();
  prev = cur;
}

// ─── MENÜ EKRANI ──────────────────────────────────────
void drawMenuScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 22, C_PANEL);
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(90, 3);
  tft.print("CNC MENU");
  tft.drawFastHLine(0, 22, TFT_W, C_GRAY);
  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 26 + i * 24;
    bool sel = (i == menuIdx);
    if (sel)
      tft.fillRoundRect(8, y, 304, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 24 : 16, y + 3);
    if (sel)
      tft.print("> ");
    tft.print(MENU_LABELS[i]);
  }
  tft.drawFastHLine(0, 150, TFT_W, C_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Encoder: Sec  |  Tikla: Gir");
  needRedraw = false;
}

// ─── SPINDLE GAUGE EKRANI ─────────────────────────────
void drawThickArc(int cx, int cy, int r, float startDeg, float sweepDeg,
                  int thick, uint16_t color) {
  int ht = thick / 2;
  for (float a = 0; a <= sweepDeg; a += 2.5) {
    float rad = (startDeg + a) * PI / 180.0;
    int x = cx + (int)(r * cos(rad));
    int y = cy + (int)(r * sin(rad));
    tft.fillCircle(x, y, ht, color);
  }
}

void drawGaugeTick(int cx, int cy, int r, float deg, int len, uint16_t col) {
  float rad = deg * PI / 180.0;
  float cs = cos(rad), sn = sin(rad);
  tft.drawLine(cx + (int)((r - len) * cs), cy + (int)((r - len) * sn),
               cx + (int)((r + 2) * cs), cy + (int)((r + 2) * sn), col);
}

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
  drawThickArc(cx, cy, r, 135.0, 270.0, 10, C_GRAY);
  // Değer arkı
  float valSweep = (cur.spindle / 20000.0) * 270.0;
  if (valSweep < 0)
    valSweep = 0;
  if (valSweep > 270)
    valSweep = 270;
  uint16_t arcCol = (cur.spindle < 7000)    ? C_GREEN
                    : (cur.spindle < 14000) ? C_YELLOW
                                            : C_RED;
  if (valSweep > 0)
    drawThickArc(cx, cy, r, 135.0, valSweep, 10, arcCol);
  // Tick işaretleri (0, 5K, 10K, 15K, 20K)
  for (int i = 0; i <= 4; i++) {
    float ta = 135.0 + i * 67.5;
    drawGaugeTick(cx, cy, r + 4, ta, 10, C_WHITE);
  }
  // Tick etiketleri
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(cx - r - 18, cy + r - 8);
  tft.print("0");
  tft.setCursor(cx - r - 18, cy - 20);
  tft.print("5K");
  tft.setCursor(cx - 12, cy - r - 14);
  tft.print("10K");
  tft.setCursor(cx + r + 4, cy - 20);
  tft.print("15K");
  tft.setCursor(cx + r + 4, cy + r - 8);
  tft.print("20K");

  // RPM ortada
  char rpm[8];
  snprintf(rpm, sizeof(rpm), "%.0f", cur.spindle);
  tft.setTextSize(3);
  tft.setTextColor(C_WHITE);
  int tw = strlen(rpm) * 18;
  tft.setCursor(cx - tw / 2, cy - 12);
  tft.print(rpm);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(cx - 9, cy + 12);
  tft.print("RPM");

  // Sağ panel: Hedef hız
  int px = 210;
  tft.drawFastVLine(200, 20, 130, C_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(C_ORANGE);
  tft.setCursor(px, 28);
  tft.print("HEDEF HIZ:");
  char tb[8];
  snprintf(tb, sizeof(tb), "%d", spindleTarget);
  tft.setTextSize(3);
  tft.setTextColor(C_YELLOW);
  int ttw = strlen(tb) * 18;
  tft.setCursor(px + (110 - ttw) / 2, 48);
  tft.print(tb);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(px + 35, 78);
  tft.print("RPM");

  // Durum göstergesi
  tft.setTextSize(2);
  tft.setTextColor(running ? C_GREEN : C_RED);
  tft.setCursor(px + 10, 100);
  tft.print(running ? "CALISIYOR" : " KAPALI ");

  // Alt bar
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(20, 157);
  tft.print("Cevir: Hiz Ayarla  |  Tikla: M3 Gonder");
  needRedraw = false;
}

// ─── JOG HIZI EKRANI ──────────────────────────────────
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
    if (sel)
      tft.fillRoundRect(30, y, 260, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 3);
    if (sel)
      tft.print("> ");
    tft.print(FEED_STR[i]);
    tft.print(" mm/dk");
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Cevir: Sec  |  Tikla: Onayla");
  needRedraw = false;
}

// ─── STEP BOYUTU EKRANI ──────────────────────────────
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
    if (sel)
      tft.fillRoundRect(30, y, 260, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 3);
    if (sel)
      tft.print("> ");
    tft.print(STEP_STR[i]);
    tft.print(" mm");
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 157);
  tft.print("Cevir: Sec  |  Tikla: Onayla");
  needRedraw = false;
}

// ─── SOĞUTMA EKRANI ──────────────────────────────────
static const char *COOL_STR[] = {"KAPALI (M9)", "FLOOD (M8)", "MIST (M7)"};
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
    if (sel)
      tft.fillRoundRect(30, y, 260, 26, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 50 : 42, y + 5);
    if (sel)
      tft.print("> ");
    tft.print(COOL_STR[i]);
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(40, 157);
  tft.print("Cevir: Sec  |  Tikla: Gonder & Geri");
  needRedraw = false;
}

// ═══════════════════════════════════════════════════════
// ─── WIFI FONKSİYONLARI ──────────────────────────────
// ═══════════════════════════════════════════════════════

void wifiSavePrefs() {
  prefs.begin("wifi", false);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("pass", wifiPass);
  prefs.putUChar("ip0", wifiIP[0]);
  prefs.putUChar("ip1", wifiIP[1]);
  prefs.putUChar("ip2", wifiIP[2]);
  prefs.putUChar("ip3", wifiIP[3]);
  prefs.end();
  Serial.println("[WIFI] Prefs saved");
}

void wifiLoadPrefs() {
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  wifiIP[0] = prefs.getUChar("ip0", 192);
  wifiIP[1] = prefs.getUChar("ip1", 168);
  wifiIP[2] = prefs.getUChar("ip2", 1);
  wifiIP[3] = prefs.getUChar("ip3", 1);
  prefs.end();
  Serial.printf("[WIFI] Loaded: SSID=%s IP=%d.%d.%d.%d\n", wifiSSID.c_str(),
                wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
}

void wifiStartScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  scanCount = min(n, WIFI_MAX_SCAN);
  for (int i = 0; i < scanCount; i++) {
    scanSSIDs[i] = WiFi.SSID(i);
    scanRSSI[i] = WiFi.RSSI(i);
  }
  wifiScanIdx = 0;
  Serial.printf("[WIFI] Scan found %d networks\n", scanCount);
}

void wifiConnect() {
  if (wifiSSID.length() == 0)
    return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  Serial.printf("[WIFI] Connecting to %s...\n", wifiSSID.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(250);
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiConnected) {
    Serial.printf("[WIFI] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    wifiSavePrefs();
    // TCP baglantisini baslat
    tcpStartConnection();
  } else {
    Serial.println("[WIFI] Connection FAILED");
  }
}

void wifiDisconnect() {
  tcpClient.stop();
  tcpConnected = false;
  WiFi.disconnect();
  wifiConnected = false;
  Serial.println("[WIFI] Disconnected");
}

// ─── TCP FONKSİYONLARI ───────────────────────────────
void tcpReadIncoming() {
  // TCP'den gelen veriyi oku ve isle (UART parse ile ayni mantik)
  while (tcpClient.available()) {
    char c = (char)tcpClient.read();
    if (c == '<') {
      tcpLine = "";
    } else if (c == '>') {
      if (tcpLine.length() > 0) {
        Serial.printf("[TCP] RX: <%s>\n", tcpLine.c_str());
        parseStatus(tcpLine);
      }
      tcpLine = "";
    } else if (c != '\n' && c != '\r') {
      tcpLine += c;
    }
  }
}

void tcpStartConnection() {
  if (!wifiConnected)
    return;
  // Mevcut baglanti varsa once kes
  if (tcpClient.connected()) {
    tcpClient.stop();
  }
  tcpConnected = false;
  char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", wifiIP[0], wifiIP[1], wifiIP[2],
           wifiIP[3]);
  Serial.printf("[TCP] Connecting to %s:%d...\n", ipStr, FLUIDNC_TCP_PORT);
  if (tcpClient.connect(ipStr, FLUIDNC_TCP_PORT)) {
    tcpConnected = true;
    Serial.println("[TCP] >>> CONNECTED to FluidNC! <<<");
    needRedraw = true;
  } else {
    Serial.println("[TCP] >>> CONNECTION FAILED <<<");
  }
}

// sinyal gucu ikonu (ASCII)
static const char *rssiIcon(int32_t rssi) {
  if (rssi > -50)
    return "||||";
  if (rssi > -65)
    return "||| ";
  if (rssi > -75)
    return "||  ";
  return "|   ";
}

// ─── WIFI MENÜ EKRANI ────────────────────────────────
void drawWifiMenuScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 22, TFT_W, C_PANEL);
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
    int y = 26 + i * 24;
    bool sel = (i == wifiMenuIdx);
    if (sel)
      tft.fillRoundRect(8, y, 304, 22, 4, C_ROWHL);
    tft.setTextSize(2);
    tft.setTextColor(sel ? C_YELLOW : C_WHITE);
    tft.setCursor(sel ? 24 : 16, y + 3);
    if (sel)
      tft.print("> ");
    tft.print(WIFI_MENU_LABELS[i]);
  }

  tft.fillRect(0, 150, TFT_W, 20, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
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

// ─── WIFI TARAMA EKRANI ──────────────────────────────
void drawWifiScanScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, TFT_W, 20, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(4, 6);
  tft.print("WIFI AG TARA");
  tft.setTextColor(C_GRAY);
  tft.setCursor(200, 6);
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d ag bulundu", scanCount);
  tft.print(cnt);
  tft.drawFastHLine(0, 20, TFT_W, C_GRAY);

  if (scanCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(C_RED);
    tft.setCursor(60, 70);
    tft.print("Ag bulunamadi!");
  } else {
    for (int i = 0; i < scanCount; i++) {
      int y = 24 + i * 16;
      bool sel = (i == wifiScanIdx);
      if (sel)
        tft.fillRect(0, y, TFT_W, 16, C_ROWHL);
      tft.setTextSize(1);
      tft.setTextColor(sel ? C_YELLOW : C_WHITE);
      tft.setCursor(4, y + 4);
      if (sel)
        tft.print("> ");
      // SSID (max 20 karakter)
      String ssidDisp = scanSSIDs[i].substring(0, 20);
      tft.print(ssidDisp);
      // Sinyal gücü
      tft.setCursor(240, y + 4);
      tft.setTextColor(
          scanRSSI[i] > -65 ? C_GREEN : (scanRSSI[i] > -80 ? C_YELLOW : C_RED));
      tft.print(rssiIcon(scanRSSI[i]));
      // dBm
      char db[8];
      snprintf(db, sizeof(db), "%ddB", (int)scanRSSI[i]);
      tft.setCursor(280, y + 4);
      tft.setTextColor(C_GRAY);
      tft.print(db);
    }
  }
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(30, 157);
  tft.print("Cevir: Sec  |  Tikla: Ag Sec  |  Uzun: Geri");
  needRedraw = false;
}

// ─── WIFI ŞİFRE GİRİŞ EKRANI ────────────────────────
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
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 28);
  tft.print("Sifre: ");
  // Şifreyi maskeli göster (son 3 karakter açık)
  int pLen = passBuffer.length();
  for (int i = 0; i < pLen; i++) {
    if (i >= pLen - 3)
      tft.print(passBuffer.charAt(i));
    else
      tft.print('*');
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
    if (offset == 0)
      continue;
    int ci = (charIdx + offset + CHAR_SET_LEN) % CHAR_SET_LEN;
    int xp = 160 + offset * 22;
    if (xp < 5 || xp > 310)
      continue;
    tft.setTextColor(abs(offset) <= 2 ? C_WHITE : C_GRAY);
    char cc[2] = {CHAR_SET[ci], 0};
    tft.setCursor(xp - 6, 110);
    tft.print(cc);
  }

  // Alt kısım
  tft.drawFastHLine(0, 135, TFT_W, C_GRAY);
  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(10, 140);
  tft.print("Cevir: Karakter  Tikla: Ekle  Uzun: Sil");
  tft.setTextColor(C_GREEN);
  tft.setCursor(10, 157);
  tft.print("[AXIS] btn = ONAYLA & KAYDET");
  needRedraw = false;
}

// ─── WIFI IP EKRANI ──────────────────────────────────
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

    if (active)
      tft.fillRoundRect(xp - 5, 50, 65, 40, 6, C_ROWHL);
    else
      tft.drawRoundRect(xp - 5, 50, 65, 40, 6, C_GRAY);

    tft.setTextSize(3);
    tft.setTextColor(active ? C_YELLOW : C_WHITE);
    char octet[4];
    snprintf(octet, sizeof(octet), "%3d", wifiIP[i]);
    tft.setCursor(xp, 58);
    tft.print(octet);

    // Nokta ayırıcı
    if (i < 3) {
      tft.setTextSize(3);
      tft.setTextColor(C_GRAY);
      tft.setCursor(xp + 58, 58);
      tft.print(".");
    }
  }

  // Altında ok göstergeleri
  int axp = 30 + ipEditIdx * 75;
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(axp + 15, 95);
  tft.print("^^");

  tft.fillRect(0, 152, TFT_W, 18, C_PANEL);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(10, 140);
  tft.print("Cevir: Deger  Tikla: Sonraki Oktet");
  tft.setCursor(10, 157);
  tft.setTextColor(C_GREEN);
  tft.print("[AXIS] btn = KAYDET  |  Uzun bas = Geri");
  needRedraw = false;
}
void popup(const char *msg, uint16_t bgCol, uint32_t dur) {
  tft.fillRoundRect(30, 60, 260, 50, 8, bgCol);
  tft.setTextSize(2);
  tft.setTextColor(C_BG);
  int tx = 30 + (260 - (int)strlen(msg) * 12) / 2;
  tft.setCursor(tx < 34 ? 34 : tx, 75);
  tft.print(msg);
  delay(dur);
  needRedraw = true;
}

// ─── EKRAN GEÇIŞ ─────────────────────────────────────
void switchScreen(ScreenState s) {
  scrState = s;
  needRedraw = true;
  lastActivity = millis();
}

// ═══════════════════════════════════════════════════════
// ─── SETUP ────────────────────────────────────────────
// ═══════════════════════════════════════════════════════
void setup() {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastClkVal = digitalRead(ENC_CLK);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT), encISR, CHANGE);
  for (int i = 0; i < 4; i++)
    pinMode(btns[i].pin, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("[PENDANT] Starting...");

  tft.init(TFT_H, TFT_W);
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  tft.setTextWrap(false);

  FLUIDNC.begin(FC_BAUD, SERIAL_8N1, D9, D8);

  // Kayıtlı WiFi ayarlarını yükle
  wifiLoadPrefs();
  // Eğer SSID kayıtlıysa otomatik bağlan
  if (wifiSSID.length() > 0) {
    wifiConnect();
  }

  // ENC_SW pin durumunu kontrol et
  Serial.print("[ENC_SW] Pin D12 initial state: ");
  Serial.println(digitalRead(ENC_SW) ? "HIGH (released)" : "LOW (pressed)");

  // Splash
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(85, 35);
  tft.print("CNC PENDANT");
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(88, 65);
  tft.print("FluidNC Pendant v4.01");
  tft.setTextColor(C_GRAY);
  tft.setCursor(110, 82);
  tft.print("by VOLTveTORK");
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  delay(2500);

  needRedraw = true;
  lastActivity = millis();
}

// ═══════════════════════════════════════════════════════
// ─── LOOP ─────────────────────────────────────────────
// ═══════════════════════════════════════════════════════
void loop() {
  readFluidNC();
  checkEncClick();
  readBtns();

  // UART aktiflik kontrolu (500ms icerisinde veri geldiyse aktif)
  bool prevUartActive = uartActive;
  uartActive = (millis() - lastUartRx < UART_TIMEOUT_MS);
  if (prevUartActive && !uartActive) {
    Serial.println("[UART] UART pasif, TCP'ye geciliyor...");
    needRedraw = true;
  } else if (!prevUartActive && uartActive) {
    Serial.println("[UART] UART aktif, TCP devre disi");
    needRedraw = true;
  }

  // UART'a periyodik ? sorgusu gonder (FluidNC rapor baslatsin)
  if (millis() - lastUartStatus > UART_STATUS_INTERVAL) {
    lastUartStatus = millis();
    FLUIDNC.println("?");
  }

  // TCP loop (gelen veri oku + baglanti yonetimi)
  if (wifiConnected) {
    // TCP baglanti durumunu kontrol et
    if (tcpConnected && !tcpClient.connected()) {
      tcpConnected = false;
      Serial.println("[TCP] Connection lost!");
      needRedraw = true;
    }
    // TCP bagli degilse yeniden baglanmayi dene
    if (!tcpConnected && millis() - lastTcpReconnect > TCP_RECONNECT_INTERVAL) {
      lastTcpReconnect = millis();
      tcpStartConnection();
    }
    // TCP'den gelen veriyi her zaman oku (baglanti canli kalsin)
    if (tcpConnected) {
      tcpReadIncoming();
      // Status sorgusu sadece UART pasifse gonder
      if (!uartActive && millis() - lastTcpStatus > TCP_STATUS_INTERVAL) {
        tcpClient.println("?");
        lastTcpStatus = millis();
      }
    }
  }

  // WiFi baglanti durumunu guncelle
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    tcpConnected = false;
    Serial.println("[WIFI] Connection lost!");
    needRedraw = true;
  }

  // Periyodik debug log
  if (millis() - lastDebugLog > DEBUG_LOG_INTERVAL) {
    lastDebugLog = millis();
    Serial.printf("[DBG] UART:%s  WiFi:%s  TCP:%s  IP:%d.%d.%d.%d:%d\n",
                  uartActive ? "YES" : "NO", wifiConnected ? "YES" : "NO",
                  tcpConnected ? "YES" : "NO", wifiIP[0], wifiIP[1], wifiIP[2],
                  wifiIP[3], FLUIDNC_TCP_PORT);
  }

  // Her detent 2 puls üretiyor → 2'ye böl, kalanı biriktir
  static int encAccum = 0;
  noInterrupts();
  encAccum += encDelta;
  encDelta = 0;
  interrupts();
  int delta = encAccum / 2;
  encAccum %= 2; // kalan tek pulsu bir sonraki tura aktar

  if (delta != 0)
    lastActivity = millis();

  // HOME hariç, buton basıldıysa alt ekranlardan çık
  // (HOME = geri tuşu, menülerde ayrıca işleniyor)
  bool anyBtnExceptHome = btns[1].fired || btns[2].fired || btns[3].fired;
  // WiFi şifre ve IP ekranlarında AXIS butonu = confirm, çıkılmamalı
  if (anyBtnExceptHome && scrState != SCR_MAIN && scrState != SCR_WIFI_PASS &&
      scrState != SCR_WIFI_IP && scrState != SCR_WIFI_MENU &&
      scrState != SCR_WIFI_SCAN) {
    for (int i = 1; i < 4; i++)
      btns[i].fired = false;
    switchScreen(SCR_MAIN);
  }

  // 10 saniye timeout → ana ekrana dön
  if (scrState != SCR_MAIN && (millis() - lastActivity > MENU_TIMEOUT_MS)) {
    switchScreen(SCR_MAIN);
  }

  // ── EKRAN DURUMUNA GÖRE İŞLEM ──
  switch (scrState) {

  case SCR_MAIN: {
    // Uzun basma → ana ekranda işlevi yok, temizle
    if (encLongPress)
      encLongPress = false;

    // ── SPEED BUTONU: Duraklat / Devam Et ──
    if (btns[3].fired) {
      if (cur.state == 1 || cur.state == 5) {
        // RUN veya JOG → Feed Hold
        btns[3].fired = false;
        FLUIDNC.write('!');
        if (!uartActive && tcpConnected)
          tcpClient.write('!');
        popup("DURAKLANDI", C_YELLOW, 600);
        Serial.println("[PENDANT] Feed Hold sent");
        needRedraw = true;
      } else if (cur.state == 2) {
        // HOLD → Cycle Start (Devam)
        btns[3].fired = false;
        FLUIDNC.write('~');
        if (!uartActive && tcpConnected)
          tcpClient.write('~');
        popup("DEVAM EDILIYOR", C_GREEN, 600);
        Serial.println("[PENDANT] Cycle Start sent");
        needRedraw = true;
      }
      // IDLE durumunda btns[3].fired tüketilmez → aşağıda step/feed ayarı yapar
    }

    // ── ALARM DURUMU: Kurtarma ──
    if (cur.state == 3) {
      // ZERO butonu → Soft Reset (Ctrl+X)
      if (btns[1].fired) {
        btns[1].fired = false;
        FLUIDNC.write(0x18); // Ctrl+X soft reset
        if (!uartActive && tcpConnected) {
          tcpClient.write(0x18);
        }
        popup("SOFT RESET!", C_ORANGE, 1000);
        Serial.println("[PENDANT] Soft Reset sent (0x18)");
        needRedraw = true;
      }
      // HOME butonu → Alarm kilidi aç ($X)
      if (btns[0].fired) {
        btns[0].fired = false;
        fcSend("$X");
        popup("KILIT ACILDI", C_GREEN, 800);
        Serial.println("[PENDANT] Alarm unlock ($X) sent");
        needRedraw = true;
      }
      // Alarm durumunda diğer butonları yoksay
      btns[2].fired = false;
      btns[3].fired = false;
      // Alarm durumunda encoder jog engelle
      if (encClicked)
        encClicked = false;
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
      popup("HOMING...", C_ORANGE, 300);
    }
    if (btns[1].fired) {
      btns[1].fired = false;
      fcZero(selAxis);
      char msg[10];
      snprintf(msg, sizeof(msg), "ZERO %s", AXIS_STR[selAxis]);
      popup(msg, C_GREEN, 400);
    }
    if (btns[2].fired) {
      btns[2].fired = false;
      selAxis = (selAxis + 1) % 3;
      needRedraw = true;
    }
    if (btns[3].fired) {
      btns[3].fired = false;
      selStep = (selStep + 1) % N_STEPS;
      if (selStep == 0)
        selFeed = (selFeed + 1) % N_FEEDS;
      needRedraw = true;
    }
    // Encoder → jog
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

  case SCR_MENU: {
    // Uzun basma veya HOME butonu → ana ekrana dön
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MAIN);
      break;
    }
    if (delta != 0) {
      menuIdx += (delta > 0) ? 1 : -1;
      if (menuIdx < 0)
        menuIdx = MENU_COUNT - 1;
      if (menuIdx >= MENU_COUNT)
        menuIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      switch (menuIdx) {
      case 0:
        spindleTarget = (int)cur.spindle;
        switchScreen(SCR_SPINDLE);
        break;
      case 1:
        switchScreen(SCR_JOG);
        break;
      case 2:
        switchScreen(SCR_STEP);
        break;
      case 3:
        switchScreen(SCR_COOLANT);
        break;
      case 4:
        wifiMenuIdx = 0;
        switchScreen(SCR_WIFI_MENU);
        break;
      case 5:
        switchScreen(SCR_MAIN);
        break;
      }
      break;
    }
    if (needRedraw)
      drawMenuScreen();
    break;
  }

  case SCR_SPINDLE: {
    // Uzun basma veya HOME butonu → menüye dön
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    bool changed = false;
    if (delta != 0) {
      spindleTarget += delta * SPINDLE_STEP;
      if (spindleTarget < 0)
        spindleTarget = 0;
      if (spindleTarget > SPINDLE_MAX)
        spindleTarget = SPINDLE_MAX;
      changed = true;
    }
    if (encClicked) {
      encClicked = false;
      char cmd[16];
      if (spindleTarget > 0) {
        snprintf(cmd, sizeof(cmd), "M3 S%d", spindleTarget);
      } else {
        snprintf(cmd, sizeof(cmd), "M5");
      }
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

  case SCR_JOG: {
    // Uzun basma veya HOME butonu → menüye dön
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      selFeed += (delta > 0) ? 1 : -1;
      if ((int8_t)selFeed < 0)
        selFeed = N_FEEDS - 1;
      if (selFeed >= N_FEEDS)
        selFeed = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      popup("JOG HIZI SECILDI", C_GREEN, 300);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw)
      drawJogScreen();
    break;
  }

  case SCR_STEP: {
    // Uzun basma veya HOME butonu → menüye dön
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      selStep += (delta > 0) ? 1 : -1;
      if ((int8_t)selStep < 0)
        selStep = N_STEPS - 1;
      if (selStep >= N_STEPS)
        selStep = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      popup("STEP SECILDI", C_GREEN, 300);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw)
      drawStepScreen();
    break;
  }

  case SCR_COOLANT: {
    // Uzun basma veya HOME butonu → menüye dön
    if (encLongPress || btns[0].fired) {
      encLongPress = false;
      btns[0].fired = false;
      switchScreen(SCR_MENU);
      break;
    }
    if (delta != 0) {
      coolantSel += (delta > 0) ? 1 : -1;
      if ((int8_t)coolantSel < 0)
        coolantSel = 2;
      if (coolantSel > 2)
        coolantSel = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      switch (coolantSel) {
      case 0:
        fcSend("M9");
        break;
      case 1:
        fcSend("M8");
        break;
      case 2:
        fcSend("M7");
        break;
      }
      popup(COOL_STR[coolantSel], C_DKGREEN, 500);
      switchScreen(SCR_MENU);
      break;
    }
    if (needRedraw)
      drawCoolantScreen();
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
      if (wifiMenuIdx < 0)
        wifiMenuIdx = WIFI_MENU_COUNT - 1;
      if (wifiMenuIdx >= WIFI_MENU_COUNT)
        wifiMenuIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      switch (wifiMenuIdx) {
      case 0: // Ag Tara
        popup("Taraniyor...", C_CYAN, 200);
        wifiStartScan();
        switchScreen(SCR_WIFI_SCAN);
        break;
      case 1: // Sifre Gir
        charIdx = 0;
        passBuffer = wifiPass; // mevcut şifreyi yükle
        switchScreen(SCR_WIFI_PASS);
        break;
      case 2: // IP Adresi Ayarla
        ipEditIdx = 0;
        switchScreen(SCR_WIFI_IP);
        break;
      case 3: // Baglan / Kes
        if (wifiConnected) {
          wifiDisconnect();
          popup("BAGLANTI KESILDI", C_RED, 800);
        } else {
          popup("Baglaniyor...", C_CYAN, 200);
          wifiConnect();
          if (wifiConnected)
            popup("BAGLANDI!", C_GREEN, 1000);
          else
            popup("BASARISIZ!", C_RED, 1000);
        }
        needRedraw = true;
        break;
      case 4: // Geri Don
        switchScreen(SCR_MENU);
        break;
      }
      break;
    }
    if (needRedraw)
      drawWifiMenuScreen();
    break;
  }

  // ── WIFI TARAMA ────────────────────────────────────
  case SCR_WIFI_SCAN: {
    if (encLongPress) {
      encLongPress = false;
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (delta != 0 && scanCount > 0) {
      wifiScanIdx += (delta > 0) ? 1 : -1;
      if (wifiScanIdx < 0)
        wifiScanIdx = scanCount - 1;
      if (wifiScanIdx >= scanCount)
        wifiScanIdx = 0;
      needRedraw = true;
    }
    if (encClicked) {
      encClicked = false;
      if (scanCount > 0) {
        wifiSSID = scanSSIDs[wifiScanIdx];
        Serial.printf("[WIFI] Selected SSID: %s\n", wifiSSID.c_str());
        char msg[32];
        snprintf(msg, sizeof(msg), "AG: %s", wifiSSID.substring(0, 14).c_str());
        popup(msg, C_GREEN, 800);
        switchScreen(SCR_WIFI_MENU);
      }
      break;
    }
    if (needRedraw)
      drawWifiScanScreen();
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
      if (charIdx < 0)
        charIdx = CHAR_SET_LEN - 1;
      if (charIdx >= (int)CHAR_SET_LEN)
        charIdx = 0;
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
      popup("SIFRE KAYDEDILDI", C_GREEN, 800);
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (needRedraw)
      drawWifiPassScreen();
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
      if (val < 0)
        val = 255;
      if (val > 255)
        val = 0;
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
      // WiFi bagliysa TCP'yi yeni IP ile yeniden baslat
      if (wifiConnected) {
        tcpStartConnection();
        Serial.println("[TCP] Reconnecting with new IP...");
      }
      char ipStr[20];
      snprintf(ipStr, sizeof(ipStr), "IP: %d.%d.%d.%d", wifiIP[0], wifiIP[1],
               wifiIP[2], wifiIP[3]);
      popup(ipStr, C_GREEN, 1000);
      switchScreen(SCR_WIFI_MENU);
      break;
    }
    if (needRedraw)
      drawWifiIPScreen();
    break;
  }
  }
}
