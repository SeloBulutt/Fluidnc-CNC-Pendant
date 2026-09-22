#pragma once
/**
 * display.h — Tüm ekran çizim fonksiyonları
 */
#include "config.h"

// Popup (#4 non-blocking)
extern PopupState popupState;
extern int cachedBatPct;

void showPopup(const char *msg, uint16_t bgCol, uint32_t dur);
bool updatePopup();
void updateBattery();

// Durum yardımcıları
uint16_t stateColor(uint8_t s);
const char *stateStr(uint8_t s);
int getMenuCount();

// Ana ekran
void drawHeader();
void drawAxisRows();
void drawFooter();
void drawMainAll();
void updateMainDisplay();

// Menü
void drawMenuScreen();

// Alt ekranlar
void drawSpindleScreen();
void drawJogScreen();
void drawStepScreen();
void drawCoolantScreen();

// Override (#15 birleştirilmiş)
void drawOverrideGauge(const char *title, const char *label,
                       uint8_t value, bool isRunning);
void drawOverrideSelScreen();

// WiFi ekranları
void drawWifiMenuScreen();
void drawWifiScanScreen();
void drawWifiPassScreen();
void drawWifiIPScreen();

// Z Probe (#18)
void drawProbeScreen();
