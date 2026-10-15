#pragma once
// ============================================================
//  asteroids.h - Asteroids
//  Layout: WIDE (128x52 arena), everything wraps around the edges
//
//  Controls:  Left / Right = rotate      Up = thrust
//             Center = fire (hold = auto-fire)
//             Down = hyperspace jump (1 in 8 chance of blowing up)
//  Rocks split big -> medium -> small. Clear all rocks for the next wave
//  (one more rock each wave). Extra life every 5000 points (max 5 hearts).
//  Difficulty: rock speed, rocks per wave, lives (Easy 4, Medium/Hard 3).
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <math.h>

constexpr uint8_t A_MAXROCK = 32;
constexpr uint8_t A_MAXBUL  = 4;
constexpr float   A_TURN    = 0.19f;
constexpr float   A_THRUST  = 0.13f;
constexpr float   A_VMAX    = 2.8f;
constexpr float   A_BUL_SPD = 3.6f;
constexpr uint8_t A_BUL_LIFE = 34;
constexpr float   A_PI2     = 6.2831853f;

struct ADiff { float vMin, vMax; uint8_t startRocks; uint8_t maxRocks; uint8_t lives; };
static const ADiff A_DIFF[DIFF_COUNT] = {
  {0.12f, 0.32f, 3, 6, 4},     // Easy   (px/frame, slow drifting rocks)
  {0.20f, 0.55f, 4, 7, 3},     // Medium
  {0.30f, 0.80f, 4, 8, 3},     // Hard
};
static const float A_RADIUS[4] = { 0, 2.0f, 3.5f, 5.5f };
static const uint16_t A_POINTS[4] = { 0, 100, 50, 20 };

struct ARock { float x, y, vx, vy, a, da; uint8_t size; uint8_t shape[8]; };
struct ABul  { float x, y, vx, vy; uint8_t life; };

static ARock    a_rock[A_MAXROCK];
static ABul     a_bul[A_MAXBUL];
static float    a_x, a_y, a_vx, a_vy, a_ang;
static bool     a_thrust;
static uint8_t  a_state;            // 0 ready, 1 play, 2 over
static bool     a_alive;
static uint32_t a_respawnAt, a_invulnUntil, a_stateAt, a_waveAt, a_boomAt;
static float    a_boomX, a_boomY;
static uint8_t  a_lives, a_wave;
static uint32_t a_nextLife;
static uint8_t  a_fireCd;
static bool     a_newBest;
static uint32_t a_bestAtStart;

static const Note A_SFX_FIRE[]  = { {1400, 8}, {900, 8} };
static const Note A_SFX_BIG[]   = { {200, 40}, {150, 60} };
static const Note A_SFX_MED[]   = { {300, 30}, {220, 40} };
static const Note A_SFX_SMALL[] = { {500, 25}, {380, 30} };
static const Note A_SFX_DIE[]   = { {400, 80}, {300, 80}, {200, 80}, {120, 200} };
static const Note A_SFX_WAVE[]  = { {880, 60}, {1175, 60}, {1568, 120} };
static const Note A_SFX_JUMP[]  = { {1200, 15}, {1800, 25} };
static const Note A_SFX_LIFE[]  = { {1319, 60}, {1760, 60}, {2349, 120} };
static const Note A_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };

inline float aRand() { return random(1000) / 1000.0f; }

// ---------------- Rocks ----------------
inline void aSpawnRock(float x, float y, float vx, float vy, uint8_t size) {
  for (uint8_t i = 0; i < A_MAXROCK; i++) {
    if (a_rock[i].size) continue;
    ARock& r = a_rock[i];
    r.x = x; r.y = y; r.vx = vx; r.vy = vy; r.size = size;
    r.a = aRand() * A_PI2;
    r.da = (aRand() - 0.5f) * 0.06f;
    for (uint8_t k = 0; k < 8; k++) r.shape[k] = random(256);
    return;
  }
}

inline void aSpawnWave() {
  const ADiff& D = A_DIFF[g_difficulty];
  uint8_t n = D.startRocks + (a_wave - 1);
  if (n > D.maxRocks) n = D.maxRocks;
  for (uint8_t i = 0; i < n; i++) {
    float x, y;
    for (uint8_t t = 0; t < 20; t++) {
      x = aRand() * 128; y = aRand() * 52;
      float dx = x - a_x, dy = y - a_y;
      if (dx * dx + dy * dy > 22 * 22) break;           // keep clear of the ship
    }
    float sp = D.vMin + aRand() * (D.vMax - D.vMin);
    float dir = aRand() * A_PI2;
    aSpawnRock(x, y, cosf(dir) * sp, sinf(dir) * sp, 3);
  }
}

inline bool aRocksLeft() {
  for (uint8_t i = 0; i < A_MAXROCK; i++) if (a_rock[i].size) return true;
  return false;
}

inline void aSetHud() { g_hud.lives = a_lives; }

inline void aPlaceShip() {
  a_x = 64; a_y = 26; a_vx = a_vy = 0; a_ang = 0;
  a_alive = true;
  a_invulnUntil = millis() + 2000;
  a_thrust = false;
}

inline void aReset() {
  for (uint8_t i = 0; i < A_MAXROCK; i++) a_rock[i].size = 0;
  for (uint8_t i = 0; i < A_MAXBUL; i++) a_bul[i].life = 0;
  a_state = 0; a_stateAt = millis();
  a_lives = A_DIFF[g_difficulty].lives;
  a_wave = 1; a_nextLife = 5000; a_fireCd = 0; a_boomAt = 0; a_waveAt = 0;
  g_hud.score = 0; g_hud.showNext = false;
  a_newBest = false;
  a_bestAtStart = g_highScore[g_curGame][g_difficulty];
  aPlaceShip();
  a_invulnUntil = 0;
  aSpawnWave();
  aSetHud();
}

inline void asteroidsInit() { aReset(); }

inline void aGameOver() {
  a_state = 2; a_stateAt = millis();
  a_newBest = (g_hud.score > a_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(A_SFX_OVER);
}

inline void aKillShip() {
  a_alive = false;
  a_boomAt = millis(); a_boomX = a_x; a_boomY = a_y;
  if (a_lives) a_lives--;
  aSetHud();
  audioPlay(A_SFX_DIE);
  if (a_lives == 0) { a_respawnAt = 0; aGameOver(); }
  else a_respawnAt = millis() + 1400;
}

inline void aAddScore(uint16_t p) {
  g_hud.score += p;
  if (g_hud.score >= a_nextLife) {
    a_nextLife += 5000;
    if (a_lives < 5) { a_lives++; aSetHud(); audioPlay(A_SFX_LIFE); }
  }
}

inline void aHitRock(uint8_t i) {
  ARock& r = a_rock[i];
  uint8_t sz = r.size;
  aAddScore(A_POINTS[sz]);
  float x = r.x, y = r.y, vx = r.vx, vy = r.vy;
  r.size = 0;
  if (sz == 3) audioPlay(A_SFX_BIG); else if (sz == 2) audioPlay(A_SFX_MED); else audioPlay(A_SFX_SMALL);
  if (sz > 1) {
    const ADiff& D = A_DIFF[g_difficulty];
    for (uint8_t k = 0; k < 2; k++) {
      float sp = D.vMin + aRand() * (D.vMax - D.vMin);
      sp *= (sz == 3) ? 1.2f : 1.35f;
      float dir = aRand() * A_PI2;
      aSpawnRock(x, y, vx * 0.4f + cosf(dir) * sp, vy * 0.4f + sinf(dir) * sp, sz - 1);
    }
  }
}

inline void aFire() {
  for (uint8_t i = 0; i < A_MAXBUL; i++) {
    if (a_bul[i].life) continue;
    float sx = sinf(a_ang), sy = -cosf(a_ang);
    a_bul[i].x = a_x + sx * 5; a_bul[i].y = a_y + sy * 5;
    a_bul[i].vx = a_vx * 0.5f + sx * A_BUL_SPD;
    a_bul[i].vy = a_vy * 0.5f + sy * A_BUL_SPD;
    a_bul[i].life = A_BUL_LIFE;
    audioPlay(A_SFX_FIRE);
    return;
  }
}

inline float aWrapX(float v) { while (v < 0) v += 128; while (v >= 128) v -= 128; return v; }
inline float aWrapY(float v) { while (v < 0) v += 52;  while (v >= 52)  v -= 52;  return v; }

// shortest wrapped difference
inline float aDX(float a, float b) { float d = a - b; if (d > 64) d -= 128; if (d < -64) d += 128; return d; }
inline float aDY(float a, float b) { float d = a - b; if (d > 26) d -= 52;  if (d < -26) d += 52;  return d; }

inline void asteroidsUpdate() {
  uint32_t now = millis();

  if (a_state == 2) {
    if (btnPressed(BTN_CENTER) && now - a_stateAt > 600) aReset();
    // rocks keep drifting behind the panel
  }
  if (a_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) {
      a_state = 1;
      a_invulnUntil = now + 1500;
    }
  }

  // rocks
  for (uint8_t i = 0; i < A_MAXROCK; i++) {
    ARock& r = a_rock[i];
    if (!r.size) continue;
    r.x = aWrapX(r.x + r.vx); r.y = aWrapY(r.y + r.vy); r.a += r.da;
  }
  if (a_state != 1) return;

  // wave handling
  if (!aRocksLeft()) {
    if (!a_waveAt) { a_waveAt = now + 1500; audioPlay(A_SFX_WAVE); }
    else if ((int32_t)(now - a_waveAt) >= 0) { a_waveAt = 0; a_wave++; aSpawnWave(); }
  }

  // ship
  if (a_alive) {
    if (btnDown(BTN_LEFT))  a_ang -= A_TURN;
    if (btnDown(BTN_RIGHT)) a_ang += A_TURN;
    a_thrust = btnDown(BTN_UP);
    if (a_thrust) {
      a_vx += sinf(a_ang) * A_THRUST; a_vy += -cosf(a_ang) * A_THRUST;
      float sp = sqrtf(a_vx * a_vx + a_vy * a_vy);
      if (sp > A_VMAX) { a_vx *= A_VMAX / sp; a_vy *= A_VMAX / sp; }
    }
    a_vx *= 0.992f; a_vy *= 0.992f;
    a_x = aWrapX(a_x + a_vx); a_y = aWrapY(a_y + a_vy);

    if (a_fireCd) a_fireCd--;
    if (btnPressed(BTN_CENTER) || (btnDown(BTN_CENTER) && !a_fireCd)) {
      if (!a_fireCd || btnPressed(BTN_CENTER)) { aFire(); a_fireCd = 9; }
    }
    if (btnPressed(BTN_DOWN)) {                       // hyperspace
      a_x = aRand() * 128; a_y = aRand() * 52; a_vx = a_vy = 0;
      audioPlay(A_SFX_JUMP);
      if (random(8) == 0) { aKillShip(); return; }
    }
  } else if (a_respawnAt && (int32_t)(now - a_respawnAt) >= 0) {
    bool clear = true;                                // wait until the middle is free
    for (uint8_t i = 0; i < A_MAXROCK; i++) {
      if (!a_rock[i].size) continue;
      float dx = aDX(a_rock[i].x, 64), dy = aDY(a_rock[i].y, 26);
      float rr = A_RADIUS[a_rock[i].size] + 10;
      if (dx * dx + dy * dy < rr * rr) { clear = false; break; }
    }
    if (clear || now - a_respawnAt > 3000) { aPlaceShip(); a_respawnAt = 0; }
  }

  // bullets
  for (uint8_t b = 0; b < A_MAXBUL; b++) {
    ABul& u = a_bul[b];
    if (!u.life) continue;
    u.life--;
    u.x = aWrapX(u.x + u.vx); u.y = aWrapY(u.y + u.vy);
    for (uint8_t i = 0; i < A_MAXROCK; i++) {
      ARock& r = a_rock[i];
      if (!r.size) continue;
      float dx = aDX(u.x, r.x), dy = aDY(u.y, r.y);
      float rr = A_RADIUS[r.size] + 1;
      if (dx * dx + dy * dy < rr * rr) { u.life = 0; aHitRock(i); break; }
    }
  }

  // ship vs rocks
  if (a_alive && (int32_t)(now - a_invulnUntil) >= 0) {
    for (uint8_t i = 0; i < A_MAXROCK; i++) {
      ARock& r = a_rock[i];
      if (!r.size) continue;
      float dx = aDX(a_x, r.x), dy = aDY(a_y, r.y);
      float rr = A_RADIUS[r.size] + 1.8f;
      if (dx * dx + dy * dy < rr * rr) { aKillShip(); break; }
    }
  }
}

// ---------------- Drawing ----------------
inline void gLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {      // clipped Bresenham (arena coords)
  int16_t dx = abs(x1 - x0), dy = -abs(y1 - y0);
  int16_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (uint8_t guard = 0; guard < 64; guard++) {
    gPixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

inline void aDrawRock(const ARock& r, float ox, float oy) {
  float base = A_RADIUS[r.size];
  int16_t px[8], py[8];
  for (uint8_t k = 0; k < 8; k++) {
    float ang = r.a + k * (A_PI2 / 8);
    float rad = base * (0.72f + r.shape[k] / 255.0f * 0.5f);
    px[k] = (int16_t)lroundf(r.x + ox + cosf(ang) * rad);
    py[k] = (int16_t)lroundf(r.y + oy + sinf(ang) * rad);
  }
  for (uint8_t k = 0; k < 8; k++) gLine(px[k], py[k], px[(k + 1) & 7], py[(k + 1) & 7]);
}

inline void aDrawShip(float x, float y, bool flame) {
  float s = sinf(a_ang), c = cosf(a_ang);
  // local coords: nose (0,-5), left (-3,3.5), right (3,3.5)
  int16_t nx = (int16_t)lroundf(x + s * 5),                 ny = (int16_t)lroundf(y - c * 5);
  int16_t lx = (int16_t)lroundf(x + (-3 * c) - (3.5f * s)), ly = (int16_t)lroundf(y + (-3 * s) + (3.5f * c) * 1.0f);
  int16_t rx = (int16_t)lroundf(x + (3 * c) - (3.5f * s)),  ry = (int16_t)lroundf(y + (3 * s) + (3.5f * c) * 1.0f);
  gLine(nx, ny, lx, ly); gLine(nx, ny, rx, ry); gLine(lx, ly, rx, ry);
  if (flame) {
    int16_t fx = (int16_t)lroundf(x - s * 7), fy = (int16_t)lroundf(y + c * 7);
    int16_t mx = (int16_t)lroundf((lx + rx) / 2.0f), my = (int16_t)lroundf((ly + ry) / 2.0f);
    gLine(mx, my, fx, fy);
  }
}

inline void asteroidsDraw() {
  g_hud.over = (a_state == 2);
  uint32_t now = millis();
  gColor(1);

  for (uint8_t i = 0; i < A_MAXROCK; i++) {
    const ARock& r = a_rock[i];
    if (!r.size) continue;
    float rad = A_RADIUS[r.size] + 1;
    for (int8_t ix = -1; ix <= 1; ix++)
      for (int8_t iy = -1; iy <= 1; iy++) {
        float ox = ix * 128.0f, oy = iy * 52.0f;
        if (r.x + ox + rad < 0 || r.x + ox - rad >= 128 || r.y + oy + rad < 0 || r.y + oy - rad >= 52) continue;
        aDrawRock(r, ox, oy);
      }
  }
  for (uint8_t b = 0; b < A_MAXBUL; b++)
    if (a_bul[b].life) gBox((int16_t)a_bul[b].x, (int16_t)a_bul[b].y, 1, 1);

  if (a_alive) {
    bool inv = (int32_t)(a_invulnUntil - now) > 0;
    if (!inv || ((now / 90) & 1)) {
      bool flame = a_thrust && ((now / 60) & 1);
      for (int8_t ix = -1; ix <= 1; ix++)
        for (int8_t iy = -1; iy <= 1; iy++) {
          float ox = ix * 128.0f, oy = iy * 52.0f;
          if (a_x + ox < -8 || a_x + ox > 136 || a_y + oy < -8 || a_y + oy > 60) continue;
          aDrawShip(a_x + ox, a_y + oy, flame);
        }
    }
  } else if (a_boomAt && now - a_boomAt < 900) {      // expanding debris ring
    float t = (now - a_boomAt) / 900.0f;
    float rad = 2 + t * 12;
    for (uint8_t k = 0; k < 10; k++) {
      float ang = k * (A_PI2 / 10) + t * 2;
      gPixel((int16_t)lroundf(a_boomX + cosf(ang) * rad), (int16_t)lroundf(a_boomY + sinf(ang) * rad));
    }
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (a_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press CENTER";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 33, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 33, w + 4, 9); gText((128 - w) / 2, 40, m);
  }
  if (a_state == 2) drawGameOverPanel("GAME OVER", a_newBest);
}
