#pragma once
// ============================================================
//  title.h - jatek kezdolap: rekord, START, nehezseg, vissza
//  Statuszsav: bal = hangero, kozep = ikon + jatek neve, jobb = akku
//  Fel/Le: sor valasztas | Bal/Jobb (vagy Kozep) a nehezsegi soron: valtas
//  Kozep: START / VISSZA
// ============================================================
#include "config.h"
#include "ui.h"
#include "audio.h"
#include "games.h"

enum TitleResult : uint8_t { TITLE_NONE, TITLE_START, TITLE_BACK };

static uint8_t g_titleGame = 0;
static uint8_t g_titleRow  = 0;     // 0 = START, 1 = nehezseg, 2 = vissza

inline void titleOpen(uint8_t game) {
  g_titleGame = game;
  g_titleRow = 0;
}

inline void titleChangeDifficulty(int8_t dir) {
  uint8_t d = (g_diffSel[g_titleGame] + DIFF_COUNT + dir) % DIFF_COUNT;
  storageSetDifficulty(g_titleGame, d);
  audioTone(1000 + d * 250, 40);
}

inline TitleResult titleUpdate() {
  if (btnPressed(BTN_DOWN)) { g_titleRow = (g_titleRow + 1) % 3; audioPlay(SFX_CLICK); }
  if (btnPressed(BTN_UP))   { g_titleRow = (g_titleRow + 2) % 3; audioPlay(SFX_CLICK); }

  if (g_titleRow == 1) {
    if (btnPressed(BTN_LEFT))  titleChangeDifficulty(-1);
    if (btnPressed(BTN_RIGHT)) titleChangeDifficulty(+1);
    if (btnPressed(BTN_CENTER)) titleChangeDifficulty(+1);
  } else if (btnPressed(BTN_CENTER)) {
    if (g_titleRow == 0) { audioPlay(SFX_OK);     return TITLE_START; }
    else                 { audioPlay(SFX_CANCEL); return TITLE_BACK;  }
  }
  return TITLE_NONE;
}

inline void titleDraw() {
  const GameDef& g = GAMES[g_titleGame];
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  drawStatusBar();                                   // bal: hangero, jobb: akku

  // Kozepen: ikon + nev (ha a teljes nev nem fer ki, a rovid nev)
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* nm = g.name;
  int16_t w = u8g2.getStrWidth(nm);
  if (w + 10 > BAR_MID_W - 2) { nm = g.shortName; w = u8g2.getStrWidth(nm); }
  int16_t gx = BAR_MID_X + (BAR_MID_W - (w + 10)) / 2;
  u8g2.drawXBM(gx, 2, 8, 8, g.icon);
  u8g2.drawStr(gx + 10, 9, nm);

  // Rekord az aktualis nehezsegen
  char b[32];
  uint32_t hs = g_highScore[g_titleGame][g_diffSel[g_titleGame]];
  if (hs > 999999UL) hs = 999999UL;
  snprintf(b, sizeof b, "Best: %lu", (unsigned long)hs);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(b)) / 2, 24, b);

  // Sorok
  const char* labels[3];
  labels[0] = "START";
  snprintf(b, sizeof b, "Level: <%s>", DIFF_NAMES[g_diffSel[g_titleGame]]);
  labels[1] = b;
  labels[2] = "Back";
  for (uint8_t r = 0; r < 3; r++) {
    int16_t y = 27 + r * 12;
    if (r == g_titleRow) u8g2.drawFrame(2, y, 124, 12);
    u8g2.drawStr((128 - u8g2.getStrWidth(labels[r])) / 2, y + 10, labels[r]);
  }
  u8g2.sendBuffer();
}
