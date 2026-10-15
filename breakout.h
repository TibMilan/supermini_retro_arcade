#pragma once
// ============================================================
//  breakout.h - Breakout
//  Layout: WIDE (128x52 arena)
//
//  Controls:  Left / Right = move the paddle (hold)
//             Center / Up  = launch the ball
//  Difficulty: ball speed, paddle width, lives (Easy 4, Medium/Hard 3).
//  Clear all bricks -> next level (new brick pattern, faster ball).
//  Score: higher rows are worth more; difficulty multiplies the points.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <math.h>

constexpr uint8_t B_COLS     = 16;
constexpr uint8_t B_ROWS_MAX = 6;
constexpr int16_t B_BW       = 8;      // brick pitch (7 px brick + 1 px gap)
constexpr int16_t B_BH       = 4;      // 3 px brick + 1 px gap
constexpr int16_t B_TOP      = 3;
constexpr int16_t B_PAD_Y    = 48;     // paddle top (2 px high)
constexpr float   B_PAD_SPD  = 2.6f;

struct BoDiff { float speed; uint8_t padW; uint8_t lives; };
static const BoDiff B_DIFF[DIFF_COUNT] = { {1.3f, 26, 4}, {1.6f, 20, 3}, {1.9f, 15, 3} };

static uint16_t b_bricks[B_ROWS_MAX];       // bit c = brick present
static uint8_t  b_rows;
static uint16_t b_left;
static float    b_pad;                      // paddle left edge
static float    b_x, b_y, b_vx, b_vy, b_spd;
static bool     b_stuck;
static uint8_t  b_state;                    // 0 play, 1 over
static uint8_t  b_lives, b_level;
static uint32_t b_stateAt;
static bool     b_newBest;
static uint32_t b_bestAtStart;

static const Note B_SFX_BRICK[] = { {900, 12} };
static const Note B_SFX_PAD[]   = { {500, 15} };
static const Note B_SFX_WALL[]  = { {350, 10} };
static const Note B_SFX_LOSE[]  = { {400, 90}, {300, 90}, {200, 160} };
static const Note B_SFX_LEVEL[] = { {1047, 70}, {1319, 70}, {1568, 70}, {2093, 160} };
static const Note B_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };

inline float boBaseSpeed() {
  float s = B_DIFF[g_difficulty].speed + 0.12f * (b_level - 1);
  return s > B_DIFF[g_difficulty].speed + 0.7f ? B_DIFF[g_difficulty].speed + 0.7f : s;
}

inline void boBuildLevel() {
  b_rows = 4 + (b_level >= 2) + (b_level >= 4);
  uint8_t pat = (b_level - 1) & 3;
  b_left = 0;
  for (uint8_t r = 0; r < B_ROWS_MAX; r++) {
    b_bricks[r] = 0;
    if (r >= b_rows) continue;
    for (uint8_t c = 0; c < B_COLS; c++) {
      bool on = true;
      if (pat == 1)      on = (((c + r) & 1) == 0);
      else if (pat == 2) on = ((c & 3) != 3);
      else if (pat == 3) on = (c >= r && c < B_COLS - r);
      if (on) { b_bricks[r] |= (1u << c); b_left++; }
    }
  }
}

inline void boSetTag() { g_hud.lives = b_lives; }

inline void boServe() {
  b_stuck = true;
  b_spd = boBaseSpeed();
  b_vx = 0; b_vy = 0;
}

inline void boReset() {
  b_level = 1;
  b_lives = B_DIFF[g_difficulty].lives;
  b_state = 0;
  b_pad = (128 - B_DIFF[g_difficulty].padW) / 2.0f;
  g_hud.score = 0; g_hud.showNext = false;
  b_newBest = false;
  b_bestAtStart = g_highScore[g_curGame][g_difficulty];
  boBuildLevel();
  boServe();
  boSetTag();
}

inline void breakoutInit() { boReset(); }

// Removes every brick touched by the 2x2 ball box at (x,y); true if any
inline bool boCollide(float x, float y) {
  bool hit = false;
  for (uint8_t k = 0; k < 4; k++) {
    float px = x + (k & 1) * 1.99f, py = y + (k >> 1) * 1.99f;
    if (py < B_TOP || px < 0) continue;
    int cy = (int)((py - B_TOP) / B_BH), cx = (int)(px / B_BW);
    if (cy >= b_rows || cx >= B_COLS) continue;
    if (b_bricks[cy] & (1u << cx)) {
      b_bricks[cy] &= ~(1u << cx);
      b_left--;
      g_hud.score += (uint32_t)(b_rows - cy) * 10 * (g_difficulty + 1);
      hit = true;
    }
  }
  return hit;
}

inline void boLaunch() {
  float off = ((int)random(-40, 41)) / 100.0f;       // slight random angle
  b_vx = b_spd * off;
  b_vy = -sqrtf(b_spd * b_spd - b_vx * b_vx);
  b_stuck = false;
  audioPlay(B_SFX_PAD);
}

inline void boGameOver() {
  b_state = 1; b_stateAt = millis();
  b_newBest = (g_hud.score > b_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(B_SFX_OVER);
}

inline void breakoutUpdate() {
  if (b_state == 1) {
    if (btnPressed(BTN_CENTER) && millis() - b_stateAt > 600) boReset();
    return;
  }
  const BoDiff& D = B_DIFF[g_difficulty];

  // paddle
  if (btnDown(BTN_LEFT))  b_pad -= B_PAD_SPD;
  if (btnDown(BTN_RIGHT)) b_pad += B_PAD_SPD;
  if (b_pad < 0) b_pad = 0;
  if (b_pad > 128 - D.padW) b_pad = 128 - D.padW;

  if (b_stuck) {
    b_x = b_pad + D.padW / 2.0f - 1;
    b_y = B_PAD_Y - 2;
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP)) boLaunch();
    return;
  }

  int steps = (int)ceilf(fmaxf(fabsf(b_vx), fabsf(b_vy)) * 2.0f);   // <= 0.5 px per sub-step
  if (steps < 1) steps = 1;
  for (int i = 0; i < steps; i++) {
    float nx = b_x + b_vx / steps;
    if (nx < 0)        { nx = 0;   b_vx = fabsf(b_vx);  audioPlay(B_SFX_WALL); }
    else if (nx > 126) { nx = 126; b_vx = -fabsf(b_vx); audioPlay(B_SFX_WALL); }
    else if (boCollide(nx, b_y)) { b_vx = -b_vx; nx = b_x; audioPlay(B_SFX_BRICK); }
    b_x = nx;

    float ny = b_y + b_vy / steps;
    if (ny < 0) { ny = 0; b_vy = fabsf(b_vy); audioPlay(B_SFX_WALL); }
    else if (boCollide(b_x, ny)) { b_vy = -b_vy; ny = b_y; audioPlay(B_SFX_BRICK); }
    b_y = ny;

    // paddle
    if (b_vy > 0 && b_y + 2 >= B_PAD_Y && b_y + 2 <= B_PAD_Y + 3 &&
        b_x + 2 >= b_pad && b_x <= b_pad + D.padW) {
      float off = ((b_x + 1) - (b_pad + D.padW / 2.0f)) / (D.padW / 2.0f + 1);
      if (off > 1) off = 1;
      if (off < -1) off = -1;
      b_spd = boBaseSpeed() + 0.002f * (b_rows * B_COLS - b_left) * 0.5f;   // slowly speeds up while clearing
      b_vx = b_spd * off * 0.85f;
      b_vy = -sqrtf(b_spd * b_spd - b_vx * b_vx);
      b_y = B_PAD_Y - 2;
      audioPlay(B_SFX_PAD);
    }

    if (b_left == 0) {
      g_hud.score += 50 * b_lives * (g_difficulty + 1);
      b_level++;
      boBuildLevel();
      boServe();
      audioPlay(B_SFX_LEVEL);
      return;
    }
    if (b_y > 52) {                                      // lost the ball
      if (b_lives) b_lives--;
      boSetTag();
      if (b_lives == 0) { boGameOver(); return; }
      audioPlay(B_SFX_LOSE);
      boServe();
      return;
    }
  }
}

inline void breakoutDraw() {
  g_hud.over = (b_state == 1);
  gColor(1);
  for (uint8_t r = 0; r < b_rows; r++)
    for (uint8_t c = 0; c < B_COLS; c++)
      if (b_bricks[r] & (1u << c)) gBox(c * B_BW, B_TOP + r * B_BH, 7, 3);

  const BoDiff& D = B_DIFF[g_difficulty];
  gBox((int16_t)b_pad, B_PAD_Y, D.padW, 2);
  if (b_state == 0) gBox((int16_t)b_x, (int16_t)b_y, 2, 2);

  u8g2.setFont(u8g2_font_4x6_tr);
  if (b_state == 0 && b_stuck && ((millis() / 450) & 1)) {
    const char* m = "Press CENTER";
    gColor(0); gBox((128 - u8g2.getStrWidth(m)) / 2 - 2, 31, u8g2.getStrWidth(m) + 4, 8);
    gColor(1); gText((128 - u8g2.getStrWidth(m)) / 2, 37, m);
  }
  if (b_state == 1) drawGameOverPanel("GAME OVER", b_newBest);
}
