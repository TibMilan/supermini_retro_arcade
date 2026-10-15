#pragma once
// ============================================================
//  moonlander.h - Moon Lander (gravity landing game)
//  Layout: WIDE (128x52 arena)
//
//  Controls:  Left / Right = rotate,  Up = main engine (burns fuel)
//  Land gently on a flat pad: slow enough, nearly upright, centre over the pad.
//  Two pads: the wide one scores x1, the narrow one scores more.
//  Crash = lose a heart and retry the same terrain. A landing builds a new terrain:
//  the pads shrink and the ground gets rougher every level.
//  Score = 100 * pad multiplier * (difficulty+1) + fuel left + softness bonus.
//  Leaving the screen at the sides or far above = crash.
//  Difficulty: gravity, speed limits, pad size, fuel.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <math.h>
#include <string.h>

struct MlDiff { float g, thrust; uint8_t pad; float vyMax, vxMax, angMax; uint8_t fuel; uint8_t rough; uint8_t lives; };
static const MlDiff ML_DIFF[DIFF_COUNT] = {
  { 0.010f, 0.032f, 22, 0.80f, 0.50f, 0.35f, 150, 3, 4 },   // Easy
  { 0.012f, 0.034f, 14, 0.60f, 0.40f, 0.28f, 120, 5, 3 },   // Medium
  { 0.014f, 0.036f, 10, 0.50f, 0.30f, 0.22f, 100, 7, 3 },   // Hard
};

constexpr float   ML_ROT  = 0.048f;
constexpr float   ML_BURN = 0.25f;
constexpr uint8_t ML_PAD_MIN = 7;

static int8_t   ml_h[128];                  // terrain height (arena y) per column
static uint8_t  ml_padX[2], ml_padW[2], ml_padM[2];
static float    ml_x, ml_y, ml_vx, ml_vy, ml_a, ml_fuel;
static float    ml_fuelStart;
static uint8_t  ml_state;                   // 0 ready, 1 play, 2 landed, 3 crashed, 4 game over
static uint8_t  ml_lives;
static uint16_t ml_level;
static uint32_t ml_stateAt;
static uint32_t ml_lastGain;
static bool     ml_thrusting;
static bool     ml_newBest;
static uint32_t ml_bestAtStart;
static float    ml_dx[8], ml_dy[8], ml_dvx[8], ml_dvy[8];   // debris

static const Note ML_SFX_LAND[]  = { {1047, 80}, {1319, 80}, {1568, 80}, {2093, 200} };
static const Note ML_SFX_CRASH[] = { {300, 80}, {200, 100}, {120, 300} };
static const Note ML_SFX_GO[]    = { {900, 40} };

inline void mlSetTag() { g_hud.tag = "L:"; g_hud.tagVal = ml_level; g_hud.lives = ml_lives; }

inline uint8_t ml_padWidth() {
  int16_t w = ML_DIFF[g_difficulty].pad - (int16_t)(ml_level - 1);
  return w < ML_PAD_MIN ? ML_PAD_MIN : (uint8_t)w;
}

inline void mlGenTerrain() {
  const MlDiff& d = ML_DIFF[g_difficulty];
  uint8_t rough = d.rough + (ml_level - 1);
  if (rough > d.rough + 5) rough = d.rough + 5;
  int8_t pts[17];
  int16_t h = 36 + random(-4, 5);
  for (uint8_t i = 0; i < 17; i++) {
    h += random(-(int16_t)rough, (int16_t)rough + 1);
    if (h < 26) h = 26;
    if (h > 47) h = 47;
    pts[i] = (int8_t)h;
  }
  for (uint8_t x = 0; x < 128; x++) {
    uint8_t s = x >> 3; uint8_t t = x & 7;
    if (s > 15) { s = 15; t = 8; }
    ml_h[x] = (int8_t)((pts[s] * (8 - t) + pts[s + 1] * t + 4) / 8);
  }
  uint8_t wA = ml_padWidth();
  uint8_t wB = (uint8_t)(wA * 2 / 3); if (wB < 6) wB = 6;
  bool aLeft = random(2);
  uint8_t xA = aLeft ? random(4, 60 - wA + 1) : random(68, 124 - wA + 1);
  uint8_t xB = aLeft ? random(68, 124 - wB + 1) : random(4, 60 - wB + 1);
  ml_padX[0] = xA; ml_padW[0] = wA; ml_padM[0] = 1;
  ml_padX[1] = xB; ml_padW[1] = wB; ml_padM[1] = (g_difficulty == DIFF_EASY) ? 2 : 3;
  for (uint8_t p = 0; p < 2; p++) {
    int16_t lv = ml_h[ml_padX[p] + ml_padW[p] / 2];
    if (lv < 30) lv = 30;
    if (lv > 46) lv = 46;
    for (uint8_t x = ml_padX[p]; x < ml_padX[p] + ml_padW[p]; x++) ml_h[x] = (int8_t)lv;
  }
}

inline void mlResetLander() {
  const MlDiff& d = ML_DIFF[g_difficulty];
  ml_x = 64 + random(-30, 31);
  ml_y = 7;
  float v = (g_difficulty == DIFF_EASY) ? 0.15f : 0.25f;
  ml_vx = (random(2) ? v : -v) * (random(50, 101) / 100.0f);
  ml_vy = 0;
  ml_a = 0;
  ml_thrusting = false;
  (void)d;
}

inline void mlReset() {
  const MlDiff& d = ML_DIFF[g_difficulty];
  ml_lives = d.lives;
  ml_level = 1;
  ml_fuel = d.fuel; ml_fuelStart = ml_fuel;
  ml_state = 0; ml_stateAt = millis();
  g_hud.score = 0; g_hud.showNext = false;
  ml_newBest = false;
  ml_bestAtStart = g_highScore[g_curGame][g_difficulty];
  mlGenTerrain();
  mlResetLander();
  mlSetTag();
}

inline void moonlanderInit() { mlReset(); }

inline int16_t mlTerrain(int16_t x) {
  if (x < 0) x = 0;
  if (x > 127) x = 127;
  return ml_h[x];
}

// local lander points: [0..4] contact points (nose, body corners, feet), legs drawn separately
static const int8_t ML_PT[7][2] = {
  { 0, -5 }, { 3, -2 }, { 3, 1 }, { -3, 1 }, { -3, -2 }, { -5, 4 }, { 5, 4 }
};
// contact test uses: nose, lower body corners, feet
static const uint8_t ML_CONTACT[5] = { 0, 2, 3, 5, 6 };

inline void mlWorld(uint8_t i, float& wx, float& wy) {
  float c = cosf(ml_a), s = sinf(ml_a);
  float lx = ML_PT[i][0], ly = ML_PT[i][1];
  wx = ml_x + lx * c - ly * s;
  wy = ml_y + lx * s + ly * c;
}

inline void mlCrash() {
  ml_state = 3; ml_stateAt = millis();
  if (ml_lives) ml_lives--;
  for (uint8_t i = 0; i < 8; i++) {
    float ang = random(0, 628) / 100.0f;
    float sp = random(20, 90) / 100.0f;
    ml_dx[i] = ml_x; ml_dy[i] = ml_y;
    ml_dvx[i] = cosf(ang) * sp + ml_vx * 0.3f;
    ml_dvy[i] = sinf(ang) * sp - 0.3f;
  }
  mlSetTag();
  audioPlay(ML_SFX_CRASH);
}

inline void mlFinish() {
  ml_state = 4; ml_stateAt = millis();
  ml_newBest = (g_hud.score > ml_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
}

inline int8_t mlPadAt(float x) {
  for (uint8_t p = 0; p < 2; p++)
    if (x >= ml_padX[p] && x <= ml_padX[p] + ml_padW[p]) return p;
  return -1;
}

inline float mlNormAngle() {
  float a = ml_a;
  while (a > 3.14159f) a -= 6.28318f;
  while (a < -3.14159f) a += 6.28318f;
  return a;
}

inline void mlLand(uint8_t pad) {
  const MlDiff& d = ML_DIFF[g_difficulty];
  float soft = 1.0f - fabsf(ml_vy) / d.vyMax; if (soft < 0) soft = 0;
  ml_lastGain = (uint32_t)(100 * ml_padM[pad] * (g_difficulty + 1)) + (uint32_t)ml_fuel + (uint32_t)(soft * 50);
  g_hud.score += ml_lastGain;
  ml_state = 2; ml_stateAt = millis();
  ml_vx = 0; ml_vy = 0; ml_a = 0; ml_thrusting = false;
  audioPlay(ML_SFX_LAND);
}

inline void moonlanderUpdate() {
  const MlDiff& d = ML_DIFF[g_difficulty];
  uint32_t now = millis();

  if (ml_state == 4) {
    if (btnPressed(BTN_CENTER) && now - ml_stateAt > 600) mlReset();
    return;
  }
  if (ml_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN) ||
        btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) { ml_state = 1; audioPlay(ML_SFX_GO); }
    return;
  }
  if (ml_state == 2) {
    if (now - ml_stateAt > 1400) {
      ml_level++;
      float f = ml_fuel + 50; if (f > d.fuel) f = d.fuel;
      ml_fuel = f; ml_fuelStart = f;
      mlGenTerrain(); mlResetLander(); mlSetTag();
      ml_state = 1; audioPlay(ML_SFX_GO);
    }
    return;
  }
  if (ml_state == 3) {
    for (uint8_t i = 0; i < 8; i++) {
      ml_dx[i] += ml_dvx[i]; ml_dy[i] += ml_dvy[i]; ml_dvy[i] += 0.03f;
    }
    if (now - ml_stateAt > 1500) {
      if (ml_lives == 0) { mlFinish(); return; }
      ml_fuel = ml_fuelStart;
      mlResetLander();
      ml_state = 1; audioPlay(ML_SFX_GO);
    }
    return;
  }

  // ---- play ----
  if (btnDown(BTN_LEFT))  ml_a -= ML_ROT;
  if (btnDown(BTN_RIGHT)) ml_a += ML_ROT;
  ml_thrusting = btnDown(BTN_UP) && ml_fuel > 0;
  if (ml_thrusting) {
    ml_vx += sinf(ml_a) * d.thrust;
    ml_vy -= cosf(ml_a) * d.thrust;
    ml_fuel -= ML_BURN;
    if (ml_fuel < 0) ml_fuel = 0;
    if ((now / 60) & 1) audioTone(110, 20);
  }
  ml_vy += d.g;
  ml_x += ml_vx;
  ml_y += ml_vy;

  if (ml_x < 2 || ml_x > 125 || ml_y < -8) { mlCrash(); return; }

  // ground contact
  bool hit = false;
  for (uint8_t k = 0; k < 5 && !hit; k++) {
    float wx, wy;
    mlWorld(ML_CONTACT[k], wx, wy);
    if (wy >= mlTerrain((int16_t)wx)) hit = true;
  }
  if (hit) {
    int8_t pad = mlPadAt(ml_x);
    float ang = fabsf(mlNormAngle());
    if (pad >= 0 && ml_vy <= d.vyMax && fabsf(ml_vx) <= d.vxMax && ang <= d.angMax) {
      mlLand(pad);
    } else {
      mlCrash();
    }
  }
}

inline void mlDrawLander() {
  float px[7], py[7];
  for (uint8_t i = 0; i < 7; i++) mlWorld(i, px[i], py[i]);
  #define MLP(i) (int16_t)lroundf(px[i]), (int16_t)lroundf(py[i])
  gLine(MLP(0), MLP(1)); gLine(MLP(1), MLP(2)); gLine(MLP(2), MLP(3));
  gLine(MLP(3), MLP(4)); gLine(MLP(4), MLP(0));
  gLine(MLP(3), MLP(5)); gLine(MLP(2), MLP(6));
  if (ml_thrusting) {
    float c = cosf(ml_a), s = sinf(ml_a);
    float len = 4 + ((millis() / 50) & 1) * 3;
    float bx = ml_x - s, by = ml_y + c;               // flame base = local (0,1)
    float tx = ml_x - (1 + len) * s, ty = ml_y + (1 + len) * c;
    gLine((int16_t)lroundf(bx), (int16_t)lroundf(by), (int16_t)lroundf(tx), (int16_t)lroundf(ty));
  }
  #undef MLP
}

inline void moonlanderDraw() {
  g_hud.over = (ml_state == 4);
  const MlDiff& d = ML_DIFF[g_difficulty];
  uint32_t now = millis();
  gColor(1);

  // terrain outline
  for (uint8_t x = 0; x < 128; x++) {
    int16_t h = ml_h[x];
    if (x == 0) gPixel(0, h);
    else {
      int16_t a = ml_h[x - 1];
      if (a == h) gPixel(x, h);
      else if (a < h) gVLine(x, a, h - a + 1);
      else gVLine(x, h, a - h + 1);
    }
  }
  // pads: thick line + multiplier label above
  for (uint8_t p = 0; p < 2; p++) {
    int16_t y = ml_h[ml_padX[p]];
    gBox(ml_padX[p], y, ml_padW[p], 2);
    gVLine(ml_padX[p], y - 2, 2); gVLine(ml_padX[p] + ml_padW[p] - 1, y - 2, 2);
    u8g2.setFont(u8g2_font_4x6_tr);
    char b[8]; snprintf(b, sizeof b, "x%u", ml_padM[p]);
    gText(ml_padX[p] + ml_padW[p] / 2 - 4, y + 9, b);
  }

  if (ml_state == 0 || ml_state == 1 || ml_state == 2) mlDrawLander();
  if (ml_state == 3) {
    for (uint8_t i = 0; i < 8; i++) gPixel((int16_t)ml_dx[i], (int16_t)ml_dy[i]);
  }

  // instruments
  u8g2.setFont(u8g2_font_4x6_tr);
  char b[12];
  bool fast = false;
  snprintf(b, sizeof b, "F:%u", (unsigned)ml_fuel);       gText(1, 6, b);
  int16_t hv = (int16_t)lroundf(ml_vx * 30), vv = (int16_t)lroundf(ml_vy * 30);
  fast = fabsf(ml_vx) > d.vxMax;
  snprintf(b, sizeof b, "H:%d%s", hv, fast ? "!" : "");  gText(1, 13, b);
  fast = ml_vy > d.vyMax;
  snprintf(b, sizeof b, "V:%d%s", vv, fast ? "!" : "");   gText(1, 20, b);
  int16_t deg = (int16_t)lroundf(mlNormAngle() * 57.2958f);
  fast = fabsf(mlNormAngle()) > d.angMax;
  snprintf(b, sizeof b, "A:%d%s", deg, fast ? "!" : "");
  gText(127 - u8g2.getStrWidth(b), 6, b);

  if (ml_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press CENTER";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 19, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 19, w + 4, 9); gText((128 - w) / 2, 26, m);
  }
  if (ml_state == 2 || (ml_state == 3 && now - ml_stateAt > 200)) {
    char m[16];
    if (ml_state == 2) snprintf(m, sizeof m, "+%lu", (unsigned long)ml_lastGain);
    else snprintf(m, sizeof m, "CRASH");
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 3, 14, w + 6, 10);
    gColor(1); gFrame((128 - w) / 2 - 3, 14, w + 6, 10); gText((128 - w) / 2, 21, m);
  }
  if (ml_state == 4) drawGameOverPanel("GAME OVER", ml_newBest);
}
