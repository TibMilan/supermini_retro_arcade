#pragma once
// ============================================================
//  menu.h - gorgetheto fomenu: jatekok ABC sorrendben +
//           Settings + Statistics
//  Fel/Le: lepkedes (korbeer) | Kozep: kivalasztas
// ============================================================
#include "config.h"
#include "ui.h"
#include "audio.h"
#include "games.h"

constexpr uint8_t MENU_SETTINGS = GAME_COUNT;
constexpr uint8_t MENU_STATS    = GAME_COUNT + 1;
constexpr uint8_t MENU_ENTRIES  = GAME_COUNT + 2;
constexpr uint8_t MENU_ROWS     = 4;
constexpr int8_t  MENU_RESULT_SETTINGS = 100;        // menuUpdate() visszateres
constexpr int8_t  MENU_RESULT_STATS    = 101;

static const uint8_t ICON_GEAR[8]  = { 0x18, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x18 };
static const uint8_t ICON_STATS[8] = { 0x40, 0x40, 0x44, 0x44, 0x54, 0x54, 0x55, 0x55 };

static uint8_t menuSel = 0, menuTop = 0;

inline int8_t menuUpdate() {
  if (btnPressed(BTN_DOWN)) { menuSel = (menuSel + 1) % MENU_ENTRIES; audioPlay(SFX_CLICK); }
  if (btnPressed(BTN_UP))   { menuSel = (menuSel + MENU_ENTRIES - 1) % MENU_ENTRIES; audioPlay(SFX_CLICK); }

  if (menuSel < menuTop) menuTop = menuSel;
  if (menuSel >= menuTop + MENU_ROWS) menuTop = menuSel - MENU_ROWS + 1;

  if (menuSel == MENU_SETTINGS) {
    if (btnPressed(BTN_CENTER)) { audioPlay(SFX_OK); return MENU_RESULT_SETTINGS; }
  } else if (menuSel == MENU_STATS) {
    if (btnPressed(BTN_CENTER)) { audioPlay(SFX_OK); return MENU_RESULT_STATS; }
  } else if (btnPressed(BTN_CENTER)) {
    audioPlay(SFX_OK);
    return MENU_ORDER[menuSel];
  }
  return -1;
}

inline void menuDraw() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  drawStatusBar();

  for (uint8_t i = 0; i < MENU_ROWS; i++) {
    uint8_t idx = menuTop + i;
    if (idx >= MENU_ENTRIES) break;
    int16_t y = 13 + i * 12;                         
    if (idx == menuSel) u8g2.drawFrame(0, y, 128, 12);

    u8g2.setFont(u8g2_font_6x10_tr);
    if (idx < GAME_COUNT) {
      const GameDef& g = GAMES[MENU_ORDER[idx]];
      u8g2.drawXBM(3, y + 2, 8, 8, g.icon);
      u8g2.drawStr(15, y + 10, g.name);
    } else if (idx == MENU_SETTINGS) {
      u8g2.drawXBM(3, y + 2, 8, 8, ICON_GEAR);
      u8g2.drawStr(15, y + 10, "Settings");
    } else {
      u8g2.drawXBM(3, y + 2, 8, 8, ICON_STATS);
      u8g2.drawStr(15, y + 10, "Statistics");
    }
  }
  u8g2.sendBuffer();
}