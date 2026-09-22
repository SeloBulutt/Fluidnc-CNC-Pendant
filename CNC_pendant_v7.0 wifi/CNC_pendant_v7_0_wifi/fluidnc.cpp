/**
 * fluidnc.cpp — FluidNC UART iletişim, komut gönderme, status parse
 * #7  char buffer ile heap fragmentation düzeltmesi
 * #14 fcSendRealtime() merkezi kullanım
 */
#include "fluidnc.h"

// TCP bağlantısı wifi_mgr modülünde tanımlı
extern bool tcpConnected;
extern WiFiClient tcpClient;

// ─── UART AKTİFLİK DEĞİŞKENLERİ ─────────────────────
unsigned long lastUartRx = 0;
bool uartActive = false;
unsigned long lastUartStatus = 0;

// ─── UART PARSE BUFFER (#7) ──────────────────────────
static char fcBuf[128];
static uint8_t fcBufIdx = 0;
static bool fcInPkt = false;

// ─── KOMUT GÖNDERME (#14) ────────────────────────────
void fcSend(const char *cmd) {
  if (uartActive) {
    FLUIDNC.println(cmd); // UART aktifse sadece UART'a gönder
  } else if (tcpConnected) {
    tcpClient.println(cmd); // UART yoksa TCP'ye gönder
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

// Tek byte realtime komut gönder (override, feed hold vb.)
void fcSendRealtime(uint8_t cmd) {
  if (uartActive) {
    FLUIDNC.write(cmd); // UART aktifse sadece UART'a gönder
  } else if (tcpConnected) {
    tcpClient.write(cmd); // UART yoksa TCP'ye gönder
  }
}

// Override kısayolları
void sendFeedOvUp() { fcSendRealtime(0x91); }    // Feed +10%
void sendFeedOvDown() { fcSendRealtime(0x92); }  // Feed -10%
void sendFeedOvReset() { fcSendRealtime(0x90); } // Feed Reset %100
void sendSpnOvUp() { fcSendRealtime(0x9A); }     // Spindle +10%
void sendSpnOvDown() { fcSendRealtime(0x9B); }   // Spindle -10%
void sendSpnOvReset() { fcSendRealtime(0x99); }  // Spindle Reset %100

// ─── STATUS PARSE (#7 char* ile, #13 enum ile) ───────
void parseStatus(const char *s) {
  // Makina durumu (#13 MachineState enum)
  if (strncmp(s, "Idle", 4) == 0)
    cur.state = ST_IDLE;
  else if (strncmp(s, "Run", 3) == 0)
    cur.state = ST_RUN;
  else if (strncmp(s, "Hold", 4) == 0)
    cur.state = ST_HOLD;
  else if (strncmp(s, "Alarm", 5) == 0)
    cur.state = ST_ALARM;
  else if (strncmp(s, "Home", 4) == 0) {
    cur.state = ST_HOME;
    cur.homed = true;
  } else if (strncmp(s, "Jog", 3) == 0)
    cur.state = ST_JOG;
  else if (strncmp(s, "Door", 4) == 0)
    cur.state = ST_DOOR;

  // MPos — strtof ile parse (heap alloc yok)
  const char *p = strstr(s, "MPos:");
  if (p) {
    p += 5;
    cur.x = strtof(p, (char **)&p);
    if (*p == ',') {
      p++;
      cur.y = strtof(p, (char **)&p);
    }
    if (*p == ',') {
      p++;
      cur.z = strtof(p, (char **)&p);
    }
  }

  // FS (Feed & Spindle)
  p = strstr(s, "FS:");
  if (p) {
    p += 3;
    cur.feed = strtof(p, (char **)&p);
    if (*p == ',') {
      p++;
      cur.spindle = strtof(p, (char **)&p);
    }
  }

  // WCO (Work Coordinate Offset)
  p = strstr(s, "WCO:");
  if (p) {
    p += 4;
    cur.wco_x = strtof(p, (char **)&p);
    if (*p == ',') {
      p++;
      cur.wco_y = strtof(p, (char **)&p);
    }
    if (*p == ',') {
      p++;
      cur.wco_z = strtof(p, (char **)&p);
    }
  }

  // Ov (Override yüzdeleri)
  p = strstr(s, "Ov:");
  if (p) {
    p += 3;
    cur.feedOv = (uint8_t)strtol(p, (char **)&p, 10);
    if (*p == ',') {
      p++;
      cur.rapidOv = (uint8_t)strtol(p, (char **)&p, 10);
    }
    if (*p == ',') {
      p++;
      cur.spindleOv = (uint8_t)strtol(p, (char **)&p, 10);
    }
  }
}

// ─── UART OKUMA (#7 char buffer) ─────────────────────
void readFluidNC() {
  while (FLUIDNC.available()) {
    lastUartRx = millis();  // UART'tan veri geldi
    char c = (char)FLUIDNC.read();
    if (c == '<') {
      fcBufIdx = 0;
      fcInPkt = true;
    } else if (c == '>') {
      if (fcInPkt) {
        fcBuf[fcBufIdx] = '\0';
        parseStatus(fcBuf);
      }
      fcInPkt = false;
      fcBufIdx = 0;
    } else if (fcInPkt && fcBufIdx < sizeof(fcBuf) - 1) {
      fcBuf[fcBufIdx++] = c;
    }
  }
}
