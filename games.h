#pragma once
// ============================================================
//  games.h - jatek-regiszter (ID sorrendben) + ikonok + TESZT-jatek (stub)
//
//  Minden jatek egy GameDef: nev, rovid nev, 8x8 ikon, layout, init/update/draw.
//  A jatek-modulok elkeszultekor a stub-pointereket cseréljuk a valodiakra.
//  A jatek a g_hud strukturaban frissiti a pontszamot / eleteket, a
//  nehezseget a g_difficulty valtozobol olvassa (DIFF_EASY/MEDIUM/HARD).
//  FONTOS: a jatek-ID a tomb indexe, ez a mentes kulcsa - ne rendezd at utolag!
//  Uj jatek hozzaadasa = uj ID (GAME_COUNT novelese a config.h-ban).
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

static Difficulty g_difficulty = DIFF_MEDIUM;   // az eppen futo jatek nehezsege
static uint8_t    g_curGame = 0;                // az eppen futo / kivalasztott jatek ID-ja

#include "dino.h"
#include "flappy.h"
#include "minesweeper.h"
#include "pacman.h"
#include "snake.h"
#include "invaders.h"
#include "tetris.h"
#include "breakout.h"
#include "pong.h"
#include "game2048.h"
#include "asteroids.h"
#include "roadhop.h"
#include "boxpush.h"

// ---- 8x8 ikonok (XBM, LSB = bal szel) ----
static const uint8_t ICON_DINO[8]    = { 0x78, 0xB8, 0x3C, 0x1D, 0x1F, 0x1E, 0x0A, 0x0A };
static const uint8_t ICON_FLAPPY[8]  = { 0x1C, 0x3E, 0x5F, 0xFF, 0x7B, 0x3E, 0x1C, 0x00 };
static const uint8_t ICON_PACMAN[8]  = { 0x3C, 0x7E, 0x1F, 0x0F, 0x0F, 0x1F, 0x7E, 0x3C };
static const uint8_t ICON_MINES[8]   = { 0x08, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08, 0x00 };
static const uint8_t ICON_SNAKE[8]   = { 0x3E, 0x02, 0x02, 0x3E, 0x20, 0x20, 0x3F, 0x00 };
static const uint8_t ICON_INVADER[8] = { 0x24, 0x18, 0x3C, 0x66, 0xFF, 0xBD, 0xA5, 0x18 };
static const uint8_t ICON_TETRIS[8]  = { 0x02, 0x07, 0x00, 0x30, 0x30, 0x00, 0x3F, 0x3F };
static const uint8_t ICON_BREAKOUT[8] = { 0x77, 0x77, 0x00, 0x00, 0x08, 0x00, 0x00, 0x3C };
static const uint8_t ICON_PONG[8]     = { 0x00, 0x81, 0x81, 0x89, 0x81, 0x81, 0x00, 0x00 };
static const uint8_t ICON_2048[8]     = { 0xFF, 0x81, 0xBD, 0xA1, 0xBD, 0x85, 0xBD, 0xFF };
static const uint8_t ICON_ASTEROIDS[8] = { 0x18, 0x18, 0x3C, 0x24, 0x66, 0x42, 0xC3, 0x00 };
static const uint8_t ICON_ROADHOP[8]  = { 0x42, 0x7E, 0x3C, 0x7E, 0x7E, 0x3C, 0x66, 0x66 };
static const uint8_t ICON_BOXPUSH[8]  = { 0xFF, 0x81, 0xBD, 0xBD, 0x81, 0xFF, 0x00, 0x18 };

struct GameDef {
  const char*    name;
  const char*    shortName;     // ha a teljes nev nem fer ki a statuszsavban
  const uint8_t* icon;
  Layout         layout;
  void (*init)();
  void (*update)();
  void (*draw)();
};

// ---- Stub: pont mozgatasa az aranaban, kozepso gomb = +10 pont (meg nem kesz jatekokhoz) ----
static int16_t stubX, stubY;

inline void stubInit() {
  g_hud.score = 0;
  g_hud.lives = 3;
  g_hud.showNext = false;
  stubX = uiArenaW() / 2;
  stubY = uiArenaH() / 2;
}

inline void stubUpdate() {
  if (btnDown(BTN_LEFT)  && stubX > 2)                stubX--;
  if (btnDown(BTN_RIGHT) && stubX < uiArenaW() - 3)   stubX++;
  if (btnDown(BTN_UP)    && stubY > 2)                stubY--;
  if (btnDown(BTN_DOWN)  && stubY < uiArenaH() - 3)   stubY++;
  if (btnPressed(BTN_CENTER)) {
    g_hud.score += 10;
    audioTone(1500, 30);
  }
}

inline void stubDraw() {
  gFrame(0, 0, uiArenaW(), uiArenaH());
  gBox(stubX - 1, stubY - 1, 3, 3);
  u8g2.setFont(u8g2_font_4x6_tr);
  gText(4, 10, "Coming soon");
}

// ABC sorrend. Tetris = szimmetrikus 64x64 arena, a tobbi a szeles 128x52.
static const GameDef GAMES[GAME_COUNT] = {
  { "Dino Run",       "Dino",     ICON_DINO,    LAYOUT_WIDE,   dinoInit, dinoUpdate, dinoDraw },
  { "Flappy Bird",    "Flappy",   ICON_FLAPPY,  LAYOUT_WIDE,   flappyInit, flappyUpdate, flappyDraw },
  { "Micro Pac-Man",  "Pac-Man",  ICON_PACMAN,  LAYOUT_WIDE,   pacmanInit, pacmanUpdate, pacmanDraw },
  { "Mine Sweeper",   "Mines",    ICON_MINES,   LAYOUT_WIDE,   msInit, msUpdate, msDraw },
  { "Retro Snake",    "Snake",    ICON_SNAKE,   LAYOUT_WIDE,   snakeInit, snakeUpdate, snakeDraw },
  { "Space Invaders", "Invaders", ICON_INVADER, LAYOUT_WIDE,   invadersInit, invadersUpdate, invadersDraw },
  { "Tetris",         "Tetris",   ICON_TETRIS,  LAYOUT_SQUARE, tetrisInit, tetrisUpdate, tetrisDraw },
  // Az uj jatekok a tomb VEGERE kerulnek (az ID a mentes kulcsa); a menu sorrendjet MENU_ORDER adja.
  { "Breakout",       "Breakout", ICON_BREAKOUT, LAYOUT_WIDE,  breakoutInit, breakoutUpdate, breakoutDraw },
  { "Pong",           "Pong",     ICON_PONG,    LAYOUT_WIDE,   pongInit, pongUpdate, pongDraw },
  { "2048",           "2048",     ICON_2048,    LAYOUT_WIDE,   g2048Init, g2048Update, g2048Draw },
  { "Asteroids",      "Asteroids", ICON_ASTEROIDS, LAYOUT_WIDE, asteroidsInit, asteroidsUpdate, asteroidsDraw },
  { "Road Hop",       "Road Hop", ICON_ROADHOP, LAYOUT_WIDE,   roadhopInit, roadhopUpdate, roadhopDraw },
  { "Box Push",       "Box Push", ICON_BOXPUSH, LAYOUT_WIDE,   boxpushInit, boxpushUpdate, boxpushDraw },
};

// Menu / statisztika sorrend (ABC): a tomb elemei JATEK-ID-k
static const uint8_t MENU_ORDER[GAME_COUNT] = { 9, 10, 12, 7, 0, 1, 2, 3, 8, 4, 11, 5, 6 };
// 2048, Asteroids, Box Push, Breakout, Dino Run, Flappy Bird, Micro Pac-Man, Mine Sweeper, Pong, Retro Snake, Road Hop, Space Invaders, Tetris
