#pragma once
// ============================================================
//  game2048.h - 2048
//  Layout: WIDE (128x52 arena), 4x4 board with 24x12 px tiles
//
//  Controls:  Arrows = slide all tiles, Center = undo (limited per game)
//  Difficulty: Easy 3 undos, 10% chance of a 4 spawn; Medium 1 undo, 15%;
//              Hard no undo, 25%.
//  Reaching 2048 shows a prompt (Center: keep playing / Left: menu).
//  Score = sum of the merged tile values.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

struct G2Diff { uint8_t undos; uint8_t p4; };
static const G2Diff Q_DIFF[DIFF_COUNT] = { {3, 10}, {1, 15}, {0, 25} };

static uint8_t  q_g[4][4];            // exponent: 0 empty, n = 2^n
static uint8_t  q_prev[4][4];
static uint32_t q_prevScore;
static bool     q_canUndo;
static uint8_t  q_undo;
static uint8_t  q_state;              // 0 play, 1 won prompt, 2 over
static bool     q_won;                // 2048 already acknowledged
static uint8_t  q_newR, q_newC;       // last spawned tile (flashes)
static uint32_t q_newAt, q_stateAt;
static bool     q_newBest;
static uint32_t q_bestAtStart;

static const Note Q_SFX_MOVE[]  = { {500, 10} };
static const Note Q_SFX_WIN[]   = { {1047, 80}, {1319, 80}, {1568, 80}, {2093, 200} };
static const Note Q_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };
static const Note Q_SFX_UNDO[]  = { {700, 25}, {500, 25} };

inline void qSetTag() { g_hud.tag = "U:"; g_hud.tagVal = q_undo; }

inline void qSpawn() {
  uint8_t er[16], ec[16], n = 0;
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t c = 0; c < 4; c++)
      if (!q_g[r][c]) { er[n] = r; ec[n] = c; n++; }
  if (!n) return;
  uint8_t k = random(n);
  q_g[er[k]][ec[k]] = (random(100) < Q_DIFF[g_difficulty].p4) ? 2 : 1;
  q_newR = er[k]; q_newC = ec[k]; q_newAt = millis();
}

inline void qReset() {
  for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++) q_g[r][c] = 0;
  q_state = 0; q_won = false; q_canUndo = false;
  q_undo = Q_DIFF[g_difficulty].undos;
  q_newAt = 0;
  g_hud.score = 0; g_hud.showNext = false;
  q_newBest = false;
  q_bestAtStart = g_highScore[g_curGame][g_difficulty];
  qSpawn(); qSpawn();
  qSetTag();
}

inline void g2048Init() { qReset(); }

// Slides/merges the board toward dir (0 left, 1 right, 2 up, 3 down). Returns true if anything moved.
inline bool qMove(uint8_t dir) {
  bool moved = false;
  uint32_t gained = 0;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t line[4], out[4] = {0, 0, 0, 0};
    bool merged[4] = {false, false, false, false};
    uint8_t rr[4], cc[4];
    for (uint8_t k = 0; k < 4; k++) {
      if (dir == 0)      { rr[k] = i;     cc[k] = k; }
      else if (dir == 1) { rr[k] = i;     cc[k] = 3 - k; }
      else if (dir == 2) { rr[k] = k;     cc[k] = i; }
      else               { rr[k] = 3 - k; cc[k] = i; }
      line[k] = q_g[rr[k]][cc[k]];
    }
    uint8_t o = 0;
    for (uint8_t k = 0; k < 4; k++) {
      if (!line[k]) continue;
      if (o && out[o - 1] == line[k] && !merged[o - 1]) {
        out[o - 1]++;                           // merge (each tile merges at most once per move)
        merged[o - 1] = true;
        gained += 1UL << out[o - 1];
      } else {
        out[o++] = line[k];
      }
    }
    for (uint8_t k = 0; k < 4; k++) {
      if (q_g[rr[k]][cc[k]] != out[k]) moved = true;
      q_g[rr[k]][cc[k]] = out[k];
    }
  }
  g_hud.score += gained;
  if (gained) {
    uint8_t mx = 0;
    for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++) if (q_g[r][c] > mx) mx = q_g[r][c];
    audioTone(500 + mx * 90, 30);
  } else if (moved) {
    audioPlay(Q_SFX_MOVE);
  }
  return moved;
}

inline bool qStuck() {
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t c = 0; c < 4; c++) {
      if (!q_g[r][c]) return false;
      if (c < 3 && q_g[r][c] == q_g[r][c + 1]) return false;
      if (r < 3 && q_g[r][c] == q_g[r + 1][c]) return false;
    }
  return true;
}

inline void qFinish() {
  q_state = 2; q_stateAt = millis();
  q_newBest = (g_hud.score > q_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(Q_SFX_OVER);
}

inline void g2048Update() {
  if (q_state == 2) {
    if (btnPressed(BTN_CENTER) && millis() - q_stateAt > 600) qReset();
    return;
  }
  if (q_state == 1) {                                   // 2048 reached
    if (btnPressed(BTN_CENTER) && millis() - q_stateAt > 400) { q_state = 0; q_won = true; }
    return;
  }

  uint8_t dir = 255;
  if (btnPressed(BTN_LEFT))       dir = 0;
  else if (btnPressed(BTN_RIGHT)) dir = 1;
  else if (btnPressed(BTN_UP))    dir = 2;
  else if (btnPressed(BTN_DOWN))  dir = 3;

  if (dir != 255) {
    uint8_t save[4][4]; uint32_t saveScore = g_hud.score;
    for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++) save[r][c] = q_g[r][c];
    if (qMove(dir)) {
      for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++) q_prev[r][c] = save[r][c];
      q_prevScore = saveScore; q_canUndo = true;
      qSpawn();
      if (!q_won) {
        for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++)
          if (q_g[r][c] >= 11) {
            q_state = 1; q_stateAt = millis();
            storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
            audioPlay(Q_SFX_WIN);
          }
      }
      if (q_state == 0 && qStuck()) qFinish();
    }
  } else if (btnPressed(BTN_CENTER) && q_canUndo && q_undo > 0) {
    for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 4; c++) q_g[r][c] = q_prev[r][c];
    g_hud.score = q_prevScore;
    q_undo--; q_canUndo = false; q_newAt = 0;
    qSetTag();
    audioPlay(Q_SFX_UNDO);
  }
}

inline void g2048Draw() {
  g_hud.over = (q_state != 0);
  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t r = 0; r < 4; r++) {
    for (uint8_t c = 0; c < 4; c++) {
      int16_t x = 13 + c * 26, y = r * 13;
      uint8_t e = q_g[r][c];
      gColor(1);
      if (!e) { gPixel(x + 11, y + 5); gPixel(x + 12, y + 6); continue; }
      char b[24]; snprintf(b, sizeof b, "%lu", 1UL << e);
      int16_t tx = x + (24 - u8g2.getStrWidth(b)) / 2, ty = y + 9;
      bool fresh = q_newAt && millis() - q_newAt < 250 && r == q_newR && c == q_newC;
      if (e >= 6 || fresh) {                             // 64+ (and a just-spawned tile): inverted
        gBox(x, y, 24, 12);
        gColor(0); gText(tx, ty, b); gColor(1);
      } else {
        gFrame(x, y, 24, 12);
        gText(tx, ty, b);
      }
    }
  }
  if (q_state == 1) {
    gColor(0); gBox(22, 8, 84, 36);
    gColor(1); gFrame(22, 8, 84, 36);
    u8g2.setFont(u8g2_font_6x10_tr);
    const char* t = "2048!";
    gText((128 - u8g2.getStrWidth(t)) / 2, 20, t);
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* a = "Center: go on";
    gText((128 - u8g2.getStrWidth(a)) / 2, 30, a);
    const char* b = "Left: menu";
    gText((128 - u8g2.getStrWidth(b)) / 2, 39, b);
  } else if (q_state == 2) {
    drawGameOverPanel("GAME OVER", q_newBest);
  }
}
