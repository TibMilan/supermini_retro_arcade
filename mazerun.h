#pragma once
// ============================================================
//  mazerun.h - Maze Run (endless generated mazes, time attack)
//  Layout: WIDE (128x52 arena)
//
//  Controls:  Arrows = walk one tile (hold = auto-repeat)
//             Center (short) = HINT: shows the shortest path for ~1.5 s, costs 8 s
//  Goal: reach the blinking exit in the bottom-right corner.
//  Every maze is generated on the spot (recursive backtracker), so no two are alike.
//  Time attack: the clock runs, each solved maze adds 6 s + 80% of its par time
//  (bigger mazes give more time).
//  Score per maze = (150 + efficiency 0..100 + speed 0..100) * (difficulty+1)
//    efficiency = fewer wasted steps, speed = faster than a quick walk.
//  Difficulty: maze size, loops (several routes) and on Hard fog of war.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <string.h>

constexpr uint8_t  MZ_MAXW = 61;
constexpr uint8_t  MZ_MAXH = 25;
constexpr uint16_t MZ_BONUS_BASE_MS = 6000;   // + 0.8 x par time of the maze
constexpr uint16_t MZ_HINT_COST_MS = 8000;
constexpr uint8_t  MZ_HINT_FRAMES = 45;

struct MzDiff { uint8_t w, h, ts, loop, rep; uint16_t timeS; bool fog; };
static const MzDiff MZ_DIFF[DIFF_COUNT] = {
  { 31, 13, 4, 6, 3, 120, false },    // Easy   (15x6 cells)
  { 41, 17, 3, 3, 2, 100, false },    // Medium (20x8 cells)
  { 61, 25, 2, 0, 2, 100, true  },    // Hard   (30x12 cells, fog)
};

static uint8_t  mz_w, mz_h;
static uint8_t  mz_wall[MZ_MAXH][MZ_MAXW];
static uint16_t mz_dist[MZ_MAXH][MZ_MAXW];
static uint8_t  mz_seen[MZ_MAXH][MZ_MAXW];
static uint8_t  mz_px, mz_py;
static uint16_t mz_steps, mz_shortest;
static uint32_t mz_parMs, mz_bonusMs;
static uint32_t mz_usedMs;
static int32_t  mz_timeMs;
static uint8_t  mz_state;                // 0 ready, 1 play, 2 solved flash, 3 time up
static uint32_t mz_stateAt;
static uint32_t mz_lastGain;
static uint16_t mz_solved;
static bool     mz_newBest;
static uint32_t mz_bestAtStart;
static int8_t   mz_dir;
static int8_t   mz_rep;
static uint8_t  mz_hint;                 // frames of hint left

static const int8_t MZ_DX[4] = { 0, 0, -1, 1 };          // up, down, left, right
static const int8_t MZ_DY[4] = { -1, 1, 0, 0 };

static const Note MZ_SFX_STEP[]  = { {700, 5} };
static const Note MZ_SFX_HINT[]  = { {1200, 30}, {1500, 30} };
static const Note MZ_SFX_SOLVE[] = { {1047, 70}, {1319, 70}, {1568, 70}, {2093, 160} };
static const Note MZ_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };
static const Note MZ_SFX_NEW[]   = { {900, 20} };

inline void mzSetTag() {
  g_hud.tag = "T:";
  g_hud.tagVal = (int16_t)((mz_timeMs + 999) / 1000);
}

inline void mzSee() {
  if (!MZ_DIFF[g_difficulty].fog) return;
  for (int8_t dy = -10; dy <= 10; dy++) {
    for (int8_t dx = -10; dx <= 10; dx++) {
      if (dx * dx + dy * dy > 100) continue;
      int16_t x = mz_px + dx, y = mz_py + dy;
      if (x < 0 || y < 0 || x >= mz_w || y >= mz_h) continue;
      mz_seen[y][x] = 1;
    }
  }
}

inline void mzGenerate() {
  const MzDiff& d = MZ_DIFF[g_difficulty];
  mz_w = d.w; mz_h = d.h;
  memset(mz_wall, 1, sizeof mz_wall);
  memset(mz_seen, 0, sizeof mz_seen);
  const uint8_t cw = (mz_w - 1) / 2, ch = (mz_h - 1) / 2;
  static uint16_t stack[(MZ_MAXW / 2) * (MZ_MAXH / 2) + 4];
  uint16_t sp = 0;
  mz_wall[1][1] = 0;
  stack[sp++] = 0;
  while (sp) {
    uint16_t cur = stack[sp - 1];
    uint8_t cx = cur % cw, cy = cur / cw;
    uint8_t cand[4], n = 0;
    for (uint8_t k = 0; k < 4; k++) {
      int8_t nx = cx + MZ_DX[k], ny = cy + MZ_DY[k];
      if (nx < 0 || ny < 0 || nx >= cw || ny >= ch) continue;
      if (mz_wall[2 * ny + 1][2 * nx + 1]) cand[n++] = k;
    }
    if (!n) { sp--; continue; }
    uint8_t k = cand[random(n)];
    int8_t nx = cx + MZ_DX[k], ny = cy + MZ_DY[k];
    mz_wall[2 * cy + 1 + MZ_DY[k]][2 * cx + 1 + MZ_DX[k]] = 0;   // wall between
    mz_wall[2 * ny + 1][2 * nx + 1] = 0;
    stack[sp++] = (uint16_t)(ny * cw + nx);
  }
  // loops: open some walls between two floors
  if (d.loop) {
    for (uint8_t y = 1; y < mz_h - 1; y++)
      for (uint8_t x = 1; x < mz_w - 1; x++) {
        if (!mz_wall[y][x] || ((x & 1) == (y & 1))) continue;
        bool between = (x & 1) ? (!mz_wall[y - 1][x] && !mz_wall[y + 1][x])
                               : (!mz_wall[y][x - 1] && !mz_wall[y][x + 1]);
        if (between && random(100) < d.loop) mz_wall[y][x] = 0;
      }
  }
  // distance map from the exit (BFS)
  static uint16_t q[MZ_MAXW * MZ_MAXH];
  for (uint8_t y = 0; y < mz_h; y++) for (uint8_t x = 0; x < mz_w; x++) mz_dist[y][x] = 0xFFFF;
  uint16_t head = 0, tail = 0;
  mz_dist[mz_h - 2][mz_w - 2] = 0;
  q[tail++] = (uint16_t)((mz_h - 2) * mz_w + (mz_w - 2));
  while (head < tail) {
    uint16_t v = q[head++];
    uint8_t x = v % mz_w, y = v / mz_w;
    for (uint8_t k = 0; k < 4; k++) {
      int8_t nx = x + MZ_DX[k], ny = y + MZ_DY[k];
      if (nx < 0 || ny < 0 || nx >= mz_w || ny >= mz_h) continue;
      if (mz_wall[ny][nx] || mz_dist[ny][nx] != 0xFFFF) continue;
      mz_dist[ny][nx] = mz_dist[y][x] + 1;
      q[tail++] = (uint16_t)(ny * mz_w + nx);
    }
  }
  mz_px = 1; mz_py = 1;
  mz_shortest = mz_dist[1][1];
  mz_parMs = (uint32_t)mz_shortest * d.rep * FRAME_MS * 3 / 2;
  mz_bonusMs = MZ_BONUS_BASE_MS + mz_parMs * 8 / 10;
  mz_steps = 0; mz_usedMs = 0;
  mz_dir = -1; mz_rep = 0; mz_hint = 0;
  mzSee();
}

inline void mzReset() {
  mz_solved = 0;
  mz_state = 0; mz_stateAt = millis();
  mz_timeMs = (int32_t)MZ_DIFF[g_difficulty].timeS * 1000;
  g_hud.score = 0; g_hud.showNext = false;
  mz_newBest = false;
  mz_bestAtStart = g_highScore[g_curGame][g_difficulty];
  mzGenerate();
  mzSetTag();
}

inline void mazerunInit() { mzReset(); }

inline void mzSolved() {
  const MzDiff& d = MZ_DIFF[g_difficulty];
  uint32_t extra = mz_steps > mz_shortest ? mz_steps - mz_shortest : 0;
  int32_t eff = 100 - (int32_t)(100UL * extra / (mz_shortest ? mz_shortest : 1));
  if (eff < 0) eff = 0;
  uint32_t parMs = mz_parMs;
  int32_t spd = 100 - (int32_t)(100ULL * mz_usedMs / (2ULL * (parMs ? parMs : 1)));
  if (spd < 0) spd = 0;
  mz_lastGain = (uint32_t)(150 + eff + spd) * (g_difficulty + 1);
  g_hud.score += mz_lastGain;
  mz_timeMs += mz_bonusMs;
  mz_solved++;
  mz_state = 2; mz_stateAt = millis();
  audioPlay(MZ_SFX_SOLVE);
}

inline void mzTimeUp() {
  mz_state = 3; mz_stateAt = millis();
  mz_timeMs = 0;
  mz_newBest = (g_hud.score > mz_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(MZ_SFX_OVER);
}

inline void mzStep(uint8_t k) {
  int8_t nx = mz_px + MZ_DX[k], ny = mz_py + MZ_DY[k];
  if (nx < 0 || ny < 0 || nx >= mz_w || ny >= mz_h || mz_wall[ny][nx]) return;
  mz_px = nx; mz_py = ny; mz_steps++;
  mzSee();
  audioPlay(MZ_SFX_STEP);
  if (mz_px == mz_w - 2 && mz_py == mz_h - 2) mzSolved();
}

inline bool mzDirDown(int8_t k) {
  switch (k) {
    case 0: return btnDown(BTN_UP);
    case 1: return btnDown(BTN_DOWN);
    case 2: return btnDown(BTN_LEFT);
    default: return btnDown(BTN_RIGHT);
  }
}

inline void mazerunUpdate() {
  const MzDiff& d = MZ_DIFF[g_difficulty];
  uint32_t now = millis();
  if (mz_state == 3) {
    if (btnPressed(BTN_CENTER) && now - mz_stateAt > 600) mzReset();
    return;
  }
  if (mz_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN) ||
        btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) { mz_state = 1; audioPlay(MZ_SFX_NEW); }
    return;
  }
  if (mz_state == 2) {
    if (now - mz_stateAt > 900) { mzGenerate(); mz_state = 1; audioPlay(MZ_SFX_NEW); }
    mzSetTag();
    return;
  }

  // ---- play ----
  mz_timeMs -= FRAME_MS;
  mz_usedMs += FRAME_MS;
  if (mz_hint) mz_hint--;
  if (mz_timeMs <= 0) { mzTimeUp(); mzSetTag(); return; }

  if (btnPressed(BTN_CENTER) && mz_hint == 0) {
    mz_hint = MZ_HINT_FRAMES;
    mz_timeMs -= MZ_HINT_COST_MS;
    audioPlay(MZ_SFX_HINT);
    if (mz_timeMs <= 0) { mzTimeUp(); mzSetTag(); return; }
  }

  int8_t nd = -1;
  if (btnPressed(BTN_UP)) nd = 0;
  else if (btnPressed(BTN_DOWN)) nd = 1;
  else if (btnPressed(BTN_LEFT)) nd = 2;
  else if (btnPressed(BTN_RIGHT)) nd = 3;
  if (nd >= 0) {
    mz_dir = nd; mz_rep = 5;
    mzStep(nd);
  } else {
    if (mz_dir >= 0 && !mzDirDown(mz_dir)) {
      mz_dir = -1;
      for (int8_t k = 0; k < 4; k++) if (mzDirDown(k)) { mz_dir = k; mz_rep = d.rep; break; }
    }
    if (mz_dir >= 0 && --mz_rep <= 0) { mz_rep = d.rep; mzStep(mz_dir); }
  }
  mzSetTag();
}

inline void mazerunDraw() {
  g_hud.over = (mz_state == 3);
  const MzDiff& d = MZ_DIFF[g_difficulty];
  uint32_t now = millis();
  const int16_t ts = d.ts;
  const int16_t ox = (128 - mz_w * ts) / 2, oy = (52 - mz_h * ts) / 2;
  const bool fog = d.fog;

  gColor(1);
  for (uint8_t y = 0; y < mz_h; y++) {
    uint8_t x = 0;
    while (x < mz_w) {
      bool w = mz_wall[y][x] && (!fog || mz_seen[y][x]);
      if (!w) { x++; continue; }
      uint8_t x0 = x;
      while (x < mz_w && mz_wall[y][x] && (!fog || mz_seen[y][x])) x++;
      gBox(ox + x0 * ts, oy + y * ts, (x - x0) * ts, ts);
    }
  }

  // hint path
  if (mz_hint && mz_state == 1) {
    uint8_t x = mz_px, y = mz_py;
    uint16_t guard = 0;
    while (mz_dist[y][x] != 0 && mz_dist[y][x] != 0xFFFF && guard++ < 400) {
      for (uint8_t k = 0; k < 4; k++) {
        int8_t nx = x + MZ_DX[k], ny = y + MZ_DY[k];
        if (nx < 0 || ny < 0 || nx >= mz_w || ny >= mz_h) continue;
        if (!mz_wall[ny][nx] && mz_dist[ny][nx] + 1 == mz_dist[y][x]) { x = nx; y = ny; break; }
      }
      int16_t cx = ox + x * ts + ts / 2, cy = oy + y * ts + ts / 2;
      gPixel(cx, cy);
    }
  }

  // exit (blinking) and player
  if (((now / 250) % 3) != 0) gBox(ox + (mz_w - 2) * ts, oy + (mz_h - 2) * ts, ts, ts);
  if (mz_state == 2 || ((now / 130) & 3) != 0) {
    int16_t ps = ts >= 3 ? ts - 1 : ts;
    int16_t off = (ts - ps) / 2;
    gColor(2);
    gBox(ox + mz_px * ts + off, oy + mz_py * ts + off, ps, ps);
    gColor(1);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (mz_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press CENTER";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 21, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 21, w + 4, 9); gText((128 - w) / 2, 28, m);
  }
  if (mz_state == 2) {
    char b[16]; snprintf(b, sizeof b, "+%lu", (unsigned long)mz_lastGain);
    int16_t w = u8g2.getStrWidth(b);
    gColor(0); gBox((128 - w) / 2 - 3, 20, w + 6, 10);
    gColor(1); gFrame((128 - w) / 2 - 3, 20, w + 6, 10); gText((128 - w) / 2, 27, b);
  }
  if (mz_state == 3) drawGameOverPanel("TIME UP", mz_newBest);
}
