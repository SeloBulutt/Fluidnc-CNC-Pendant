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

// SD Kart (#21)
String sdFiles[SD_MAX_FILES];
int sdFileCount = 0;
int sdSelIdx = 0;
bool sdListPending = false;
String sdSelectedFile = "";
String sdPreviewLines[SD_PREVIEW_MAX_LINES];
int sdPreviewLineCount = 0;
int sdTotalLineCount = 0;
bool sdShowPending = false;
int sdPreviewScroll = 0;
bool sdJobRunningView = false;
static char sdLineBuf[128];
static uint8_t sdLineBufIdx = 0;

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

void fcGoToZero() {
  // Once Z eksenini guvenli yukseklige cikar
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "G90 G0 Z%d F%d", GOTO_ZERO_Z_CLEARANCE, GOTO_ZERO_FEED);
  fcSend(cmd);
  // Sonra XY sifirina git
  snprintf(cmd, sizeof(cmd), "G90 G0 X0 Y0 F%d", GOTO_ZERO_FEED);
  fcSend(cmd);
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

// Alarm mesaj tablosu (#22)
const char* getAlarmMsg(uint8_t code) {
  switch (code) {
    case 1:  return "Hard Limit";
    case 2:  return "Soft Limit";
    case 3:  return "Abort";
    case 4:  return "Probe Fail";
    case 5:  return "Probe Safety";
    case 6:  return "Homing Fail";
    case 7:  return "Homing Pull-off";
    case 8:  return "Homing Switch";
    case 9:  return "Homing Required";
    case 10: return "Spindle Err";
    default: return "Bilinmiyor";
  }
}

// ─── STATUS PARSE (#7 char* ile, #13 enum ile) ───────
void parseStatus(const char *s) {
  // Makina durumu (#13 MachineState enum)
  if (strncmp(s, "Idle", 4) == 0) {
    cur.state = ST_IDLE;
    cur.alarmCode = 0;
  } else if (strncmp(s, "Run", 3) == 0) {
    cur.state = ST_RUN;
    cur.alarmCode = 0;
  } else if (strncmp(s, "Hold", 4) == 0) {
    cur.state = ST_HOLD;
    cur.alarmCode = 0;
  } else if (strncmp(s, "Alarm", 5) == 0) {
    cur.state = ST_ALARM;
    if (s[5] == ':') {
      cur.alarmCode = (uint8_t)atoi(s + 6);
    } else {
      cur.alarmCode = 0;
    }
  } else if (strncmp(s, "Home", 4) == 0) {
    cur.state = ST_HOME;
    cur.homed = true;
    cur.alarmCode = 0;
  } else if (strncmp(s, "Jog", 3) == 0) {
    cur.state = ST_JOG;
    cur.alarmCode = 0;
  } else if (strncmp(s, "Door", 4) == 0) {
    cur.state = ST_DOOR;
    cur.alarmCode = 0;
  }

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

  // SD (SD kart ilerleme yuzdesi)
  p = strstr(s, "SD:");
  if (p) {
    p += 3;
    cur.sdPercent = strtof(p, (char **)&p);
  }

  // Ln (G-code satir no)
  p = strstr(s, "Ln:");
  if (p) {
    p += 3;
    cur.sdLine = (uint32_t)strtoul(p, (char **)&p, 10);
  } else if (cur.state == ST_RUN || cur.state == ST_HOLD) {
    uint32_t total = (sdTotalLineCount > 0) ? (uint32_t)sdTotalLineCount : (uint32_t)sdPreviewLineCount;
    if (total > 0 && cur.sdPercent > 0.0f) {
      uint32_t calcLine = (uint32_t)roundf((cur.sdPercent / 100.0f) * total);
      if (calcLine < 1) calcLine = 1;
      if (calcLine > total) calcLine = total;
      cur.sdLine = calcLine;
    }
  }
}

// SD Kart komutlari (#21)
void fcSdList() {
  sdFileCount = 0;
  sdSelIdx = 0;
  sdListPending = true;
  fcSend("$SD/List");
}

void fcSdRun(const char* file) {
  if (!file || strlen(file) == 0) return;
  while (*file == ' ') file++;

  char path[64];
  if (file[0] == '/') {
    snprintf(path, sizeof(path), "%s", file);
  } else {
    snprintf(path, sizeof(path), "/%s", file);
  }

  char cmd[96];
  snprintf(cmd, sizeof(cmd), "$SD/Run=%s", path);
  Serial.printf("[SD] Starting job: %s\n", cmd);
  fcSend(cmd);
}
void fcSdShow(const char* file) {
  if (!file || strlen(file) == 0) return;
  while (*file == ' ') file++;

  sdPreviewLineCount = 0;
  sdTotalLineCount = 0;
  for (int i = 0; i < SD_PREVIEW_MAX_LINES; i++) {
    sdPreviewLines[i] = "";
  }
  sdShowPending = true;

  char path[64];
  if (file[0] == '/') {
    snprintf(path, sizeof(path), "%s", file);
  } else {
    snprintf(path, sizeof(path), "/%s", file);
  }

  char cmd[80];
  snprintf(cmd, sizeof(cmd), "$SD/Show=%s", path);
  fcSend(cmd);
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
    } else if (fcInPkt) {
      if (fcBufIdx < sizeof(fcBuf) - 1) {
        fcBuf[fcBufIdx++] = c;
      }
    } else {
      // Sadece paket disindayken SD parse
      parseSdChar(c);
    }
  }
}

void parseSdChar(char c) {
  if (c == '\n' || c == '\r') {
    if (sdLineBufIdx > 0) {
      sdLineBuf[sdLineBufIdx] = '\0';
      Serial.printf("[SD-LINE] %s\n", sdLineBuf);

      if (sdListPending) {
        // [FILE: dosya.nc|SIZE:1234] formatini ara
        char *fileTag = strstr(sdLineBuf, "[FILE:");
        if (!fileTag) fileTag = strstr(sdLineBuf, "[FILE :");
        if (!fileTag) fileTag = strstr(sdLineBuf, "FILE:");

        if (fileTag && sdFileCount < SD_MAX_FILES) {
          char *fname = strchr(fileTag, ':') + 1;
          while (*fname == ' ') fname++;

          // '|' veya ']' veya kontrol karakterlerinde kes
          char *p = fname;
          while (*p) {
            if (*p == '|' || *p == ']' || *p == '\r' || *p == '\n') {
              *p = '\0';
              break;
            }
            p++;
          }
          // Sondaki bosluklari kirp
          int len = strlen(fname);
          while (len > 0 && fname[len - 1] == ' ') {
            fname[--len] = '\0';
          }

          if (len > 0) {
            bool exists = false;
            for (int i = 0; i < sdFileCount; i++) {
              if (sdFiles[i].equalsIgnoreCase(fname)) {
                exists = true;
                break;
              }
            }
            if (!exists) {
              sdFiles[sdFileCount] = String(fname);
              sdFileCount++;
              needRedraw = true; // Yeni dosya eklenince hemen ciz
            }
          }
        }

        // Liste sonu kontrolu
        if (strstr(sdLineBuf, "End") || strstr(sdLineBuf, "ok") || strstr(sdLineBuf, "[MSG:Done")) {
          sdListPending = false;
          needRedraw = true; // Liste bitti, 'Yukleniyor...' aninda kalksin
        }
      }

      if (sdShowPending) {
        // Onizleme sonu veya hata kontrolu
        if (strstr(sdLineBuf, "End") || strcmp(sdLineBuf, "ok") == 0 ||
            strstr(sdLineBuf, "[MSG:Done") || strncmp(sdLineBuf, "error:", 6) == 0) {
          sdShowPending = false;
          needRedraw = true;
        } else {
          // Satir basi bosluklari gec
          char *p = sdLineBuf;
          while (*p == ' ' || *p == '\t') p++;
          // Durum paketi veya sistem mesaji degilse
          if (*p != '\0' && *p != '<' && strncmp(p, "[MSG:", 5) != 0 && strncmp(p, "[GC:", 4) != 0) {
            sdTotalLineCount++;
            if (sdPreviewLineCount < SD_PREVIEW_MAX_LINES) {
              String lineStr = String(p);
              if (lineStr.length() > SD_PREVIEW_LINE_LEN) {
                lineStr = lineStr.substring(0, SD_PREVIEW_LINE_LEN);
              }
              sdPreviewLines[sdPreviewLineCount++] = lineStr;
              needRedraw = true;
            }
          }
        }
      }

      sdLineBufIdx = 0;
    }
  } else if (sdLineBufIdx < sizeof(sdLineBuf) - 1) {
    sdLineBuf[sdLineBufIdx++] = c;
  }
}
