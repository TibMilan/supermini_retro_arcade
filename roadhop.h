#pragma once
// ============================================================
//  roadhop.h - Road Hop (Frogger-style crossing)
//  Layout: WIDE (128x52 arena): 10 rows of 5 px
//          row 0 = 5 home slots, rows 1-3 = river (logs), row 4 = safe bank,
//          rows 5-8 = road, row 9 = start
//
//  Controls:  Arrows = hop one step (Up/Down: one row, Left/Right: 5 px)
//  Cars kill, water kills (stand on a log!), logs carry you sideways,
//  falling off the screen edge kills. Fill all 5 homes to finish the level.
//  A fresh random traffic pattern is generated for every level.
//  Each frog has a time limit (bar at the bottom).
//  Score: +10 for every new row reached, +50 + time bonus for a home,
//  +500 for a cleared level. Extra life every 5000 (max 5 hearts).
//  Difficulty: speed, traffic density, log length, time limit, lives.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <math.h>

constexpr int16_t  R_ROW   = 5;
constexpr float    R_PERIOD = 176.0f;       // lane pattern length (>= 128 + longest object 40)
constexpr float    R_SHIFT  = 40.0f;        // objects wrap off-screen
constexpr uint8_t  R_MAXOBJ = 5;
constexpr uint8_t  R_HOMES  = 5;
constexpr int16_t  R_HOME_X0 = 8, R_HOME_STEP = 24, R_HOME_W = 12;

struct RhDiff {
  float roadMin, roadMax, riverMin, riverMax;
  uint8_t cars, logMin, logMax;
  uint8_t timeS, lives;
};
static const RhDiff R_DIFF[DIFF_COUNT] = {
  {0.35f, 0.80f, 0.25f, 0.60f, 3, 22, 34, 40, 4},
  {0.50f, 1.10f, 0.35f, 0.80f, 4, 18, 30, 30, 3},
  {0.70f, 1.50f, 0.50f, 1.00f, 5, 14, 26, 24, 3},
};

struct RLane {
  int8_t  dir;
  float   spd, off;
  uint8_t n;
  float   x0[R_MAXOBJ];
  uint8_t len[R_MAXOBJ];
};

static RLane    r_lane[10];                 // index = row (only 1-3 and 5-8 used)
static float    r_fx;
static int8_t   r_row;
static int8_t   r_bestRow;
static bool     r_home[R_HOMES];
static uint8_t  r_homesDone;
static uint8_t  r_state;                    // 0 ready, 1 play, 2 dying, 3 level clear, 4 over
static uint32_t r_stateAt;
static int32_t  r_timeMs;
static uint8_t  r_lives, r_level;
static uint32_t r_nextLife;
static bool     r_newBest;
static uint32_t r_bestAtStart;

static const Note R_SFX_HOP[]   = { {700, 8} };
static const Note R_SFX_HOME[]  = { {1047, 60}, {1319, 60}, {1568, 100} };
static const Note R_SFX_DIE[]   = { {400, 70}, {300, 70}, {200, 70}, {120, 150} };
static const Note R_SFX_LEVEL[] = { {1047, 70}, {1319, 70}, {1568, 70}, {2093, 200} };
static const Note R_SFX_LIFE[]  = { {1319, 60}, {1760, 60}, {2349, 120} };
static const Note R_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };
static const Note R_SFX_SPLASH[]= { {250, 50}, {180, 120} };

inline float rRand() { return random(1000) / 1000.0f; }
inline bool rIsRiver(int8_t row) { return row >= 1 && row <= 3; }
inline bool rIsRoad(int8_t row)  { return row >= 5 && row <= 8; }

inline void rBuildLane(uint8_t row, bool river, int8_t dir) {
  const RhDiff& D = R_DIFF[g_difficulty];
  RLane& L = r_lane[row];
  float mult = 1.0f + 0.07f * (r_level - 1);
  if (mult > 1.6f) mult = 1.6f;
  float lo = river ? D.riverMin : D.roadMin, hi = river ? D.riverMax : D.roadMax;
  L.dir = dir;
  L.spd = (lo + rRand() * (hi - lo)) * mult;
  L.off = rRand() * R_PERIOD;

  uint8_t n = river ? 4 : D.cars;
  L.n = n;
  float sum = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (river) L.len[i] = D.logMin + random(D.logMax - D.logMin + 1);
    else       L.len[i] = (random(100) < 30) ? 14 : 8;               // truck / car
    sum += L.len[i];
  }
  if (river && sum > R_PERIOD - n * 8) {                             // keep every log gap >= 8 px
    for (uint8_t i = 0; i < n; i++) L.len[i] = L.len[i] > 4 ? L.len[i] - 4 : L.len[i];
    sum = 0; for (uint8_t i = 0; i < n; i++) sum += L.len[i];
  }
  float minGap = river ? 8.0f : 16.0f;
  float spare = R_PERIOD - sum - n * minGap;
  if (spare < 0) spare = 0;
  float w[R_MAXOBJ], wsum = 0;
  for (uint8_t i = 0; i < n; i++) { w[i] = 0.4f + rRand(); wsum += w[i]; }
  float pos = 0;
  for (uint8_t i = 0; i < n; i++) {
    L.x0[i] = pos;
    pos += L.len[i] + minGap + spare * w[i] / wsum;
  }
}

inline void rBuildLevel() {
  int8_t dir = random(2) ? 1 : -1;
  for (uint8_t row = 1; row <= 8; row++) {
    if (!rIsRiver(row) && !rIsRoad(row)) continue;
    rBuildLane(row, rIsRiver(row), dir);
    dir = -dir;
  }
  for (uint8_t i = 0; i < R_HOMES; i++) r_home[i] = false;
  r_homesDone = 0;
}

inline float rObjX(const RLane& L, uint8_t i) {          // left edge on screen (can be < 0)
  float x = fmodf(L.x0[i] + L.off, R_PERIOD);
  if (x < 0) x += R_PERIOD;
  return x - R_SHIFT;
}

inline void rSetHud() { g_hud.lives = r_lives; }

inline void rSpawnFrog() {
  r_fx = 62; r_row = 9; r_bestRow = 9;
  r_timeMs = (int32_t)R_DIFF[g_difficulty].timeS * 1000;
}

inline void rReset() {
  r_level = 1; r_lives = R_DIFF[g_difficulty].lives;
  r_nextLife = 5000;
  g_hud.score = 0; g_hud.showNext = false;
  r_newBest = false;
  r_bestAtStart = g_highScore[g_curGame][g_difficulty];
  r_state = 0; r_stateAt = millis();
  rBuildLevel();
  rSpawnFrog();
  rSetHud();
}

inline void roadhopInit() { rReset(); }

inline void rAddScore(uint32_t p) {
  g_hud.score += p;
  if (g_hud.score >= r_nextLife) {
    r_nextLife += 5000;
    if (r_lives < 5) { r_lives++; rSetHud(); audioPlay(R_SFX_LIFE); }
  }
}

inline void rDie(bool water) {
  r_state = 2; r_stateAt = millis();
  if (water) audioPlay(R_SFX_SPLASH); else audioPlay(R_SFX_DIE);
}

inline void rGameOver() {
  r_state = 4; r_stateAt = millis();
  r_newBest = (g_hud.score > r_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(R_SFX_OVER);
}

// is x-range [a,b) hit by an object in the given row?
inline bool rHit(uint8_t row, float a, float b) {
  const RLane& L = r_lane[row];
  for (uint8_t i = 0; i < L.n; i++) {
    float xo = rObjX(L, i);
    if (a < xo + L.len[i] && b > xo) return true;
  }
  return false;
}

// evaluates the frog's current position (after a hop or a lane step)
inline void rCheck() {
  if (r_state != 1) return;
  if (r_row == 0) {                                        // reached the home row
    float c = r_fx + 2;
    for (uint8_t i = 0; i < R_HOMES; i++) {
      float sx = R_HOME_X0 + i * R_HOME_STEP;
      if (c >= sx && c < sx + R_HOME_W) {
        if (r_home[i]) { rDie(false); return; }
        r_home[i] = true; r_homesDone++;
        rAddScore(50 + (uint32_t)(r_timeMs / 1000) * 2);
        audioPlay(R_SFX_HOME);
        if (r_homesDone >= R_HOMES) {
          rAddScore(500);
          r_state = 3; r_stateAt = millis();
          audioPlay(R_SFX_LEVEL);
        } else {
          rSpawnFrog();
        }
        return;
      }
    }
    rDie(false);                                           // hit the wall between homes
    return;
  }
  if (rIsRoad(r_row)) {
    if (rHit(r_row, r_fx + 0.5f, r_fx + 3.5f)) rDie(false);
  } else if (rIsRiver(r_row)) {
    if (!rHit(r_row, r_fx + 1.5f, r_fx + 2.5f)) rDie(true);   // centre of the frog must be on a log
  }
}

inline void roadhopUpdate() {
  uint32_t now = millis();

  if (r_state == 4) {
    if (btnPressed(BTN_CENTER) && now - r_stateAt > 600) rReset();
    // traffic keeps moving behind the panel
  }
  // lanes always move
  for (uint8_t row = 1; row <= 8; row++) {
    if (!rIsRiver(row) && !rIsRoad(row)) continue;
    r_lane[row].off += r_lane[row].dir * r_lane[row].spd;
    if (r_lane[row].off >= R_PERIOD) r_lane[row].off -= R_PERIOD;
    if (r_lane[row].off < 0) r_lane[row].off += R_PERIOD;
  }

  if (r_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN) ||
        btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) r_state = 1;
    return;
  }
  if (r_state == 2) {
    if (now - r_stateAt > 800) {
      if (r_lives) r_lives--;
      rSetHud();
      if (r_lives == 0) rGameOver();
      else { rSpawnFrog(); r_state = 1; }
    }
    return;
  }
  if (r_state == 3) {
    if (now - r_stateAt > 1400) {
      r_level++;
      rBuildLevel();
      rSpawnFrog();
      r_state = 1;
    }
    return;
  }
  if (r_state != 1) return;

  // timer
  r_timeMs -= FRAME_MS;
  if (r_timeMs <= 0) { r_timeMs = 0; rDie(false); return; }

  // carried by a log
  if (rIsRiver(r_row)) {
    r_fx += r_lane[r_row].dir * r_lane[r_row].spd;
    if (r_fx < -2 || r_fx > 126) { rDie(true); return; }
  }

  // hop
  int8_t nrow = r_row; float nfx = r_fx; bool hop = false;
  if (btnPressed(BTN_UP))         { nrow--; hop = true; }
  else if (btnPressed(BTN_DOWN))  { nrow++; hop = true; }
  else if (btnPressed(BTN_LEFT))  { nfx -= 5; hop = true; }
  else if (btnPressed(BTN_RIGHT)) { nfx += 5; hop = true; }
  if (hop) {
    if (nrow < 0 || nrow > 9) hop = false;
    if (nfx < 0) nfx = 0;
    if (nfx > 124) nfx = 124;
    if (hop) {
      r_row = nrow; r_fx = nfx;
      audioPlay(R_SFX_HOP);
      if (r_row < r_bestRow) { rAddScore(10 * (r_bestRow - r_row)); r_bestRow = r_row; }
    }
  }
  rCheck();
}

// ---------------- Drawing ----------------
inline void rDrawFrog(int16_t x, int16_t y) {
  gBox(x, y + 1, 4, 4);
  gColor(0); gPixel(x, y + 1); gPixel(x + 3, y + 1); gColor(1);        // eye sockets
}

inline void roadhopDraw() {
  g_hud.over = (r_state == 4);
  uint32_t now = millis();
  gColor(1);

  // home row: checker wall with 5 slots
  for (int16_t x = 0; x < 128; x++) {
    bool inSlot = false;
    for (uint8_t i = 0; i < R_HOMES; i++) {
      int16_t sx = R_HOME_X0 + i * R_HOME_STEP;
      if (x >= sx && x < sx + R_HOME_W) { inSlot = true; break; }
    }
    if (inSlot) continue;
    for (int16_t y = (x & 1); y < 5; y += 2) gPixel(x, y);
  }
  for (uint8_t i = 0; i < R_HOMES; i++) {
    int16_t sx = R_HOME_X0 + i * R_HOME_STEP;
    if (r_home[i]) rDrawFrog(sx + 4, 0);
  }

  // river waves
  for (uint8_t row = 1; row <= 3; row++) {
    int16_t y = row * R_ROW;
    for (int16_t x = (int16_t)((now / 220) % 8); x < 128; x += 8) gPixel(x, y + 2);
  }
  // bank / start dotted lines
  for (int16_t x = 0; x < 128; x += 4) { gPixel(x, 4 * R_ROW + 2); gPixel(x, 9 * R_ROW + 2); }

  // objects
  for (uint8_t row = 1; row <= 8; row++) {
    if (!rIsRiver(row) && !rIsRoad(row)) continue;
    const RLane& L = r_lane[row];
    int16_t y = row * R_ROW;
    for (uint8_t i = 0; i < L.n; i++) {
      int16_t xo = (int16_t)lroundf(rObjX(L, i));
      if (rIsRiver(row)) {
        gFrame(xo, y + 1, L.len[i], 4);
      } else {
        gBox(xo, y + 1, L.len[i], 4);
        gColor(0);
        gPixel(L.dir > 0 ? xo + L.len[i] - 2 : xo + 1, y + 2);      // windscreen (shows driving direction)
        gColor(1);
      }
    }
  }

  // frog
  if (r_state == 2) {
    if ((now / 120) & 1) {
      int16_t x = (int16_t)r_fx, y = r_row * R_ROW + 1;
      gPixel(x, y); gPixel(x + 3, y); gPixel(x + 1, y + 1); gPixel(x + 2, y + 1);
      gPixel(x + 1, y + 2); gPixel(x + 2, y + 2); gPixel(x, y + 3); gPixel(x + 3, y + 3);
    }
  } else if (r_state != 4 && r_state != 3) {
    rDrawFrog((int16_t)r_fx, r_row * R_ROW);
  }

  // time bar (bottom 2 px)
  if (r_state == 1 || r_state == 0) {
    int32_t total = (int32_t)R_DIFF[g_difficulty].timeS * 1000;
    int16_t w = (int16_t)((int64_t)128 * r_timeMs / total);
    if (r_timeMs * 4 < total && ((now / 200) & 1)) w = 0;                // last quarter blinks
    gBox(0, 50, w, 2);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (r_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press ARROW";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 20, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 20, w + 4, 9); gText((128 - w) / 2, 27, m);
  }
  if (r_state == 3) {
    const char* m = "LEVEL CLEAR!";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 3, 20, w + 6, 10);
    gColor(1); gFrame((128 - w) / 2 - 3, 20, w + 6, 10); gText((128 - w) / 2, 27, m);
  }
  if (r_state == 4) drawGameOverPanel("GAME OVER", r_newBest);
}
