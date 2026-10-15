#pragma once
// ============================================================
//  pong.h - Pong vs. CPU
//  Layout: WIDE (128x52 arena)
//
//  Controls:  Up / Down = move your paddle (left side), Center = serve
//  Match: first to 7 points. Every point is worth 100 x (difficulty+1),
//  winning the match adds a 500 x (difficulty+1) bonus.
//  Difficulty: ball speed, CPU paddle speed and accuracy, paddle size.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <math.h>

constexpr int16_t P_PL_X = 3;
constexpr int16_t P_CPU_X = 123;
constexpr uint8_t P_WIN_POINTS = 7;
constexpr float   P_PL_SPD = 2.4f;
constexpr float   P_SPD_MAX = 3.4f;

struct PongDiff { float ballSpd; float cpuSpd; float err; uint8_t padH; };
static const PongDiff P_DIFF[DIFF_COUNT] = {
  {1.5f, 1.0f, 7.0f, 16},    // Easy   (follows the ball, no prediction)
  {1.8f, 1.5f, 9.0f, 13},    // Medium (predicts wall bounces, sloppy aim)
  {2.1f, 2.0f, 7.5f, 10},    // Hard   (predicts, accurate, fast)
};

static float    p_plY, p_cpuY;              // paddle top
static float    p_x, p_y, p_vx, p_vy, p_spd;
static uint8_t  p_state;                    // 0 waiting for first serve, 1 play, 2 point pause, 3 over
static uint8_t  p_pl, p_cpu;
static int8_t   p_serveDir;                 // +1 toward CPU, -1 toward player
static uint32_t p_stateAt;
static float    p_cpuTarget, p_cpuErr;
static uint8_t  p_cpuTick;
static bool     p_newBest, p_won;
static uint32_t p_bestAtStart;

static const Note P_SFX_PAD[]   = { {600, 15} };
static const Note P_SFX_CPU[]   = { {450, 15} };
static const Note P_SFX_WALL[]  = { {300, 10} };
static const Note P_SFX_WIN_P[] = { {1047, 70}, {1568, 100} };
static const Note P_SFX_LOSE_P[] = { {300, 120}, {200, 180} };
static const Note P_SFX_WIN[]   = { {1047, 90}, {1319, 90}, {1568, 90}, {2093, 220} };
static const Note P_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };

inline uint32_t pScore() { return (uint32_t)p_pl * 100 * (g_difficulty + 1) + (p_won ? 500UL * (g_difficulty + 1) : 0); }
inline void pSetTag() { g_hud.tag = "C:"; g_hud.tagVal = p_cpu; }

inline void pPlaceBall() {
  const PongDiff& D = P_DIFF[g_difficulty];
  p_x = 63; p_y = 25; p_spd = D.ballSpd;
  p_vx = 0; p_vy = 0;
}

inline void pServe() {
  float ang = ((int)random(-35, 36)) / 100.0f;            // radians-ish, small
  p_vx = p_serveDir * p_spd * cosf(ang);
  p_vy = p_spd * sinf(ang);
  p_state = 1;
  p_cpuErr = ((int)random(-100, 101)) / 100.0f * P_DIFF[g_difficulty].err;
}

inline void pReset() {
  p_pl = p_cpu = 0; p_won = false;
  p_plY = p_cpuY = (52 - P_DIFF[g_difficulty].padH) / 2.0f;
  p_state = 0; p_stateAt = millis();
  p_serveDir = 1;
  p_cpuTick = 0; p_cpuTarget = 26;
  g_hud.score = 0; g_hud.showNext = false;
  p_newBest = false;
  p_bestAtStart = g_highScore[g_curGame][g_difficulty];
  pPlaceBall();
  pSetTag();
}

inline void pongInit() { pReset(); }

inline void pOver() {
  p_state = 3; p_stateAt = millis();
  p_won = (p_pl >= P_WIN_POINTS);
  g_hud.score = pScore();
  p_newBest = (g_hud.score > p_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  if (p_won) audioPlay(P_SFX_WIN); else audioPlay(P_SFX_OVER);
}

inline void pPoint(bool toPlayer) {
  if (toPlayer) { p_pl++; p_serveDir = 1;  audioPlay(P_SFX_WIN_P); }
  else          { p_cpu++; p_serveDir = -1; audioPlay(P_SFX_LOSE_P); }
  g_hud.score = pScore();
  pSetTag();
  if (p_pl >= P_WIN_POINTS || p_cpu >= P_WIN_POINTS) { pOver(); return; }
  p_state = 2; p_stateAt = millis();
  pPlaceBall();
}

inline void pPaddleBounce(float padY, uint8_t padH, int8_t dir) {
  float off = ((p_y + 1) - (padY + padH / 2.0f)) / (padH / 2.0f + 1);
  if (off > 1) off = 1;
  if (off < -1) off = -1;
  p_spd += 0.1f; if (p_spd > P_SPD_MAX) p_spd = P_SPD_MAX;
  p_vy = p_spd * off * 0.85f;
  p_vx = dir * sqrtf(p_spd * p_spd - p_vy * p_vy);
}

inline void pongUpdate() {
  const PongDiff& D = P_DIFF[g_difficulty];
  uint32_t now = millis();

  if (p_state == 3) {
    if (btnPressed(BTN_CENTER) && now - p_stateAt > 600) pReset();
    return;
  }

  // player paddle (always responsive)
  if (btnDown(BTN_UP))   p_plY -= P_PL_SPD;
  if (btnDown(BTN_DOWN)) p_plY += P_PL_SPD;
  if (p_plY < 0) p_plY = 0;
  if (p_plY > 52 - D.padH) p_plY = 52 - D.padH;

  if (p_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN)) pServe();
    return;
  }
  if (p_state == 2) {
    if (now - p_stateAt > 900) pServe();
    return;
  }

  // CPU paddle: re-aims every few frames; drifts to the middle while the ball goes away
  if (++p_cpuTick >= (g_difficulty == DIFF_EASY ? 6 : 3)) {
    p_cpuTick = 0;
    if (p_vx > 0) {
      float ty = p_y + 1;
      if (g_difficulty != DIFF_EASY) {                   // predict where the ball will arrive (with wall bounces)
        float frames = (P_CPU_X - p_x) / p_vx;
        float y = fmodf(ty - 1 + p_vy * frames, 98.0f);
        if (y < 0) y += 98.0f;
        if (y > 49.0f) y = 98.0f - y;
        ty = y + 1;
      }
      p_cpuTarget = ty + p_cpuErr;
    } else {
      p_cpuTarget = 26.0f;
    }
  }
  float center = p_cpuY + D.padH / 2.0f;
  float d = p_cpuTarget - center;
  if (fabsf(d) > 1.5f) p_cpuY += (d > 0 ? 1 : -1) * fminf(D.cpuSpd, fabsf(d));
  if (p_cpuY < 0) p_cpuY = 0;
  if (p_cpuY > 52 - D.padH) p_cpuY = 52 - D.padH;

  // ball (sub-steps so fast balls cannot tunnel through a paddle)
  int steps = (int)ceilf(p_spd * 1.5f);
  if (steps < 1) steps = 1;
  for (int i = 0; i < steps; i++) {
    p_x += p_vx / steps;
    p_y += p_vy / steps;
    if (p_y < 0)  { p_y = 0;  p_vy = fabsf(p_vy);  audioPlay(P_SFX_WALL); }
    if (p_y > 50) { p_y = 50; p_vy = -fabsf(p_vy); audioPlay(P_SFX_WALL); }

    // player paddle (x P_PL_X .. P_PL_X+2)
    if (p_vx < 0 && p_x <= P_PL_X + 2 && p_x + 2 >= P_PL_X &&
        p_y + 2 >= p_plY && p_y <= p_plY + D.padH) {
      p_x = P_PL_X + 2;
      pPaddleBounce(p_plY, D.padH, +1);
      p_cpuErr = ((int)random(-100, 101)) / 100.0f * D.err;
      audioPlay(P_SFX_PAD);
    }
    // CPU paddle
    if (p_vx > 0 && p_x + 2 >= P_CPU_X && p_x <= P_CPU_X + 2 &&
        p_y + 2 >= p_cpuY && p_y <= p_cpuY + D.padH) {
      p_x = P_CPU_X - 2;
      pPaddleBounce(p_cpuY, D.padH, -1);
      audioPlay(P_SFX_CPU);
    }
    if (p_x < -2)  { pPoint(false); return; }
    if (p_x > 128) { pPoint(true);  return; }
  }
}

inline void pongDraw() {
  g_hud.over = (p_state == 3);
  const PongDiff& D = P_DIFF[g_difficulty];
  gColor(1);
  for (int16_t y = 1; y < 52; y += 4) gVLine(63, y, 2);           // net

  u8g2.setFont(u8g2_font_6x10_tr);
  char b[4];
  snprintf(b, sizeof b, "%u", p_pl);  gText(52 - u8g2.getStrWidth(b), 12, b);
  snprintf(b, sizeof b, "%u", p_cpu); gText(75, 12, b);

  gBox(P_PL_X, (int16_t)p_plY, 2, D.padH);
  gBox(P_CPU_X, (int16_t)p_cpuY, 2, D.padH);
  if (p_state != 3) gBox((int16_t)p_x, (int16_t)p_y, 2, 2);

  u8g2.setFont(u8g2_font_4x6_tr);
  if (p_state == 0 && ((millis() / 450) & 1)) {
    const char* m = "Press CENTER";
    gColor(0); gBox((128 - u8g2.getStrWidth(m)) / 2 - 2, 31, u8g2.getStrWidth(m) + 4, 8);
    gColor(1); gText((128 - u8g2.getStrWidth(m)) / 2, 37, m);
  }
  if (p_state == 3) drawGameOverPanel(p_won ? "YOU WIN!" : "GAME OVER", p_newBest);
}
