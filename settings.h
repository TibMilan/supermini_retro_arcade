#pragma once
// ============================================================
//  settings.h - Settings (Volume, Fenyero, Kepernyokimelo, Akku kalibracio)
//               + Statistics
// ============================================================
#include "config.h"
#include "ui.h"
#include "audio.h"
#include "battery.h"
#include "games.h"
#include "menu.h"

// ---------------- Settings ----------------
enum SettingsResult : uint8_t { SET_NONE, SET_BACK, SET_CALIB };

constexpr uint8_t SET_ENTRIES = 5;        // Összes menüpont száma (Volume, Brightness, Screensaver, Battery calib., Back)
constexpr uint8_t SET_VISIBLE_ROWS = 4;   // Egyszerre látható sorok száma a kijelzőn
static uint8_t set_sel = 0, set_top = 0;  // set_top követi a görgetést

inline void settingsOpen() { set_sel = 0; set_top = 0; }

inline SettingsResult settingsUpdate() {
  if (btnPressed(BTN_DOWN)) { set_sel = (set_sel + 1) % SET_ENTRIES; audioPlay(SFX_CLICK); }
  if (btnPressed(BTN_UP))   { set_sel = (set_sel + SET_ENTRIES - 1) % SET_ENTRIES; audioPlay(SFX_CLICK); }

  // Görgetés (Kamera követés) logikája
  if (set_sel < set_top) set_top = set_sel;
  if (set_sel >= set_top + SET_VISIBLE_ROWS) set_top = set_sel - SET_VISIBLE_ROWS + 1;

  if (set_sel == 0) {
    // Hangerő
    int8_t d = 0;
    if (btnPressed(BTN_LEFT))  d = -1;
    if (btnPressed(BTN_RIGHT)) d = +1;
    if (d) {
      int8_t v = (int8_t)g_volume + d;
      if (v >= 0 && v <= VOL_MAX) {
        storageSetVolume(v);
        audioTone(800 + v * 100, 30); // Teszt hang
      }
    }
  } else if (set_sel == 1) {
    // Fényerő
    int8_t d = 0;
    if (btnPressed(BTN_LEFT))  d = -1;
    if (btnPressed(BTN_RIGHT)) d = +1;
    if (d) {
      int8_t b = (int8_t)g_brightness + d;
      if (b >= 0 && b < BRI_COUNT) {
        storageSetBrightness(b);
        uiApplyBrightness();                       
        audioTone(800 + b * 150, 30);
      }
    }
  } else if (set_sel == 2) {
    // Képernyőkímélő
    int8_t d = 0;
    if (btnPressed(BTN_LEFT))  d = -1;
    if (btnPressed(BTN_RIGHT)) d = +1;
    if (d) {
      int8_t s = (int8_t)g_saverSel + d;
      if (s >= 0 && s < SAVER_COUNT) {
        storageSetSaver(s);
        audioTone(800 + s * 150, 30);
      }
    }
  } else if (set_sel == 3) {
    // Kalibráció
    if (btnPressed(BTN_CENTER)) { audioPlay(SFX_OK); return SET_CALIB; }
  } else {
    // Vissza (A 4-es index)
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_LEFT)) { audioPlay(SFX_CANCEL); return SET_BACK; }
  }
  return SET_NONE;
}

inline void drawBarTitle(const char* t, const uint8_t* icon) {   
  u8g2.setFont(u8g2_font_5x7_tr);
  int16_t w = u8g2.getStrWidth(t);
  int16_t iw = icon ? 10 : 0;
  int16_t x = BAR_MID_X + (BAR_MID_W - (w + iw)) / 2;
  if (icon) u8g2.drawXBM(x, 2, 8, 8, icon);
  u8g2.drawStr(x + iw, 9, t);
}

inline void settingsDraw() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  drawStatusBar();
  drawBarTitle("Settings", ICON_GEAR);

  static const char* const LABELS[SET_ENTRIES] = { "Volume", "Brightness", "Screensaver", "Battery calib.", "Back" };
  
  for (uint8_t i = 0; i < SET_VISIBLE_ROWS; i++) {
    uint8_t idx = set_top + i;
    if (idx >= SET_ENTRIES) break;
    
    int16_t y = 13 + i * 12; // Csak 4 fizikai sorunk van a kijelzőn
    if (idx == set_sel) u8g2.drawFrame(0, y, 128, 12);
    u8g2.setFont(u8g2_font_6x10_tr);
    
    if (idx == 0) {
      drawSpeakerIcon(3, y + 3, g_volume);
      u8g2.drawStr(15, y + 10, LABELS[idx]);
      for (uint8_t k = 0; k < VOL_MAX; k++) {
        int16_t x = 76 + k * 5;
        if (k < g_volume) u8g2.drawBox(x, y + 3, 4, 6);
        else              u8g2.drawHLine(x, y + 9, 4);
      }
    } else {
      u8g2.drawStr(4, y + 10, LABELS[idx]);
      if (idx == 1) {
        for (uint8_t k = 0; k < BRI_COUNT; k++) {
          int16_t x = 76 + k * 7;
          if (k <= g_brightness) u8g2.drawBox(x, y + 3, 6, 6);
          else                   u8g2.drawHLine(x, y + 9, 6);
        }
      } else if (idx == 2) {
        char b[16];
        snprintf(b, sizeof b, "%c%s%c", g_saverSel > 0 ? '<' : ' ', SAVER_LABELS[g_saverSel],
                 g_saverSel < SAVER_COUNT - 1 ? '>' : ' ');
        u8g2.drawStr(124 - u8g2.getStrWidth(b), y + 10, b);
      }
    }
  }
  u8g2.sendBuffer();
}

// ---------------- Statistics ----------------
static uint8_t st_page = 0;               
constexpr uint8_t ST_PAGES = GAME_COUNT + 1;

inline void statsOpen() { st_page = 0; }

// true = kilepes
inline bool statsUpdate() {
  if (btnPressed(BTN_DOWN) || btnPressed(BTN_RIGHT)) { st_page = (st_page + 1) % ST_PAGES; audioPlay(SFX_CLICK); }
  if (btnPressed(BTN_UP))                            { st_page = (st_page + ST_PAGES - 1) % ST_PAGES; audioPlay(SFX_CLICK); }
  if (btnPressed(BTN_CENTER) || btnPressed(BTN_LEFT)) { audioPlay(SFX_CANCEL); return true; }
  return false;
}

inline void fmtTime(char* out, size_t n, uint32_t s) {
  uint32_t h = s / 3600, m = (s / 60) % 60;
  if (h) snprintf(out, n, "%luh %02lum", (unsigned long)h, (unsigned long)m);
  else   snprintf(out, n, "%lum %02lus", (unsigned long)m, (unsigned long)(s % 60));
}

inline void statsDraw() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  drawStatusBar();

  char b[28], t[16];
  if (st_page == 0) {
    drawBarTitle("Statistics", ICON_STATS);
    uint32_t plays = 0, secs = 0;
    for (uint8_t g = 0; g < GAME_COUNT; g++) { plays += g_statPlays[g]; secs += g_statSecs[g]; }
    u8g2.setFont(u8g2_font_5x7_tr);
    snprintf(b, sizeof b, "Games played: %lu", (unsigned long)plays);
    u8g2.drawStr(4, 22, b);
    fmtTime(t, sizeof t, secs);
    snprintf(b, sizeof b, "Play time: %s", t);
    u8g2.drawStr(4, 31, b);
    snprintf(b, sizeof b, "Battery: %u.%02u V  %u%%", g_batMv / 1000, (g_batMv % 1000) / 10, g_batPercent);
    u8g2.drawStr(4, 40, b);
    fmtTime(t, sizeof t, millis() / 1000);
    snprintf(b, sizeof b, "On for: %s", t);
    u8g2.drawStr(4, 49, b);
  } else {
    uint8_t id = MENU_ORDER[st_page - 1];
    const GameDef& g = GAMES[id];
    drawBarTitle(g.shortName, g.icon);
    u8g2.setFont(u8g2_font_5x7_tr);
    snprintf(b, sizeof b, "Played: %lu", (unsigned long)g_statPlays[id]);
    u8g2.drawStr(4, 22, b);
    fmtTime(t, sizeof t, g_statSecs[id]);
    snprintf(b, sizeof b, "Time: %s", t);
    u8g2.drawStr(4, 31, b);
    for (uint8_t d = 0; d < DIFF_COUNT; d++) {
      uint32_t hs = g_highScore[id][d]; if (hs > 999999UL) hs = 999999UL;
      snprintf(b, sizeof b, "Best %-6s %lu", DIFF_NAMES[d], (unsigned long)hs);
      u8g2.drawStr(4, 40 + d * 9, b);
    }
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(b, sizeof b, "%u/%u", st_page + 1, ST_PAGES);
  u8g2.drawStr(124 - u8g2.getStrWidth(b), 22, b);
  u8g2.sendBuffer();
}