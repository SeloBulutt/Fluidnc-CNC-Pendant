#pragma once
/**
 * input.h — Encoder ve buton okuma modülü
 */
#include "config.h"

// Encoder
extern volatile int encDelta;
extern volatile int lastClkVal;
extern bool encClicked;
extern bool encLongPress;
extern bool encLongFired;

// Butonlar
extern Btn btns[4];
extern unsigned long homePressStart;
extern bool homeLongFired;

void IRAM_ATTR encISR();
void checkEncClick();
void readBtns();
