#pragma once
// ============================================================
//  calib.h - akku-meres kalibracios kepernyo
//
//  Hasznalat: multimeterrel merd meg a LiPo valodi feszultseget, es
//  Fel/Le (+-1%) ill. Bal/Jobb (+-0.1%) gombokkal allitsd a kijelzett
//  "Akku" mV erteket a mertre. A szorzo automatikusan mentodik.
//  Kozep = kesz.
// ============================================================
#include "config.h"
#include "ui.h"
#include "audio.h"
#include "battery.h"

static uint32_t cal_lastSample = 0;

inline void calibOpen() {
  batForceSample();
  cal_lastSample = millis();
}

// true = kilepes
inline bool calibUpdate() {
  if (millis() - cal_lastSample >= 250) {
    cal_lastSample = millis();
    batForceSample();
  }
  int16_t c = g_batCal;
  if (btnPressed(BTN_UP))    c += 10;
  if (btnPressed(BTN_DOWN))  c -= 10;
  if (btnPressed(BTN_RIGHT)) c += 1;
  if (btnPressed(BTN_LEFT))  c -= 1;
  if (c < 800)  c = 800;
  if (c > 1200) c = 1200;
  if (c != g_batCal) {
    storageSetBatCal(c);
    batForceSample();
    audioTone(1200, 20);
  }
  if (btnPressed(BTN_CENTER)) {
    audioPlay(SFX_OK);
    storageFlush();
    return true;
  }
  return false;
}

inline void calibDraw() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  drawStatusBar();

  u8g2.setFont(u8g2_font_5x7_tr);
  const char* t = "Battery cal.";
  u8g2.drawStr(BAR_MID_X + (BAR_MID_W - u8g2.getStrWidth(t)) / 2, 9, t);

  char b[32];
  snprintf(b, sizeof b, "Pin:   %u mV", g_batPinMv);
  u8g2.drawStr(4, 21, b);
  snprintf(b, sizeof b, "Batt:  %u mV  %u%%", g_batMv, g_batPercent);
  u8g2.drawStr(4, 30, b);
  snprintf(b, sizeof b, "Factor: %u.%03u", g_batCal / 1000, g_batCal % 1000);
  u8g2.drawStr(4, 39, b);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(4, 48, "Match your multimeter!");
  u8g2.drawStr(4, 55, "U/D: +-1%  L/R: +-0.1%");
  u8g2.drawStr(4, 62, "Center: done");
  u8g2.sendBuffer();
}
