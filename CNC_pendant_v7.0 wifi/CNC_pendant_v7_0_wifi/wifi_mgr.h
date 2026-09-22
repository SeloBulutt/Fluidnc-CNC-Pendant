#pragma once
/**
 * wifi_mgr.h — WiFi, mDNS, TCP bağlantı yönetimi
 */
#include "config.h"

// WiFi durumu
extern Preferences prefs;
extern WiFiClient tcpClient;
extern bool tcpConnected;
extern bool wifiConnected;
extern String wifiSSID, wifiPass;
extern uint8_t wifiIP[4];
extern bool mdnsStarted;
extern uint8_t tcpFailCount;
extern unsigned long lastTcpReconnect;
extern unsigned long lastTcpStatus;
extern unsigned long lastDebugLog;

// WiFi tarama
extern String scanSSIDs[WIFI_MAX_SCAN];
extern int32_t scanRSSI[WIFI_MAX_SCAN];
extern int scanCount;
extern int wifiScanIdx;
extern bool wifiScanRunning;

// WiFi menü
extern int wifiMenuIdx;
extern int charIdx;
extern String passBuffer;
extern uint8_t ipEditIdx;

// WiFi bağlantı state machine (#5)
extern WifiConnState wcsState;

// Preferences
void wifiSavePrefs();
void wifiLoadPrefs();
void saveUserPrefs();
void loadUserPrefs();

// WiFi bağlantı (#5 non-blocking)
void wifiConnectStart();
WifiConnState wifiConnectUpdate();
void wifiDisconnect();

// WiFi tarama (#6 async)
void wifiStartScanAsync();
bool wifiScanUpdate();

// mDNS + TCP
bool mdnsDiscoverFluidNC();
void tcpStartConnection();
void tcpReadIncoming();

// Yardımcı
const char *rssiIcon(int32_t rssi);
