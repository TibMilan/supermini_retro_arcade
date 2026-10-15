#pragma once
// ============================================================
//  dino.h - Dino Run (Chrome-stilusu futo/ugralo jatek)
//  Layout: WIDE (128x52 arena)
//
//  Iranyitas:
//    Center / Up : ugras (mindig teljes magassagu - egy rovid koppintas is eleg)
//    Down        : lebukas (fold kozeleben) / gyors esés (levegoben)
//  Nehezseg (Easy/Medium/Hard): kezdo- es max. sebesseg, gyorsulas,
//    akadaly-gyakorisag, madarak megjelenese.
//  Halal utan 0.6 mp mulva Center/Up = ujrakezdes. A rekord automatikusan mentodik.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

// ---------------- Sprite-ok (XBM, LSB = bal szel) ----------------
// Dino 16x16: az 1..15. sor kozos, a labak (utolso sor) kulonboznek
#define DINO_ROWS_0_14 \
  0x00,0x7C, 0x00,0xFC, 0x00,0xF4, 0x00,0xFC, 0x00,0xFC, 0x00,0x1C, 0x01,0x1E, 0x01,0xBF, \
  0x83,0x7F, 0xC7,0x3F, 0xFF,0x1F, 0xFE,0x1F, 0xFC,0x0F, 0xF8,0x07, 0x30,0x03

static const uint8_t DINO_A[32]     = { DINO_ROWS_0_14, 0x70, 0x00 };   // bal lab lent
static const uint8_t DINO_B[32]     = { DINO_ROWS_0_14, 0x00, 0x07 };   // jobb lab lent
static const uint8_t DINO_STAND[32] = { DINO_ROWS_0_14, 0x70, 0x07 };   // levegoben: mindket lab

// Lebukott dino 16x9
static const uint8_t DUCK_A[18] = {
  0x00,0x7C, 0x00,0xFC, 0x00,0xF4, 0xC1,0x7F, 0xFF,0x7F, 0xFF,0x3F, 0xFE,0x1F, 0xFC,0x0F, 0x30,0x00
};
static const uint8_t DUCK_B[18] = {
  0x00,0x7C, 0x00,0xFC, 0x00,0xF4, 0xC1,0x7F, 0xFF,0x7F, 0xFF,0x3F, 0xFE,0x1F, 0xFC,0x0F, 0x00,0x03
};

// Kaktuszok
static const uint8_t CACTUS_SMALL[12] = { 0x08,0x1C,0x5C,0x5D,0x7D,0x1F,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C };   // 7x12
static const uint8_t CACTUS_LARGE[32] = {                                                                // 9x16
  0x10,0x00, 0x38,0x00, 0x38,0x01, 0x39,0x01, 0x39,0x01, 0xF9,0x01, 0x3F,0x00,
  0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00, 0x38,0x00
};

// Madar 14x8 (ket szarnyallas)
static const uint8_t BIRD_A[16] = {
  0x20,0x00, 0x30,0x00, 0x78,0x00, 0xFB,0x01, 0xFE,0x1F, 0xFC,0x07, 0xF0,0x03, 0x80,0x01
};
static const uint8_t BIRD_B[16] = {
  0xFB,0x01, 0xFE,0x1F, 0xFC,0x07, 0xF8,0x01, 0xF8,0x00, 0xF0,0x00, 0x60,0x00, 0x40,0x00
};

// ---------------- Hangok ----------------
static const Note D_SFX_JUMP[] = { {600, 25}, {900, 35} };
static const Note D_SFX_MILE[] = { {1568, 40}, {0, 25}, {1568, 60} };
static const Note D_SFX_DEAD[] = { {440, 90}, {330, 90}, {220, 240} };

// ---------------- Konstansok ----------------
constexpr int16_t D_GROUND_Y = 46;      // talaj vonala (arena-koordinata)
constexpr int16_t D_DINO_X   = 10;
constexpr float   D_GRAVITY  = 0.55f;    // px / kepkocka^2
constexpr float   D_JUMP_V   = 5.6f;    // kezdosebesseg (csucs ~26 px, levegoben ~19 kepkocka)
// Minimalis "idoablak" (kepkockaban), amennyi hibat a jatekos elkovethet az ugras idozitesekor.
// 6 kepkocka = 200 ms. Az akadaly-csoportok merete ebbol van korlatozva (lasd dMaxCount).
constexpr float   D_MIN_WINDOW = 6.0f;

struct DinoDiff {
  float    v0, vmax, acc;      // kezdosebesseg, max sebesseg, gyorsulas / kepkocka (px/kepkocka)
  uint16_t birdScore;          // ennyi pont utan jelennek meg madarak
  uint8_t  birdPct;            // madar esely (%)
  float    gapMin, gapMax;     // akadalyok kozti tavolsag szorzo (ugrashossz-egysegben)
};
static const DinoDiff D_DIFF[DIFF_COUNT] = {
{ 3.0f, 6.0f, 0.0015f, 300, 15, 1.5f, 2.8f },     // Easy
  { 4.0f, 7.5f, 0.0020f, 200, 25, 1.3f, 2.4f },   // Medium
  { 5.0f, 9.0f, 0.0025f, 100, 35, 1.1f, 1.9f },   // Hard
};

struct DObstacle { bool active; uint8_t kind; uint8_t count; float x; int16_t y; };  // kind: 0 kis kaktusz, 1 nagy kaktusz, 2 madar
struct DCloud    { float x; int16_t y; };

enum DinoState : uint8_t { D_PLAY, D_DEAD };

static DinoState d_state;
static float     d_speed, d_dist, d_scroll, d_dy, d_vy, d_sinceSpawn, d_nextGap;
static bool      d_duck, d_newBest;
static uint8_t   d_jumpBuf, d_anim;
static uint32_t  d_deadAt, d_bestAtStart, d_lastMilestone;
static DObstacle d_obs[4];
static DCloud    d_clouds[3];

// ---------------- Segedek ----------------
inline int16_t dObsW(const DObstacle& o) {
  if (o.kind == 2) return 14;
  int16_t w = (o.kind == 0) ? 7 : 9;
  return o.count * w + (o.count - 1);
}
inline int16_t dObsH(uint8_t kind) { return kind == 0 ? 12 : (kind == 1 ? 16 : 8); }

inline bool dRectHit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                     int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

// Hany kepkockaig van a dino legalabb h pixellel a fold folott egy teljes ugras alatt
inline uint8_t dFramesAbove(float h) {
  float dy = 0.0f, vy = D_JUMP_V;
  uint8_t n = 0;
  for (uint8_t i = 0; i < 60; i++) {
    vy -= D_GRAVITY; dy += vy;
    if (dy <= 0.0f) break;
    if (dy >= h) n++;
  }
  return n;
}

// Legnagyobb kaktusz-csoport, amit az aktualis sebessegnel meg igazsagosan at lehet ugrani:
// ablak = (ennyi kepkocka van elég magasan) - (csoport szelessege + dino test) / sebesseg
inline uint8_t dMaxCount(uint8_t kind) {
  const bool large    = (kind == 1);
  const int16_t hitW  = large ? 7 : 5;          // egy kaktusz talalati szelessege
  const int16_t pitch = large ? 10 : 8;         // kaktuszok kozti lepeskoz
  const float   need  = large ? 13.0f : 10.0f;  // ennyi px magasra kell emelkedni (hitbox teteje)
  const uint8_t maxN  = large ? 2 : 3;
  const float frames  = dFramesAbove(need);
  for (uint8_t n = maxN; n > 1; n--) {
    float span = hitW + (n - 1) * pitch + 10;   // + dino test (10 px)
    if (frames - span / d_speed >= D_MIN_WINDOW) return n;
  }
  return 1;
}

inline void dinoReset() {
  const DinoDiff& D = D_DIFF[g_difficulty];
  d_state = D_PLAY;
  d_speed = D.v0;
  d_dist = 0; d_scroll = 0; d_dy = 0; d_vy = 0;
  d_duck = false; d_jumpBuf = 0; d_anim = 0;
  d_sinceSpawn = 0; d_nextGap = 150;
  d_newBest = false; d_lastMilestone = 0;
  for (uint8_t i = 0; i < 4; i++) d_obs[i].active = false;
  for (uint8_t i = 0; i < 3; i++) { d_clouds[i].x = 25 + i * 45; d_clouds[i].y = 3 + random(0, 12); }
  d_bestAtStart = g_highScore[g_curGame][g_difficulty];
  g_hud.score = 0;
  g_hud.lives = 0;
  g_hud.showNext = false;
}

inline void dSpawn(const DinoDiff& D) {
  int8_t slot = -1;
  for (uint8_t i = 0; i < 4; i++) if (!d_obs[i].active) { slot = i; break; }
  if (slot < 0) return;
  DObstacle& o = d_obs[slot];
  o.active = true;
  o.x = 128;
  bool bird = (d_dist / 4 >= D.birdScore) && (random(100) < D.birdPct);
  
  if (bird) {
    o.kind = 2; o.count = 1;
    int r = random(3);
    // 36: also (ugrani kell), 28: kozepso (hajolni kell), 16: felso (atmegy feletted)
    o.y = (r == 0) ? 36 : (r == 1 ? 28 : 16);  
  } else {
    o.kind = (random(100) < 55) ? 0 : 1;
    // Az akadaly-csoport merete a sebessegbol szamolt idozitesi ablakhoz igazodik
    o.count = random(1, dMaxCount(o.kind) + 1);
    o.y = D_GROUND_Y - dObsH(o.kind);
  }
  
  float jumpDist = d_speed * 20.5f; // a levegoben toltott idohoz (~19 kepkocka) igazitva
  float mult = D.gapMin + (D.gapMax - D.gapMin) * (random(1000) / 1000.0f);
  d_nextGap = (jumpDist + dObsW(o)) * mult;
  d_sinceSpawn = 0;
}

inline bool dCollides() {
  int16_t h = d_duck ? 9 : 16;
  int16_t top = D_GROUND_Y - h - (int16_t)d_dy;
  int16_t r[2][4];
  if (d_duck) {
    r[0][0] = D_DINO_X + 1;  r[0][1] = top + 3; r[0][2] = 14; r[0][3] = 6;
    r[1][0] = D_DINO_X + 11; r[1][1] = top;     r[1][2] = 4;  r[1][3] = 3;
  } else {
    r[0][0] = D_DINO_X + 2;  r[0][1] = top + 6; r[0][2] = 10; r[0][3] = 10;
    r[1][0] = D_DINO_X + 11; r[1][1] = top;     r[1][2] = 4;  r[1][3] = 6;
  }
  for (uint8_t i = 0; i < 4; i++) {
    const DObstacle& o = d_obs[i];
    if (!o.active) continue;
    if (o.kind == 2) {
      for (uint8_t k = 0; k < 2; k++)
        if (dRectHit(r[k][0], r[k][1], r[k][2], r[k][3], (int16_t)o.x + 2, o.y + 1, 10, 6)) return true;
    } else {
      int16_t w = (o.kind == 0) ? 7 : 9, ch = dObsH(o.kind);
      int16_t topCut = (o.kind == 1) ? 3 : 2;       // a kaktusz vekony cscsat nem szamoljuk talalatnak
      for (uint8_t c = 0; c < o.count; c++) {
        int16_t cx = (int16_t)o.x + c * (w + 1);
        for (uint8_t k = 0; k < 2; k++)
          if (dRectHit(r[k][0], r[k][1], r[k][2], r[k][3], cx + 1, o.y + topCut, w - 2, ch - topCut)) return true;
      }
    }
  }
  return false;
}

// ---------------- Jatek-interfesz ----------------
inline void dinoInit() { dinoReset(); }

inline void dinoUpdate() {
  const DinoDiff& D = D_DIFF[g_difficulty];
  bool jumpPress = btnPressed(BTN_CENTER) || btnPressed(BTN_UP);
  bool duckHeld  = btnDown(BTN_DOWN);

  if (d_state == D_DEAD) {
    if (jumpPress && millis() - d_deadAt > 600) dinoReset();
    return;
  }

  // --- Ugras (rovid bemeneti puffer: kozvetlenul landing elotti nyomas is szamit) ---
  if (jumpPress) d_jumpBuf = 5;
  else if (d_jumpBuf) d_jumpBuf--;

  bool onGround = (d_dy <= 0.0f);
  if (onGround && d_jumpBuf) {
    d_vy = D_JUMP_V;
    d_jumpBuf = 0;
    onGround = false;
    audioPlay(D_SFX_JUMP);
  }
  if (!onGround) {
    float g = D_GRAVITY * (duckHeld ? 2.5f : 1.0f);             // Down: gyors esés
    d_vy -= g;
    d_dy += d_vy;
    if (d_dy <= 0.0f) { d_dy = 0.0f; d_vy = 0.0f; }
  }
  d_duck = duckHeld;
  d_anim++;

  // --- Sebesseg / tavolsag / pont ---
  d_speed += D.acc;
  if (d_speed > D.vmax) d_speed = D.vmax;
  d_dist += d_speed;
  d_scroll += d_speed;
  uint32_t score = (uint32_t)(d_dist / 4);
  g_hud.score = score;
  if (score / 100 > d_lastMilestone) {
    d_lastMilestone = score / 100;
    audioPlay(D_SFX_MILE);
  }

  // --- Felhok ---
  for (uint8_t i = 0; i < 3; i++) {
    d_clouds[i].x -= d_speed * 0.25f;
    if (d_clouds[i].x < -16) { d_clouds[i].x = 128 + random(0, 40); d_clouds[i].y = 3 + random(0, 12); }
  }

  // --- Akadalyok ---
  for (uint8_t i = 0; i < 4; i++) {
    DObstacle& o = d_obs[i];
    if (!o.active) continue;
    o.x -= (o.kind == 2) ? d_speed * 1.15f : d_speed;
    if (o.x + dObsW(o) < -2) o.active = false;
  }
  d_sinceSpawn += d_speed;
  if (d_sinceSpawn >= d_nextGap) dSpawn(D);

  // --- utkozes ---
  if (dCollides()) {
    d_state = D_DEAD;
    d_deadAt = millis();
    d_newBest = score > d_bestAtStart;
    storageSubmitScore(g_curGame, g_difficulty, score);
    audioPlay(D_SFX_DEAD);
  }
}

inline void dinoDraw() {
  g_hud.over = (d_state == D_DEAD);
  // Felhok
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = (int16_t)d_clouds[i].x, y = d_clouds[i].y;
    gHLine(x + 3, y, 6);
    gHLine(x, y + 1, 13);
    gHLine(x + 2, y + 2, 9);
  }

  // Talaj + gorgo egyenetlensegek
  gHLine(0, D_GROUND_Y, 128);
  int s = (int)d_scroll % 152;
  for (uint8_t k = 0; k < 8; k++) {
    int bx = (k * 19 - s + 152) % 152;
    if (bx < 128) gHLine(bx, 48 + (k & 1) * 2, 1 + (k % 3));
  }

  // Akadalyok
  for (uint8_t i = 0; i < 4; i++) {
    const DObstacle& o = d_obs[i];
    if (!o.active) continue;
    if (o.kind == 2) {
      int16_t x = (int16_t)o.x;
      if (x > 127 || x + 14 < 0) continue;
      u8g2.drawXBM(g_ox + x, g_oy + o.y, 14, 8, ((d_anim / 6) & 1) ? BIRD_A : BIRD_B);
    } else {
      int16_t w = (o.kind == 0) ? 7 : 9, h = dObsH(o.kind);
      const uint8_t* bits = (o.kind == 0) ? CACTUS_SMALL : CACTUS_LARGE;
      for (uint8_t c = 0; c < o.count; c++) {
        int16_t x = (int16_t)o.x + c * (w + 1);
        if (x > 127 || x + w < 0) continue;
        u8g2.drawXBM(g_ox + x, g_oy + o.y, w, h, bits);
      }
    }
  }

  // Dino
  bool air = (d_dy > 0.0f);
  if (d_duck && d_state == D_PLAY) {
    int16_t top = D_GROUND_Y - 9 - (int16_t)d_dy;
    const uint8_t* bits = air ? DUCK_A : (((d_anim / 4) & 1) ? DUCK_A : DUCK_B);
    u8g2.drawXBM(g_ox + D_DINO_X, g_oy + top, 16, 9, bits);
  } else {
    int16_t top = D_GROUND_Y - 16 - (int16_t)d_dy;
    const uint8_t* bits = (air || d_state == D_DEAD) ? DINO_STAND : (((d_anim / 4) & 1) ? DINO_A : DINO_B);
    u8g2.drawXBM(g_ox + D_DINO_X, g_oy + top, 16, 16, bits);
  }

  // Game over felirat
  if (d_state == D_DEAD) {
    drawGameOverPanel("GAME OVER", d_newBest);
  }
}
