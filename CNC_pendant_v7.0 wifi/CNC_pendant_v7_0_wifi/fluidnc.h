#pragma once
/**
 * fluidnc.h — FluidNC komut gönderme ve status parse modülü
 */
#include "config.h"

// UART aktiflik takibi
extern unsigned long lastUartRx;
extern bool uartActive;
extern unsigned long lastUartStatus;

// Komut gönderme
void fcSend(const char *cmd);
void fcJog(uint8_t axis, int8_t dir, float dist, float feed);
void fcHome();
void fcZero(uint8_t ax);
void fcSendRealtime(uint8_t cmd);

// Override kısayolları
void sendFeedOvUp();
void sendFeedOvDown();
void sendFeedOvReset();
void sendSpnOvUp();
void sendSpnOvDown();
void sendSpnOvReset();

// Parse + okuma
void parseStatus(const char *s);
void readFluidNC();
