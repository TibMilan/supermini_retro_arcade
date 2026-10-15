#pragma once
// ============================================================
//  saver.h - roboeyes kori szemek:
//    saverRun()      - kepernyokimelo (tetlenseg), true = gombnyomas megszakitotta
//    wakeAnimation() - deep-sleepbol ebredes: almos szemek kinyilnak, korbenéznek
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"

// Egy kepkocka: ket kerek szem (ex,ey = eltolas, eh = magassag, happy = "^ ^" szem)
inline void drawEyes(float ex, float ey, int16_t eh, bool happy) {
  const int16_t EW = 30, GAP = 12;
  int16_t left0 = (128 - (2 * EW + GAP)) / 2;
  int16_t top = 32 - eh / 2 + (int16_t)ey;
  int16_t r = eh / 2 < 8 ? eh / 2 : 8;

  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  for (uint8_t e = 0; e < 2; e++) {
    int16_t x = left0 + e * (EW + GAP) + (int16_t)ex;
    u8g2.drawRBox(x, top, EW, eh, r);
    if (happy && eh > 20) {                             // alulrol kiharapott "^ ^" szem
      u8g2.setDrawColor(0);
      u8g2.drawDisc(x + EW / 2, top + eh + 4, 15);
      u8g2.setDrawColor(1);
    }
  }
  u8g2.sendBuffer();
}

inline float eyeSmooth(float x) {                        // 0..1 -> lagy atmenet
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  return x * x * (3 - 2 * x);
}

// Visszaad: true = gombnyomas megszakitotta, false = lejart (-> deep-sleep)
inline bool saverRun() {
  static const int8_t LOOK[][2] = { {-14, 0}, {14, 0}, {0, -9}, {-14, 8}, {14, -8}, {0, 0}, {10, 8}, {-10, -8} };
  const uint8_t LOOK_N = sizeof(LOOK) / sizeof(LOOK[0]);

  uint32_t start = millis(), lastF = 0;
  uint32_t nextLook = start + 400, blinkAt = start + 1500, blinkEnd = 0;
  float ex = 0, ey = 0;
  int8_t tx = 0, ty = 0;
  uint8_t li = 0;

  while (millis() - start < SAVER_MS) {
    inputPoll();
    if ((int32_t)(in_lastActivity - start) >= 0) return true;   // gombnyomas -> vissza
    if (millis() - lastF < 33) { delay(1); continue; }
    lastF = millis();
    uint32_t now = lastF;

    if (now >= nextLook) {
      tx = LOOK[li % LOOK_N][0];
      ty = LOOK[li % LOOK_N][1];
      li++;
      nextLook = now + 700;
    }
    ex += (tx - ex) * 0.3f;
    ey += (ty - ey) * 0.3f;

    if (now >= blinkAt) {
      blinkEnd = now + 140;
      blinkAt = now + 1800 + (esp_random() % 1200);
    }
    bool blink = now < blinkEnd;
    bool happy = (now - start) > (SAVER_MS - 1600);     // az utolso pillanatokban boldog szemek

    drawEyes(ex, ey, blink ? 4 : 28, happy && !blink);
  }
  return false;
}

// Ebredesi animacio (~2.4 mp): csukott szem -> almos felnyitas -> lassu pislogas ->
// teljesen kinyilik -> balra, jobbra nez -> boldog szemek. Kozben vidam csipogas.
inline void wakeAnimation() {
  static const Note CHIRP[] = { {659, 70}, {988, 70}, {1319, 120} };
  audioPlay(CHIRP);

  const uint32_t TOTAL = 2400;
  uint32_t start = millis(), lastF = 0;
  while (millis() - start < TOTAL) {
    audioUpdate();
    if (millis() - lastF < 33) { delay(1); continue; }
    lastF = millis();
    float t = (float)(lastF - start);

    float eh;
    if      (t < 300) eh = 2;
    else if (t < 700) eh = 2 + 10 * eyeSmooth((t - 300) / 400);          // almosan felnyilik
    else if (t < 900) eh = 12 - 9 * eyeSmooth((t - 700) / 200);          // lassu pislogas
    else if (t < 1300) eh = 3 + 25 * eyeSmooth((t - 900) / 400);         // teljesen kinyilik
    else eh = 28;

    float ex = 0;
    if      (t >= 1300 && t < 1650) ex = -12 * eyeSmooth((t - 1300) / 350);       // balra
    else if (t >= 1650 && t < 2000) ex = -12 + 24 * eyeSmooth((t - 1650) / 350);  // jobbra
    else if (t >= 2000)             ex = 12 - 12 * eyeSmooth((t - 2000) / 200);   // kozepre

    drawEyes(ex, 0, (int16_t)eh, t > 2100);
  }
  audioStop();
}
