/**
 * wifi_mgr.cpp — WiFi, mDNS keşfi, TCP bağlantı yönetimi
 * #5  Non-blocking WiFi connect (state machine)
 * #6  Async WiFi scan
 * #7  TCP parse char buffer ile
 * #20 Kullanıcı ayarları kalıcılığı (Preferences)
 */
#include "wifi_mgr.h"
#include "fluidnc.h"  // parseStatus() için

// ─── GLOBAL DEĞİŞKENLER ──────────────────────────────
Preferences prefs;
WiFiClient tcpClient;
bool tcpConnected = false;
bool wifiConnected = false;
String wifiSSID = "";
String wifiPass = "";
uint8_t wifiIP[4] = {192, 168, 1, 1};
bool mdnsStarted = false;
uint8_t tcpFailCount = 0;
unsigned long lastTcpReconnect = 0;
unsigned long lastTcpStatus = 0;
unsigned long lastDebugLog = 0;

// WiFi tarama
String scanSSIDs[WIFI_MAX_SCAN];
int32_t scanRSSI[WIFI_MAX_SCAN];
int scanCount = 0;
int wifiScanIdx = 0;
bool wifiScanRunning = false;

// WiFi menü
int wifiMenuIdx = 0;
int charIdx = 0;
String passBuffer = "";
uint8_t ipEditIdx = 0;

// WiFi bağlantı state machine (#5)
WifiConnState wcsState = WCS_IDLE;
static unsigned long wcsStartMs = 0;

// TCP parse buffer (#7)
static char tcpBuf[128];
static uint8_t tcpBufIdx = 0;

// ═══════════════════════════════════════════════════════
// ─── PREFERENCES ─────────────────────────────────────
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

// #20 Kullanıcı ayarları kalıcılığı
void saveUserPrefs() {
  prefs.begin("user", false);
  prefs.putUChar("selAxis", selAxis);
  prefs.putUChar("selStep", selStep);
  prefs.putUChar("selFeed", selFeed);
  prefs.putUChar("bright", brightnessIdx);
  prefs.end();
  Serial.println("[PREFS] User prefs saved");
}

void loadUserPrefs() {
  prefs.begin("user", true);
  selAxis = prefs.getUChar("selAxis", 0);
  selStep = prefs.getUChar("selStep", 0);
  selFeed = prefs.getUChar("selFeed", 1);
  brightnessIdx = prefs.getUChar("bright", BRIGHTNESS_LEVELS - 1);
  prefs.end();
  Serial.printf("[PREFS] Loaded: axis=%d step=%d feed=%d bright=%d\n",
                selAxis, selStep, selFeed, brightnessIdx);
}

// ═══════════════════════════════════════════════════════
// ─── WiFi BAĞLANTI (#5 Non-blocking) ────────────────
// ═══════════════════════════════════════════════════════

void wifiConnectStart() {
  if (wifiSSID.length() == 0) {
    wcsState = WCS_FAILED;
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  wcsState = WCS_CONNECTING;
  wcsStartMs = millis();
  Serial.printf("[WIFI] Connecting to %s...\n", wifiSSID.c_str());
}

WifiConnState wifiConnectUpdate() {
  if (wcsState != WCS_CONNECTING) return wcsState;

  if (WiFi.status() == WL_CONNECTED) {
    wcsState = WCS_CONNECTED;
    wifiConnected = true;
    Serial.printf("[WIFI] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    wifiSavePrefs();
    // mDNS ile FluidNC IP adresini otomatik keşfet
    Serial.println("[WIFI] Auto-discovering FluidNC IP via mDNS...");
    mdnsDiscoverFluidNC();
    // TCP bağlantısını başlat
    tcpStartConnection();
  } else if (millis() - wcsStartMs > 8000) {
    wcsState = WCS_FAILED;
    Serial.println("[WIFI] Connection FAILED (timeout)");
  }
  return wcsState;
}

void wifiDisconnect() {
  tcpClient.stop();
  tcpConnected = false;
  tcpFailCount = 0;
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
  WiFi.disconnect();
  wifiConnected = false;
  wcsState = WCS_IDLE;
  Serial.println("[WIFI] Disconnected");
}

// ═══════════════════════════════════════════════════════
// ─── WiFi TARAMA (#6 Async) ─────────────────────────
// ═══════════════════════════════════════════════════════

void wifiStartScanAsync() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true);  // async = true → bloklamaz
  wifiScanRunning = true;
  Serial.println("[WIFI] Async scan started");
}

bool wifiScanUpdate() {
  if (!wifiScanRunning) return false;

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;  // henüz bitmedi

  // Tarama tamamlandı
  wifiScanRunning = false;
  scanCount = min(n < 0 ? 0 : n, (int)WIFI_MAX_SCAN);
  for (int i = 0; i < scanCount; i++) {
    scanSSIDs[i] = WiFi.SSID(i);
    scanRSSI[i] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  wifiScanIdx = 0;
  Serial.printf("[WIFI] Scan found %d networks\n", scanCount);
  return true;  // tamamlandı
}

// ═══════════════════════════════════════════════════════
// ─── mDNS İLE FLUIDNC OTOMATİK KEŞFİ ──────────────
// ═══════════════════════════════════════════════════════

bool mdnsDiscoverFluidNC() {
  if (!wifiConnected) return false;

  if (!mdnsStarted) {
    if (MDNS.begin("cncpendant")) {
      mdnsStarted = true;
      Serial.println("[mDNS] Started as 'cncpendant'");
    } else {
      Serial.println("[mDNS] Start failed!");
      return false;
    }
  }

  Serial.println("[mDNS] Searching for FluidNC on network...");
  // FluidNC _http._tcp servisi yayınlar
  int n = MDNS.queryService("http", "tcp");
  Serial.printf("[mDNS] Found %d http services\n", n);

  for (int i = 0; i < n; i++) {
    String hostname = MDNS.hostname(i);
    String hostLower = hostname;
    hostLower.toLowerCase();
    Serial.printf("[mDNS]   [%d] host=%s ip=%s port=%d\n", i, hostname.c_str(),
                  MDNS.IP(i).toString().c_str(), MDNS.port(i));
    // FluidNC hostname'i genellikle "fluidnc" içerir
    if (hostLower.indexOf("fluidnc") >= 0 || hostLower.indexOf("fluid") >= 0) {
      IPAddress ip = MDNS.IP(i);
      if (ip[0] != 0) {
        // Mevcut IP ile aynı mı kontrol et
        if (wifiIP[0] == ip[0] && wifiIP[1] == ip[1] &&
            wifiIP[2] == ip[2] && wifiIP[3] == ip[3]) {
          Serial.println("[mDNS] IP unchanged, already correct");
          return true;
        }
        wifiIP[0] = ip[0]; wifiIP[1] = ip[1];
        wifiIP[2] = ip[2]; wifiIP[3] = ip[3];
        wifiSavePrefs();
        Serial.printf("[mDNS] >>> FluidNC found: %d.%d.%d.%d <<<\n",
                      ip[0], ip[1], ip[2], ip[3]);
        needRedraw = true;
        return true;
      }
    }
  }

  Serial.println("[mDNS] FluidNC not found on network");
  return false;
}

// ═══════════════════════════════════════════════════════
// ─── TCP FONKSİYONLARI ──────────────────────────────
// ═══════════════════════════════════════════════════════

void tcpStartConnection() {
  if (!wifiConnected) return;
  // Mevcut bağlantı varsa önce kes
  if (tcpClient.connected()) {
    tcpClient.stop();
  }
  tcpConnected = false;
  char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
           wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
  Serial.printf("[TCP] Connecting to %s:%d (1s timeout)...\n",
                ipStr, FLUIDNC_TCP_PORT);
  // 1000ms timeout — donma süresi max 1 saniye
  if (tcpClient.connect(ipStr, FLUIDNC_TCP_PORT, 1000)) {
    tcpConnected = true;
    tcpFailCount = 0;
    Serial.println("[TCP] >>> CONNECTED to FluidNC! <<<");
    needRedraw = true;
  } else {
    tcpFailCount++;
    Serial.printf("[TCP] Connection FAILED (attempt #%d)\n", tcpFailCount);
    // Her 3 başarısız denemede mDNS ile IP'yi tekrar keşfet
    if (tcpFailCount >= 3 && tcpFailCount % 3 == 0) {
      Serial.println("[TCP] Re-discovering FluidNC via mDNS...");
      mdnsDiscoverFluidNC();
    }
  }
}

// #7 TCP parse char buffer ile (heap fragmentation düzeltme)
void tcpReadIncoming() {
  while (tcpClient.available()) {
    char c = (char)tcpClient.read();
    if (c == '<') {
      tcpBufIdx = 0;
    } else if (c == '>') {
      if (tcpBufIdx > 0) {
        tcpBuf[tcpBufIdx] = '\0';
        Serial.printf("[TCP] RX: <%s>\n", tcpBuf);
        parseStatus(tcpBuf);
        lastDataReceived = millis();  // TCP veri geldi, uyku sıfırla
      }
      tcpBufIdx = 0;
    } else if (c != '\n' && c != '\r' && tcpBufIdx < sizeof(tcpBuf) - 1) {
      tcpBuf[tcpBufIdx++] = c;
    }
  }
}

// Sinyal gücü ikonu (ASCII)
const char *rssiIcon(int32_t rssi) {
  if (rssi > -50) return "||||";
  if (rssi > -65) return "||| ";
  if (rssi > -75) return "||  ";
  return "|   ";
}
