#pragma once
// ============================================================
//  storage.h - Preferences (NVS): hangero, nehezseg, high score-ok,
//              akku kalibracios szorzo
//  A rekordok jatekonkent ES nehezsegenkent kulon tarolodnak.
// ============================================================
#include <Preferences.h>
#include "config.h"

static const char* const DIFF_NAMES[DIFF_COUNT] = { "Easy", "Medium", "Hard" };

// ---- Beallitasok: fenyero + kepernyokimelo ideje ----
static const uint8_t  BRI_COUNT = 7;
static const uint8_t  BRI_VALUES[BRI_COUNT] = { 10, 40, 80, 120, 180, 220, 255 };   // SSD1306 contrast
static const uint8_t  BRI_DEFAULT = 4;
static const uint8_t  SAVER_COUNT = 6;
static const uint32_t SAVER_TIMES[SAVER_COUNT] = { 30000UL, 60000UL, 180000UL, 300000UL, 600000UL, 0UL };  // 0 = soha
static const char* const SAVER_LABELS[SAVER_COUNT] = { "30 s", "1 min", "3 min", "5 min", "10 min", "Never" };

static Preferences g_prefs;
static uint8_t     g_brightness = BRI_DEFAULT;
static uint8_t     g_saverSel   = 0;
// ---- Statisztika: jatekonkent inditasok + jatekido (masodperc) ----
static uint32_t    g_statPlays[GAME_COUNT];
static uint32_t    g_statSecs[GAME_COUNT];
static uint32_t    g_statMsCarry[GAME_COUNT];   // a masodperc alatti maradek
static uint16_t    g_statDirty = 0;             // bitmaszk: mely jatek statjat kell menteni
static uint8_t     g_volume = VOL_DEFAULT;
static uint32_t    g_highScore[GAME_COUNT][DIFF_COUNT];
static uint8_t     g_diffSel[GAME_COUNT];       // jatekonkent utoljara valasztott nehezseg
static uint16_t    g_batCal = 1000;             // akku kalibracios szorzo ezrelekben (1000 = 1.000)

static bool     g_volDirty   = false;
static uint32_t g_volDirtyAt = 0;
static bool     g_calDirty   = false;
static uint32_t g_calDirtyAt = 0;

inline void storageBegin() {
  g_prefs.begin("konzol", false);
  g_volume = g_prefs.getUChar("vol", VOL_DEFAULT);
  if (g_volume > VOL_MAX) g_volume = VOL_DEFAULT;
  g_brightness = g_prefs.getUChar("bri", BRI_DEFAULT);
  if (g_brightness >= BRI_COUNT) g_brightness = BRI_DEFAULT;
  g_saverSel = g_prefs.getUChar("sav", 0);
  if (g_saverSel >= SAVER_COUNT) g_saverSel = 0;
  g_batCal = g_prefs.getUShort("bcal", 1000);
  if (g_batCal < 800 || g_batCal > 1200) g_batCal = 1000;
  for (uint8_t g = 0; g < GAME_COUNT; g++) {
    char key[8];
    snprintf(key, sizeof key, "f%u", g);
    snprintf(key, sizeof key, "n%u", g); g_statPlays[g] = g_prefs.getUInt(key, 0);
    snprintf(key, sizeof key, "t%u", g); g_statSecs[g]  = g_prefs.getUInt(key, 0);
    g_statMsCarry[g] = 0;
    snprintf(key, sizeof key, "f%u", g);
    g_diffSel[g] = g_prefs.getUChar(key, DIFF_MEDIUM);
    if (g_diffSel[g] >= DIFF_COUNT) g_diffSel[g] = DIFF_MEDIUM;
    for (uint8_t d = 0; d < DIFF_COUNT; d++) {
      snprintf(key, sizeof key, "s%u%u", g, d);
      g_highScore[g][d] = g_prefs.getUInt(key, 0);
    }
  }
}

// A hangerot / kalibraciot nem irjuk minden lepesnel a flash-be: 1.5 s nyugalom utan mentjuk.
inline void storageSetVolume(uint8_t v) {
  if (v > VOL_MAX) v = VOL_MAX;
  if (v == g_volume) return;
  g_volume = v;
  g_volDirty = true;
  g_volDirtyAt = millis();
}

inline void storageSetBatCal(uint16_t c) {
  if (c < 800) c = 800;
  if (c > 1200) c = 1200;
  if (c == g_batCal) return;
  g_batCal = c;
  g_calDirty = true;
  g_calDirtyAt = millis();
}

// Beallitasok: ritkan valtoznak -> azonnal mentjuk
inline void storageSetBrightness(uint8_t b) {
  if (b >= BRI_COUNT || b == g_brightness) return;
  g_brightness = b;
  g_prefs.putUChar("bri", b);
}
inline void storageSetSaver(uint8_t s) {
  if (s >= SAVER_COUNT || s == g_saverSel) return;
  g_saverSel = s;
  g_prefs.putUChar("sav", s);
}

// Statisztika: RAM-ban gyujtjuk, jatekbol kilepeskor / alvas elott irjuk a flash-be
inline void statAddPlay(uint8_t game) {
  if (game >= GAME_COUNT) return;
  g_statPlays[game]++;
  g_statDirty |= (1u << game);
}
inline void statAddTime(uint8_t game, uint32_t ms) {
  if (game >= GAME_COUNT) return;
  g_statMsCarry[game] += ms;
  if (g_statMsCarry[game] >= 1000) {
    g_statSecs[game] += g_statMsCarry[game] / 1000;
    g_statMsCarry[game] %= 1000;
    g_statDirty |= (1u << game);
  }
}
inline void statFlush() {
  if (!g_statDirty) return;
  for (uint8_t g = 0; g < GAME_COUNT; g++) {
    if (!(g_statDirty & (1u << g))) continue;
    char key[8];
    snprintf(key, sizeof key, "n%u", g); g_prefs.putUInt(key, g_statPlays[g]);
    snprintf(key, sizeof key, "t%u", g); g_prefs.putUInt(key, g_statSecs[g]);
  }
  g_statDirty = 0;
}

inline void storageFlush() {              // alvas / kikapcsolas elott
  statFlush();
  if (g_volDirty) { g_prefs.putUChar("vol", g_volume); g_volDirty = false; }
  if (g_calDirty) { g_prefs.putUShort("bcal", g_batCal); g_calDirty = false; }
}

inline void storageUpdate() {
  if ((g_volDirty && millis() - g_volDirtyAt > 1500) ||
      (g_calDirty && millis() - g_calDirtyAt > 1500)) storageFlush();
}

inline void storageSetDifficulty(uint8_t game, uint8_t diff) {
  if (game >= GAME_COUNT || diff >= DIFF_COUNT || g_diffSel[game] == diff) return;
  g_diffSel[game] = diff;
  char key[8];
  snprintf(key, sizeof key, "f%u", game);
  g_prefs.putUChar(key, diff);
}

// Igaz, ha uj rekord szuletett (ekkor ment is).
inline bool storageSubmitScore(uint8_t game, uint8_t diff, uint32_t score) {
  if (game >= GAME_COUNT || diff >= DIFF_COUNT || score <= g_highScore[game][diff]) return false;
  g_highScore[game][diff] = score;
  char key[8];
  snprintf(key, sizeof key, "s%u%u", game, diff);
  g_prefs.putUInt(key, score);
  return true;
}
