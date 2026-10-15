#pragma once
// ============================================================
//  snake.h - Retro Snake (Kígyó)
//  Layout: WIDE (128x52 arena)
//
//  Irányítás:
//    Nyílgombok: Kanyarodás (2 elemű bemeneti sor: gyors kettős kanyar sem vész el)
//    Közép gomb: Indítás / Újrakezdés
//  Nehézség: Easy = a falon átbújik (nincs fal-halál), Medium/Hard = fal öl.
//  Minden megevett alma kicsit gyorsít a kígyón (a nehézség alsó határáig).
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

constexpr uint8_t  SN_COLS = 32;       // 128 / 4
constexpr uint8_t  SN_ROWS = 13;       // 52 / 4
constexpr uint16_t SN_MAX_LEN = 250;   // A kígyó maximális hossza

struct SnakeDiff { uint16_t delayMs; uint16_t minMs; bool wrap; };
static const SnakeDiff SN_DIFF[DIFF_COUNT] = {
  {180, 110, true },  // Easy: lassú, a fal nem öl (átbújik a túloldalra)
  {120,  70, false},  // Medium
  { 70,  45, false}   // Hard: gyors
};

static uint8_t  sn_x[SN_MAX_LEN], sn_y[SN_MAX_LEN];
static uint16_t sn_len;
static int8_t   sn_dx, sn_dy;          // Aktuális haladási irány
static int8_t   sn_qx[2], sn_qy[2];    // Bemeneti sor: a következő (max 2) kanyar
static uint8_t  sn_qn;
static uint8_t  sn_appleX, sn_appleY;
static uint16_t sn_apples;             // megevett almák (gyorsításhoz)
static uint8_t  sn_state;              // 0: Ready, 1: Play, 2: Dead
static uint32_t sn_lastUpdate, sn_deadAt;
static bool     sn_newBest;
static uint32_t sn_bestAtStart;

// Hangok
static const Note SN_SFX_EAT[]  = { {1200, 20}, {1600, 30} };
static const Note SN_SFX_DEAD[] = { {300, 100}, {200, 150}, {100, 250} };

// Alma lerakása szabad mezőre
inline void snSpawnApple() {
  bool valid = false;
  while (!valid) {
    sn_appleX = random(SN_COLS);
    sn_appleY = random(SN_ROWS);
    valid = true;
    for (uint16_t i = 0; i < sn_len; i++) {
      if (sn_x[i] == sn_appleX && sn_y[i] == sn_appleY) {
        valid = false;
        break;
      }
    }
  }
}

// Kanyar sorba állítása. Hivatkozás: az utolsó sorban álló irány (vagy a jelenlegi).
inline void snQueueTurn(int8_t dx, int8_t dy) {
  int8_t rx = sn_qn ? sn_qx[sn_qn - 1] : sn_dx;
  int8_t ry = sn_qn ? sn_qy[sn_qn - 1] : sn_dy;
  if (dx == rx && dy == ry) return;          // ugyanaz az irány
  if (dx == -rx && dy == -ry) return;        // 180 fokos fordulat tiltva
  if (sn_qn >= 2) return;
  sn_qx[sn_qn] = dx; sn_qy[sn_qn] = dy; sn_qn++;
  audioTone(800, 10);
}

inline void snakeReset() {
  sn_state = 0;
  sn_len = 3;

  // A kígyó középen indul, jobbra nézve
  sn_x[0] = SN_COLS / 2;     sn_y[0] = SN_ROWS / 2;
  sn_x[1] = sn_x[0] - 1;     sn_y[1] = sn_y[0];
  sn_x[2] = sn_x[0] - 2;     sn_y[2] = sn_y[0];

  sn_dx = 1; sn_dy = 0;
  sn_qn = 0;
  sn_apples = 0;

  g_hud.score = 0;
  g_hud.showNext = false;
  sn_newBest = false;
  sn_bestAtStart = g_highScore[g_curGame][g_difficulty];

  snSpawnApple();
}

inline void snakeInit() { snakeReset(); }

inline void snGameOver() {
  sn_state = 2;
  sn_deadAt = millis();
  sn_newBest = (g_hud.score > sn_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(SN_SFX_DEAD);
}

inline void snakeUpdate() {
  // GAME OVER Állapot
  if (sn_state == 2) {
    if (btnPressed(BTN_CENTER) && millis() - sn_deadAt > 600) snakeReset();
    return;
  }

  // READY Állapot (Várakozás indulásra)
  if (sn_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_RIGHT) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN)) {
      sn_state = 1;
      sn_lastUpdate = millis();
      if (btnPressed(BTN_UP))   snQueueTurn(0, -1);
      if (btnPressed(BTN_DOWN)) snQueueTurn(0, 1);
      // A balra nyíl le van tiltva induláskor, mert a test ott van
    }
    return;
  }

  // PLAY Állapot - Gombnyomások sorba állítása
  if (btnPressed(BTN_LEFT))  snQueueTurn(-1, 0);
  if (btnPressed(BTN_RIGHT)) snQueueTurn(1, 0);
  if (btnPressed(BTN_UP))    snQueueTurn(0, -1);
  if (btnPressed(BTN_DOWN))  snQueueTurn(0, 1);

  // Lépésköz: almánként 2 ms-t gyorsul, a nehézség alsó határáig
  const SnakeDiff& D = SN_DIFF[g_difficulty];
  int32_t stepMs = (int32_t)D.delayMs - (int32_t)sn_apples * 2;
  if (stepMs < D.minMs) stepMs = D.minMs;

  // PLAY Állapot - Léptetés
  if (millis() - sn_lastUpdate >= (uint32_t)stepMs) {
    sn_lastUpdate = millis();

    // Következő kanyar érvényesítése a sorból
    if (sn_qn) {
      sn_dx = sn_qx[0]; sn_dy = sn_qy[0];
      sn_qx[0] = sn_qx[1]; sn_qy[0] = sn_qy[1];
      sn_qn--;
    }

    int16_t nx = sn_x[0] + sn_dx;
    int16_t ny = sn_y[0] + sn_dy;

    // Fal: Easy-n átbújik, egyébként halál
    bool dead = false;
    if (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS) {
      if (D.wrap) {
        nx = (nx + SN_COLS) % SN_COLS;
        ny = (ny + SN_ROWS) % SN_ROWS;
      } else {
        dead = true;
      }
    }

    bool ate = (!dead && nx == sn_appleX && ny == sn_appleY);

    // Saját test: ha nem nő, a farok-cella ebben a lépésben felszabadul -> oda szabad lépni
    if (!dead) {
      uint16_t chk = ate ? sn_len : sn_len - 1;
      for (uint16_t i = 0; i < chk; i++) {
        if (sn_x[i] == nx && sn_y[i] == ny) { dead = true; break; }
      }
    }

    if (dead) {
      snGameOver();
    } else {
      if (ate) {
        if (sn_len < SN_MAX_LEN) sn_len++;
        sn_apples++;
        g_hud.score += (g_difficulty + 1) * 10;
        audioPlay(SN_SFX_EAT);
      }

      // Test mozgatása (Hátulról előre másoljuk az elemeket)
      for (uint16_t i = sn_len - 1; i > 0; i--) {
        sn_x[i] = sn_x[i - 1];
        sn_y[i] = sn_y[i - 1];
      }
      // Új fej pozíció
      sn_x[0] = nx;
      sn_y[0] = ny;

      // Az új alma csak a mozgatás UTÁN kerül le (így nem a fej alá)
      if (ate) snSpawnApple();
    }
  }
}

inline void snakeDraw() {
  g_hud.over = (sn_state == 2);
  // Alma rajzolása (Gyémánt / Kereszt forma a textúra miatt)
  int16_t ax = sn_appleX * 4;
  int16_t ay = sn_appleY * 4;
  gColor(1);
  gBox(ax + 1, ay, 2, 4);
  gBox(ax, ay + 1, 4, 2);

  // Kígyó rajzolása: 3x3-as blokkok + 1 pixeles összekötők -> összefüggő test
  for (uint16_t i = 0; i < sn_len; i++) {
    int16_t x = sn_x[i] * 4, y = sn_y[i] * 4;
    gBox(x, y, 3, 3);
    if (i > 0) {
      int16_t dx = (int16_t)sn_x[i] - (int16_t)sn_x[i - 1];
      int16_t dy = (int16_t)sn_y[i] - (int16_t)sn_y[i - 1];
      if (dy == 0 && (dx == 1 || dx == -1))      gBox((dx > 0 ? x - 1 : x + 3), y, 1, 3);
      else if (dx == 0 && (dy == 1 || dy == -1)) gBox(x, (dy > 0 ? y - 1 : y + 3), 3, 1);
    }
  }
  // Fej: a közepén "szem" (kiütött pixel), hogy látszódjon merre néz
  gColor(0);
  gPixel(sn_x[0] * 4 + 1, sn_y[0] * 4 + 1);
  gColor(1);

  // Feliratok
  if (sn_state == 0) {
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* msg = "Press ARROW to Start";
    gText((128 - u8g2.getStrWidth(msg)) / 2, 35, msg);
  } else if (sn_state == 2) {
    drawGameOverPanel("GAME OVER", sn_newBest);
  }
}
