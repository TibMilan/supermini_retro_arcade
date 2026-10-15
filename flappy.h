#pragma once
// ============================================================
//  flappy.h - Flappy Bird klón
//  Layout: WIDE (128x52 arena)
//
//  Irányítás: Center / Up = Ugrás (Szárnycsapás)
//  Nehézség (Easy/Medium/Hard): Csövek sebessége, lyukak
//  mérete (gap) és a csövek közötti távolság.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

// ---------------- Sprite-ok (XBM, 16x8) ----------------
static const uint8_t F_BIRD_UP[16] = {
  0x00,0x00, 0x80,0x01, 0x40,0x03, 0xFE,0x07, 0x81,0x0F, 0xBD,0x09, 0xC3,0x07, 0x00,0x00
};
static const uint8_t F_BIRD_DOWN[16] = {
  0x00,0x00, 0xE0,0x03, 0x10,0x04, 0x08,0x0A, 0xFC,0x0B, 0x02,0x08, 0xFC,0x07, 0x00,0x00
};

// ---------------- Hangok ----------------
static const Note F_SFX_JUMP[]  = { {800, 15}, {1100, 25} };
static const Note F_SFX_SCORE[] = { {1400, 30}, {1800, 40} };
static const Note F_SFX_HIT[]   = { {250, 80}, {150, 120}, {100, 200} };

// ---------------- Konstansok ----------------
constexpr int16_t F_GROUND_Y = 48;       // Talaj Y koordináta (max 52 lehet az aréna miatt)
constexpr int16_t F_BIRD_X   = 24;       // A madár fix X pozíciója
constexpr float   F_GRAVITY  = 0.25f;    // Gravitáció (px / frame^2)
constexpr float   F_JUMP_V   = 2.4f;     // Ugrás kezdősebessége (egy csapás ~11 px; 2.8 mellett Hard gyakorlatilag lehetetlen volt)

struct FlappyDiff { 
  float speed;       // Csövek mozgási sebessége (px/frame)
  int8_t gapH;       // A csövek közötti rés magassága (px)
  float spawnDist;   // Milyen messze generálódjon a következő cső
  int8_t maxStep;    // Két egymást követő rés középpontja legfeljebb ennyivel térhet el (px)
};
static const FlappyDiff F_DIFF[DIFF_COUNT] = {
  { 1.2f, 32, 85.0f, 10 },   // Easy
  { 1.6f, 26, 75.0f, 13 },   // Medium
  { 2.2f, 24, 65.0f, 15 }    // Hard
};

struct FPipe { float x; int8_t gapY; bool active; bool passed; };
enum FState : uint8_t { F_READY, F_PLAY, F_DEAD };

static FState   f_state;
static float    f_y, f_vy, f_scroll;
static uint32_t f_deadAt;
static FPipe    f_pipes[3];
static bool     f_newBest;
static int16_t  f_lastGapY;     // az elozo cso resenek kozepe (-1 = meg nincs)
static uint32_t f_bestAtStart;

// ---------------- Segédek ----------------
inline bool fRectHit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                     int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

inline void fSpawnPipe() {
  const FlappyDiff& D = F_DIFF[g_difficulty];
  for (uint8_t i = 0; i < 3; i++) {
    if (!f_pipes[i].active) {
      f_pipes[i].active = true;
      f_pipes[i].passed = false;
      f_pipes[i].x = 128.0f;
      // Véletlenszerű rés-magasság. A régi 10 px-es margóval a tartomány Easy-n 1 értékre,
      // Medium-on 2-re zsugorodott (a random(min,max) a min-et adja, ha min >= max).
      const int16_t M  = 3;                                  // legalább ennyi cső látszik fent/lent
      int16_t lo = M + D.gapH / 2;
      int16_t hi = F_GROUND_Y - M - D.gapH / 2;              // inkluzív
      if (f_lastGapY >= 0) {                                 // ne ugorjon túl nagyot az előző réshez képest
        if (lo < f_lastGapY - D.maxStep) lo = f_lastGapY - D.maxStep;
        if (hi > f_lastGapY + D.maxStep) hi = f_lastGapY + D.maxStep;
      }
      f_pipes[i].gapY = random(lo, hi + 1);
      f_lastGapY = f_pipes[i].gapY;
      break;
    }
  }
}

inline void flappyReset() {
  f_state = F_READY;
  f_y = 20.0f;
  f_vy = 0.0f;
  f_scroll = 0.0f;
  f_newBest = false;
  f_lastGapY = -1;
  f_bestAtStart = g_highScore[g_curGame][g_difficulty]; // Rekord mentése induláskor
  for (uint8_t i = 0; i < 3; i++) f_pipes[i].active = false;
  g_hud.score = 0;
  g_hud.showNext = false;
}

// ---------------- Játék-interfész ----------------
inline void flappyInit() { flappyReset(); }

inline void flappyUpdate() {
  const FlappyDiff& D = F_DIFF[g_difficulty];
  bool jumpPress = btnPressed(BTN_CENTER) || btnPressed(BTN_UP);

  if (f_state == F_DEAD) {
    if (jumpPress && millis() - f_deadAt > 600) flappyReset();
    
    // Halál után a madár lezuhan a talajig
    if (f_y < F_GROUND_Y - 8) {
      f_vy -= F_GRAVITY * 1.5f;
      f_y -= f_vy;
      if (f_y >= F_GROUND_Y - 8) { f_y = F_GROUND_Y - 8; f_vy = 0; }
    }
    return;
  }

  // --- READY Állapot (Várakozás a kezdésre) ---
  if (f_state == F_READY) {
    f_y = 20.0f + sin(millis() / 150.0f) * 2.5f; // Lebegés
    if (jumpPress) {
      f_state = F_PLAY;
      f_vy = F_JUMP_V;
      audioPlay(F_SFX_JUMP);
      fSpawnPipe();
    }
    return;
  }

  // --- PLAY Állapot ---
  if (jumpPress) {
    f_vy = F_JUMP_V;
    audioPlay(F_SFX_JUMP);
  }

  // Fizika (Felfelé a koordináta csökken)
  f_vy -= F_GRAVITY;
  f_y -= f_vy;
  f_scroll += D.speed;

  // Akadályok mozgatása
  float maxPx = 0.0f;
  for (uint8_t i = 0; i < 3; i++) {
    if (f_pipes[i].active) {
      f_pipes[i].x -= D.speed;
      if (f_pipes[i].x > maxPx) maxPx = f_pipes[i].x;

      // Pontszerzés (A madár áthaladt a csövön)
      if (!f_pipes[i].passed && f_pipes[i].x + 12 < F_BIRD_X) {
        f_pipes[i].passed = true;
        g_hud.score++;
        audioPlay(F_SFX_SCORE);
      }

      // Kiment a képernyőről
      if (f_pipes[i].x < -16.0f) f_pipes[i].active = false;
    }
  }

  // Új cső generálása
  if (128.0f - maxPx >= D.spawnDist) fSpawnPipe();

  // --- Ütközésvizsgálat ---
  int16_t by = (int16_t)f_y;
  bool hit = false;
  
  // Padló vagy plafon érintése
  if (by < -4 || by >= F_GROUND_Y - 7) hit = true;

  for (uint8_t i = 0; i < 3; i++) {
    if (!f_pipes[i].active) continue;
    int16_t px = (int16_t)f_pipes[i].x;
    int16_t gapTop = f_pipes[i].gapY - D.gapH / 2;
    int16_t gapBot = f_pipes[i].gapY + D.gapH / 2;

    // A madár hitboxa picit kisebb, mint a 16x8-as rajzolt sprite (elnézőbb a játékoshoz)
    // Felső cső: (px, 0) - (px+12, gapTop)
    if (fRectHit(F_BIRD_X + 2, by + 2, 12, 5, px, 0, 12, gapTop)) hit = true;
    
    // Alsó cső: (px, gapBot) - (px+12, F_GROUND_Y-gapBot)
    if (fRectHit(F_BIRD_X + 2, by + 2, 12, 5, px, gapBot, 12, F_GROUND_Y - gapBot)) hit = true;
  }

  if (hit) {
    f_state = F_DEAD;
    f_deadAt = millis();
    f_newBest = (g_hud.score > f_bestAtStart); // Megvizsgáljuk, dőlt-e rekord
    storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
    audioPlay(F_SFX_HIT);
  }
}

inline void flappyDraw() {
  g_hud.over = (f_state == F_DEAD);
  // 1. Háttér és Talaj
  gHLine(0, F_GROUND_Y, 128);
  int s = (int)f_scroll % 12;
  for (int i = 0; i < 12; i++) {
     gHLine(i * 12 - s, F_GROUND_Y + 2, 6); // Görgetett vonalak a földön
  }

  // 2. Csövek
  for (uint8_t i = 0; i < 3; i++) {
    if (!f_pipes[i].active) continue;
    int16_t px = (int16_t)f_pipes[i].x;
    int16_t gapTop = f_pipes[i].gapY - F_DIFF[g_difficulty].gapH / 2;
    int16_t gapBot = f_pipes[i].gapY + F_DIFF[g_difficulty].gapH / 2;

    if (px > 127 || px + 14 < 0) continue;

    // Felső cső (Test + Gallér)
    gFrame(px, 0, 12, gapTop);
    gBox(px - 1, gapTop - 4, 14, 4);

    // Alsó cső (Test + Gallér)
    gFrame(px, gapBot, 12, F_GROUND_Y - gapBot);
    gBox(px - 1, gapBot, 14, 4);
  }

  // 3. Madár rajzolása
  int16_t by = (int16_t)f_y;
  const uint8_t* bits = (f_vy > 0.0f && f_state != F_DEAD) ? F_BIRD_UP : F_BIRD_DOWN;
  if (f_state == F_DEAD) bits = F_BIRD_UP; // Zuhanáskor merev
  
  u8g2.drawXBM(g_ox + F_BIRD_X, g_oy + by, 16, 8, bits);

  // 4. Feliratok
  if (f_state == F_READY) {
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* msg = "Press JUMP";
    gText((128 - u8g2.getStrWidth(msg)) / 2, 35, msg);
  } else if (f_state == F_DEAD) {
    drawGameOverPanel("GAME OVER", f_newBest);
  }
}