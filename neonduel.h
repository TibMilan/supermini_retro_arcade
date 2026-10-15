#pragma once
// ============================================================
//  neonduel.h - Neon Duel (light-cycle duel against the CPU)
//  Layout: WIDE (128x52 arena), 64x26 grid of 2x2 px cells
//
//  Controls:  Arrows = turn (no reversing; two quick turns are queued)
//  Both bikes leave a wall behind them. Hit a wall / trail / the other bike and you crash.
//  Win a round = +score, crash = lose a heart (a draw replays the round).
//  The CPU gets smarter with every round you win.
//  Round score = (100 + ticks/2) * (difficulty+1)
//  Difficulty: bike speed, hearts and the CPU's starting skill.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <string.h>

constexpr uint8_t ND_W = 64;
constexpr uint8_t ND_H = 26;

struct NdDiff { uint8_t tickFrames; uint8_t lives; uint8_t skill; };
static const NdDiff ND_DIFF[DIFF_COUNT] = {
  { 4, 4, 2 },    // Easy
  { 3, 3, 4 },    // Medium
  { 2, 3, 7 },    // Hard
};

static const int8_t ND_DX[4] = { 0, 1, 0, -1 };     // up, right, down, left
static const int8_t ND_DY[4] = { -1, 0, 1, 0 };

static uint8_t  nd_g[ND_H][ND_W];                    // 0 empty, 1 player, 2 cpu, 3 wall
static uint16_t nd_vis[ND_H][ND_W];                  // flood fill stamps
static uint16_t nd_stamp;
static int8_t   nd_px, nd_py, nd_cx, nd_cy;
static uint8_t  nd_pd, nd_cd;
static uint8_t  nd_q[2], nd_qn;
static uint8_t  nd_state;            // 0 ready, 1 play, 2 round result, 3 game over
static uint8_t  nd_result;           // 0 draw, 1 win, 2 lose
static uint8_t  nd_lives;
static uint16_t nd_wins;
static uint16_t nd_ticks;
static uint8_t  nd_tickCnt;
static uint8_t  nd_wait;             // frames of freeze at round start
static uint32_t nd_stateAt;
static uint32_t nd_lastGain;
static bool     nd_newBest;
static uint32_t nd_bestAtStart;

static const Note ND_SFX_WIN[]  = { {1047, 70}, {1319, 70}, {1568, 70}, {2093, 160} };
static const Note ND_SFX_LOSE[] = { {400, 100}, {300, 100}, {200, 240} };
static const Note ND_SFX_DRAW[] = { {500, 80}, {500, 80} };
static const Note ND_SFX_GO[]   = { {900, 40} };

inline void ndSetTag() { g_hud.tag = "W:"; g_hud.tagVal = nd_wins; g_hud.lives = nd_lives; }

inline bool ndFree(int8_t x, int8_t y) {
  return x >= 0 && y >= 0 && x < ND_W && y < ND_H && nd_g[y][x] == 0;
}

inline uint8_t ndSkill() {
  uint16_t s = ND_DIFF[g_difficulty].skill + nd_wins / 2;
  return s > 12 ? 12 : s;
}

inline void ndNewRound() {
  memset(nd_g, 0, sizeof nd_g);
  for (uint8_t x = 0; x < ND_W; x++) { nd_g[0][x] = 3; nd_g[ND_H - 1][x] = 3; }
  for (uint8_t y = 0; y < ND_H; y++) { nd_g[y][0] = 3; nd_g[y][ND_W - 1] = 3; }
  nd_px = 8;           nd_py = ND_H / 2; nd_pd = 1;
  nd_cx = ND_W - 9;    nd_cy = ND_H / 2; nd_cd = 3;
  nd_g[nd_py][nd_px] = 1;
  nd_g[nd_cy][nd_cx] = 2;
  nd_qn = 0; nd_ticks = 0; nd_tickCnt = 0; nd_wait = 20;
  ndSetTag();
}

inline void ndReset() {
  nd_lives = ND_DIFF[g_difficulty].lives;
  nd_wins = 0;
  nd_state = 0; nd_stateAt = millis();
  nd_result = 0;
  g_hud.score = 0; g_hud.showNext = false;
  nd_newBest = false;
  nd_bestAtStart = g_highScore[g_curGame][g_difficulty];
  ndNewRound();
}

inline void neonduelInit() { ndReset(); }

// Free cells straight ahead (up to maxn)
inline uint8_t ndRun(int8_t x, int8_t y, uint8_t d, uint8_t maxn) {
  uint8_t n = 0;
  while (n < maxn) {
    x += ND_DX[d]; y += ND_DY[d];
    if (!ndFree(x, y)) break;
    n++;
  }
  return n;
}

// Limited flood fill: free area reachable from (sx,sy), at most `cap` cells
inline uint16_t ndArea(int8_t sx, int8_t sy, uint16_t cap, int8_t bx, int8_t by) {
  static uint16_t q[256];
  if (++nd_stamp == 0) { memset(nd_vis, 0, sizeof nd_vis); nd_stamp = 1; }
  uint16_t head = 0, tail = 0, count = 0;
  if (bx >= 0 && bx < ND_W && by >= 0 && by < ND_H) nd_vis[by][bx] = nd_stamp;   // opponent's next cell
  nd_vis[sy][sx] = nd_stamp;
  q[tail++] = (uint16_t)(sy * ND_W + sx);
  while (head != tail && count < cap) {
    uint16_t v = q[head++];
    head &= 255;
    int8_t x = v % ND_W, y = v / ND_W;
    count++;
    for (uint8_t d = 0; d < 4; d++) {
      int8_t nx = x + ND_DX[d], ny = y + ND_DY[d];
      if (!ndFree(nx, ny) || nd_vis[ny][nx] == nd_stamp) continue;
      nd_vis[ny][nx] = nd_stamp;
      uint16_t nt = (tail + 1) & 255;
      if (nt == head) continue;                       // queue full: ignore (cap is far below)
      q[tail] = (uint16_t)(ny * ND_W + nx);
      tail = nt;
    }
  }
  return count;
}

inline uint8_t ndAiChoose() {
  uint8_t skill = ndSkill();
  int8_t pnx = nd_px + ND_DX[nd_pd], pny = nd_py + ND_DY[nd_pd];
  uint8_t opts[3] = { nd_cd, (uint8_t)((nd_cd + 3) & 3), (uint8_t)((nd_cd + 1) & 3) };
  int16_t score[3];
  bool    ok[3];
  for (uint8_t i = 0; i < 3; i++) {
    int8_t nx = nd_cx + ND_DX[opts[i]], ny = nd_cy + ND_DY[opts[i]];
    ok[i] = ndFree(nx, ny) && !(skill >= 4 && nx == pnx && ny == pny);
    score[i] = -1;
    if (!ok[i]) continue;
    score[i] = ndRun(nx, ny, opts[i], skill);
    if (skill >= 8) {
      // temporarily occupy the cell so the flood does not walk back through it
      score[i] += ndArea(nx, ny, 60 + skill * 8, pnx, pny) / 2;
    }
  }
  // occasional mistake / wobble on low skill
  if (ok[0] && random(100) < (uint8_t)(skill < 12 ? 12 - skill : 0)) {
    bool l = ok[1], r = ok[2];
    if (l && r) return opts[random(2) ? 1 : 2];
    if (l) return opts[1];
    if (r) return opts[2];
  }
  int16_t best = -1; uint8_t bi = 0;
  for (uint8_t i = 0; i < 3; i++) if (ok[i] && score[i] > best) { best = score[i]; bi = i; }
  return opts[bi];
}

inline void ndFinish() {
  nd_state = 3; nd_stateAt = millis();
  nd_newBest = (g_hud.score > nd_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
}

inline void ndRoundEnd(bool pdead, bool cdead) {
  nd_state = 2; nd_stateAt = millis();
  if (pdead && cdead)  { nd_result = 0; audioPlay(ND_SFX_DRAW); }
  else if (cdead)      {
    nd_result = 1; nd_wins++;
    nd_lastGain = (uint32_t)(100 + nd_ticks / 2) * (g_difficulty + 1);
    g_hud.score += nd_lastGain;
    audioPlay(ND_SFX_WIN);
  } else {
    nd_result = 2;
    if (nd_lives) nd_lives--;
    audioPlay(ND_SFX_LOSE);
  }
  ndSetTag();
}

inline void ndTick() {
  if (nd_qn) { nd_pd = nd_q[0]; nd_q[0] = nd_q[1]; nd_qn--; }
  nd_cd = ndAiChoose();
  int8_t pnx = nd_px + ND_DX[nd_pd], pny = nd_py + ND_DY[nd_pd];
  int8_t cnx = nd_cx + ND_DX[nd_cd], cny = nd_cy + ND_DY[nd_cd];
  bool pdead = !ndFree(pnx, pny);
  bool cdead = !ndFree(cnx, cny);
  if (pnx == cnx && pny == cny) pdead = cdead = true;       // head-on
  if (!pdead) { nd_px = pnx; nd_py = pny; nd_g[pny][pnx] = 1; }
  if (!cdead) { nd_cx = cnx; nd_cy = cny; nd_g[cny][cnx] = 2; }
  nd_ticks++;
  if (pdead || cdead) ndRoundEnd(pdead, cdead);
}

inline void ndQueueTurn(uint8_t d) {
  uint8_t last = nd_qn ? nd_q[nd_qn - 1] : nd_pd;
  if (d == last || d == ((last + 2) & 3)) return;
  if (nd_qn >= 2) return;
  nd_q[nd_qn++] = d;
  audioTone(900, 4);
}

inline void neonduelUpdate() {
  uint32_t now = millis();
  if (nd_state == 3) {
    if (btnPressed(BTN_CENTER) && now - nd_stateAt > 600) ndReset();
    return;
  }
  if (nd_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN) ||
        btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) { nd_state = 1; audioPlay(ND_SFX_GO); }
    return;
  }
  if (nd_state == 2) {
    if (now - nd_stateAt > 1400) {
      if (nd_result == 2 && nd_lives == 0) { ndFinish(); return; }
      ndNewRound();
      nd_state = 1;
      audioPlay(ND_SFX_GO);
    }
    return;
  }
  // ---- play ----
  if (btnPressed(BTN_UP))    ndQueueTurn(0);
  if (btnPressed(BTN_RIGHT)) ndQueueTurn(1);
  if (btnPressed(BTN_DOWN))  ndQueueTurn(2);
  if (btnPressed(BTN_LEFT))  ndQueueTurn(3);
  if (nd_wait) { nd_wait--; return; }
  if (++nd_tickCnt >= ND_DIFF[g_difficulty].tickFrames) { nd_tickCnt = 0; ndTick(); }
}

inline void neonduelDraw() {
  g_hud.over = (nd_state == 3);
  uint32_t now = millis();
  gColor(1);
  gFrame(0, 0, 128, 52);
  for (uint8_t y = 1; y < ND_H - 1; y++) {
    for (uint8_t x = 1; x < ND_W - 1; x++) {
      uint8_t v = nd_g[y][x];
      if (!v) continue;
      if (v == 1)      gBox(x * 2, y * 2, 2, 2);
      else if (v == 2) { gPixel(x * 2, y * 2); gPixel(x * 2 + 1, y * 2 + 1); }
    }
  }
  bool blinkP = !(nd_state == 2 && nd_result != 1 && ((now / 120) & 1));
  bool blinkC = !(nd_state == 2 && nd_result != 2 && ((now / 120) & 1));
  if (blinkP) gBox(nd_px * 2 - 1, nd_py * 2 - 1, 4, 4);
  if (blinkC) {
    gColor(0); gBox(nd_cx * 2 - 1, nd_cy * 2 - 1, 4, 4);
    gColor(1); gFrame(nd_cx * 2 - 1, nd_cy * 2 - 1, 4, 4);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (nd_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press CENTER";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 19, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 19, w + 4, 9); gText((128 - w) / 2, 26, m);
  }
  if (nd_state == 2) {
    char b[16];
    const char* m = nd_result == 1 ? b : (nd_result == 2 ? "CRASH" : "DRAW");
    if (nd_result == 1) snprintf(b, sizeof b, "+%lu", (unsigned long)nd_lastGain);
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 3, 19, w + 6, 10);
    gColor(1); gFrame((128 - w) / 2 - 3, 19, w + 6, 10); gText((128 - w) / 2, 26, m);
  }
  if (nd_state == 3) drawGameOverPanel("GAME OVER", nd_newBest);
}
