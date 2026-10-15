#pragma once
// ============================================================
//  battery.h - LiPo toltottseg meres (PIN_BAT, 2x100k osztó)
//  + felhasznaloi kalibracio (g_batCal) + kritikus szint figyeles
//
//  mV = ADC-pin mV * BAT_DIV_RATIO * g_batCal / 1000
// ============================================================
#include "config.h"
#include "storage.h"

static uint16_t g_batPinMv = 0;     // nyers ADC-pin feszultseg (kalibracios kepernyohoz)
static uint16_t g_batMv = 4000;
static uint8_t  g_batPercent = 100;
static bool     g_batCritical = false;
static uint8_t  bat_lowCount = 0;
static uint32_t bat_lastRead = 0;
static bool     bat_first = true;

// Tipikus 1S LiPo terheletlen feszultseg -> szazalek.
// A toltes vegen az akku nyugalmi feszultsege ~4.15-4.19 V, ezert 4150 mV mar 100%.
static const uint16_t BAT_MV[]  = { 3300, 3500, 3600, 3700, 3750, 3800, 3850, 3920, 4000, 4080, 4150 };
static const uint8_t  BAT_PCT[] = {    0,    5,   10,   20,   30,   40,   50,   65,   80,   92,  100 };

inline uint8_t batMvToPercent(uint16_t mv) {
  const uint8_t n = sizeof(BAT_MV) / sizeof(BAT_MV[0]);
  if (mv <= BAT_MV[0]) return 0;
  if (mv >= BAT_MV[n - 1]) return 100;
  for (uint8_t i = 1; i < n; i++) {
    if (mv <= BAT_MV[i]) {
      uint32_t span = BAT_MV[i] - BAT_MV[i - 1];
      uint32_t off  = mv - BAT_MV[i - 1];
      return BAT_PCT[i - 1] + (uint32_t)(BAT_PCT[i] - BAT_PCT[i - 1]) * off / span;
    }
  }
  return 100;
}

inline uint16_t batReadMv() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_BAT);
  g_batPinMv = (uint16_t)(sum / 16);
  return (uint16_t)((sum / 16.0f) * BAT_DIV_RATIO * g_batCal / 1000.0f);
}

inline void batSample() {
  uint16_t mv = batReadMv();
  if (bat_first) { g_batMv = mv; bat_first = false; }
  else g_batMv = (g_batMv * 3 + mv) / 4;       // egyszeru simitas
  g_batPercent = batMvToPercent(g_batMv);

  // Kritikus szint: 3 egymas utani mintan (~6 mp) at alacsony kell legyen,
  // hogy a terheles okozta pillanatnyi feszultsegesés ne kapcsoljon ki.
  // Ha nincs akku (csak USB, ~0 mV) az nem szamit.
  if (g_batMv >= BAT_MIN_VALID_MV && g_batMv < BAT_CRITICAL_MV) {
    if (bat_lowCount < 255) bat_lowCount++;
  } else {
    bat_lowCount = 0;
  }
  g_batCritical = (bat_lowCount >= 3);
}

// Kalibracios kepernyohoz: simitas nelkuli, azonnali mintavetel (a kritikus szamlalot nem erinti)
inline void batForceSample() {
  g_batMv = batReadMv();
  bat_first = false;
  g_batPercent = batMvToPercent(g_batMv);
}

inline void batBegin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT, ADC_11db);
  batSample();
  bat_lastRead = millis();
}

// loop()-bol; 2 masodpercenkent mer
inline void batUpdate() {
  if (millis() - bat_lastRead >= 2000) {
    bat_lastRead = millis();
    batSample();
  }
}
