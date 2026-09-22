#pragma once
/**
 * config.h — CNC Pendant v7.0 WiFi
 * Tüm sabitler, enum, struct ve extern tanımları
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// ─── PIN TANIMLARI ────────────────────────────────────
#define ENC_CLK D2
#define ENC_DT  D3
#define ENC_SW  A0

#define BTN_HOME  D4
#define BTN_ZERO  D5
#define BTN_AXIS  A6
#define BTN_SPEED A7

#define TFT_CS  D10
#define TFT_DC  D6
#define TFT_RST D7
#define TFT_BLK A2  // Arka ışık (Backlight) kontrolü — PWM

// ─── TFT ──────────────────────────────────────────────
#define TFT_W 320
#define TFT_H 170

// ─── PİL (BATTERY) ────────────────────────────────────
#define BAT_PIN       A1      // GPIO2
#define BAT_R_RATIO   2.0f    // 100K + 100K voltaj bölücü oranı
#define BAT_READ_INTERVAL 10000  // 10 saniyede bir oku (#9)
#define BAT_SAMPLES   8          // Gürültü filtresi (#9)

// ─── RENKLER ──────────────────────────────────────────
#define C_BG      0x0000
#define C_WHITE   0xFFFF
#define C_CYAN    0x07FF
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_YELLOW  0xFFE0
#define C_ORANGE  0xFD20
#define C_GRAY    0x4208
#define C_PANEL   0x1082
#define C_ROWHL   0x0841
#define C_DKGREEN 0x03E0

// ─── FLUIDNC UART ─────────────────────────────────────
#define FLUIDNC Serial1
#define FC_BAUD 115200

// ─── MAKİNA DURUMU ENUM (#13) ─────────────────────────
enum MachineState : uint8_t {
  ST_IDLE  = 0,
  ST_RUN   = 1,
  ST_HOLD  = 2,
  ST_ALARM = 3,
  ST_HOME  = 4,
  ST_JOG   = 5,
  ST_DOOR  = 6
};

// ─── STATUS STRUCT ────────────────────────────────────
struct Status {
  float x, y, z;
  float wco_x, wco_y, wco_z;
  float feed;
  float spindle;
  uint8_t state;
  bool homed;
  uint8_t feedOv;    // Feed override %
  uint8_t rapidOv;   // Rapid override %
  uint8_t spindleOv; // Spindle override %
};

// ─── BUTON STRUCT ─────────────────────────────────────
struct Btn {
  uint8_t pin;
  bool lastState;
  bool fired;
  unsigned long lastMs;
};

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
  SCR_WIFI_IP,
  SCR_FEED_OV,
  SCR_SPINDLE_OV,
  SCR_OVERRIDE_SEL,
  SCR_PROBE          // #18 Z Probe
};

// ─── POPUP STRUCT (#4 non-blocking) ──────────────────
struct PopupState {
  bool active;
  unsigned long startMs;
  unsigned long durMs;
};

// ─── WiFi BAĞLANTI STATE MACHINE (#5) ────────────────
enum WifiConnState { WCS_IDLE, WCS_CONNECTING, WCS_CONNECTED, WCS_FAILED };

// ─── ZAMANLAMA SABİTLERİ ──────────────────────────────
#define DEBOUNCE_MS        50
#define ENC_SW_STABLE_MS   30
#define ENC_LONG_PRESS_MS  500
#define HOME_LONG_PRESS_MS 1500  // 1.5 saniye basılı tutunca uyku

#define MENU_TIMEOUT_MS    10000
#define JOG_THROTTLE_MS    60

#define SPINDLE_STEP  500
#define SPINDLE_MAX   20000

#define UART_TIMEOUT_MS       500
#define UART_STATUS_INTERVAL  250

#define TCP_STATUS_INTERVAL    250
#define FLUIDNC_TCP_PORT       23
#define TCP_RECONNECT_INTERVAL 5000
#define DEBUG_LOG_INTERVAL     3000

#define SLEEP_TIMEOUT_MS  120000   // 2 dakika veri gelmezse uyu
#define SLEEP_WARNING_MS  110000   // 10 saniye kala uyarı göster
#define BTN_HOME_GPIO     GPIO_NUM_7  // D4 = GPIO7

#define WIFI_MAX_SCAN     8
#define PASS_MAX_LEN      32

// ─── FLOAT KARŞILAŞTIRMA EPSİLON (#8) ────────────────
#define POS_EPSILON 0.0005f

// ─── PARLAKLIK (#19) ──────────────────────────────────
#define BRIGHTNESS_LEVELS 4
static const uint8_t BRIGHTNESS_VAL[] = {64, 128, 192, 255};

// ─── Z PROBE SABİTLERİ (#18) ─────────────────────────
#define PROBE_DEPTH_DEFAULT -30
#define PROBE_FEED_DEFAULT  100
#define PROBE_DEPTH_MIN    -100
#define PROBE_DEPTH_MAX     -5
#define PROBE_FEED_MIN      10
#define PROBE_FEED_MAX      500
#define PROBE_RETRACT_DEFAULT 5
#define PROBE_RETRACT_MIN     1
#define PROBE_RETRACT_MAX     50

// ─── MENÜ SAYILARİ ───────────────────────────────────
#define MENU_IDLE_COUNT 6   // IDLE: Spindle, Jog, Step, Sogutma, Z Probe, WiFi
#define MENU_RUN_COUNT  5   // RUN/HOLD: SpindleOv, FeedOv, Jog, Step, Sogutma
#define WIFI_MENU_COUNT 6   // Ag Tara, Sifre, Oto IP, Baglan, Parlaklik, Geri

// ─── PENDANT AYAR DİZİLERİ ───────────────────────────
static const char *AXIS_STR[] = {"X", "Y", "Z"};
static const float STEP_VAL[] = {0.1f, 0.5f, 1.0f};
static const char *STEP_STR[] = {"0.100", "0.500", "1.000"};
static const uint8_t N_STEPS = 3;
static const float FEED_VAL[] = {1000.0f, 2000.0f, 3000.0f};
static const char *FEED_STR[] = {"1000", "2000", "3000"};
static const uint8_t N_FEEDS = 3;
static const char *COOL_STR[] = {"KAPALI (M9)", "FLOOD (M8)", "MIST (M7)"};

// Şifre girişi karakter seti
static const char CHAR_SET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%_-.";
#define CHAR_SET_LEN (sizeof(CHAR_SET) - 1)

// ═══════════════════════════════════════════════════════
// ─── EXTERN GLOBAL DEĞİŞKENLER ───────────────────────
// (Tanımları CNC_pendant_v7_0_wifi.ino dosyasında)
// ═══════════════════════════════════════════════════════

// TFT
extern Adafruit_ST7789 tft;

// Makina durumu
extern Status cur, prev;

// Ekran
extern ScreenState scrState;
extern bool needRedraw;
extern unsigned long lastActivity;

// Ayarlar
extern uint8_t selAxis;
extern uint8_t selStep;
extern uint8_t selFeed;

// Menü
extern int menuIdx;
extern uint8_t ovSelIdx;
extern int spindleTarget;
extern uint8_t coolantSel;

// Uyku
extern unsigned long lastDataReceived;
extern bool sleepWarningShown;

// Jog
extern unsigned long lastJog;

// Parlaklık (#19)
extern uint8_t brightnessIdx;

// Z Probe (#18)
extern int probeDepth;
extern int probeFeed;
extern int probeRetract;
extern uint8_t probeParamIdx;

// ─── FORWARD DECLARATIONS ─────────────────────────────
// (Tanımları .ino dosyasında)
void enterDeepSleep();
void switchScreen(ScreenState s);
