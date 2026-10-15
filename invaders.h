#pragma once
// ============================================================
//  invaders.h - Space Invaders klón
//  Layout: WIDE (128x52 arena)
//
//  Irányítás:
//    Bal / Jobb : Űrhajó mozgatása
//    Közép / Fel: Lövés (egyszerre 1 lövedék)
//  Életek (a státuszsávban "L:n"), a találat után rövid sérthetetlenség.
//  Az idegenek az oszlopuk legalsó tagjából lőnek; minden hullám gyorsabb.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

constexpr uint8_t SI_COLS = 7;
constexpr uint8_t SI_ROWS = 3;
constexpr uint8_t SI_TOTAL = SI_COLS * SI_ROWS;
constexpr uint8_t SI_MAX_BOMBS = 3;
constexpr int16_t SI_SHIP_Y = 46;

// Nehézség: lépésköz (teljes flottával), életek, egyidejű idegen lövedékek, lövési esély (ezrelék / képkocka)
struct SiDiff { uint16_t baseDelay; uint8_t lives; uint8_t maxBombs; uint8_t fireRate; };
static const SiDiff SI_DIFF[DIFF_COUNT] = {
  {600, 4, 1, 14},   // Easy:   lassú alaplépés, 4 élet, 1 lövedék
  {400, 3, 2, 22},   // Medium: normál tempó
  {250, 2, 3, 32}    // Hard:   gyors kezdés, 2 élet, 3 lövedék
};

// --- Sprite-ok (7x5 XBM) ---
static const uint8_t SI_SHIP[5] = { 0x08, 0x1C, 0x3E, 0x7F, 0x7F };
static const uint8_t SI_ALIEN_A[5] = { 0x22, 0x1C, 0x3E, 0x6B, 0x41 };
static const uint8_t SI_ALIEN_B[5] = { 0x41, 0x22, 0x3E, 0x6B, 0x14 };

// --- Állapotváltozók ---
struct SiBomb { float x, y; bool active; };

static float    si_px;                 // Játékos X pozíció
static float    si_bx, si_by;          // Játékos lövedék pozíció
static bool     si_bActive;            // Van-e aktív játékos lövedék
static SiBomb   si_bombs[SI_MAX_BOMBS];// Idegen lövedékek

static bool     si_aliens[SI_TOTAL];   // Idegenek élnek-e (Tömb)
static float    si_ax, si_ay;          // Az idegen rács bal felső sarka
static int8_t   si_adir;               // Haladási irány (1: jobb, -1: bal)
static uint8_t  si_aliveCount;         // Életben lévő idegenek száma
static uint32_t si_lastMove;           // Idegenek utolsó lépésének ideje
static uint8_t  si_anim;               // Animációs számláló

static uint8_t  si_lives;
static uint8_t  si_wave;               // 0-tól számolva
static uint32_t si_invulnUntil;        // találat utáni sérthetetlenség vége

static uint8_t  si_state;              // 0: Ready, 1: Play, 2: Game Over
static uint32_t si_deadAt;
static bool     si_newBest;
static uint32_t si_bestAtStart;

// --- Hangok ---
static const Note SI_SFX_SHOOT[] = { {1200, 15}, {800, 20} };
static const Note SI_SFX_BOOM[]  = { {1500, 20}, {900, 25} };
static const Note SI_SFX_HIT[]   = { {300, 80}, {200, 120}, {150, 150} };
static const Note SI_SFX_WAVE[]  = { {1000, 50}, {1300, 50}, {1700, 100} };
static const Note SI_SFX_DEAD[]  = { {200, 150}, {150, 200}, {100, 300} };

// ---------------- Segédfüggvények ----------------
inline void siSpawnWave() {
  for (uint8_t i = 0; i < SI_TOTAL; i++) si_aliens[i] = true;
  si_aliveCount = SI_TOTAL;
  si_ax = 10;
  si_ay = 2 + (si_wave > 2 ? 2 : si_wave) * 3;   // a későbbi hullámok lejjebb indulnak (max +6 px)
  si_adir = 1;
  si_bActive = false;
  for (uint8_t i = 0; i < SI_MAX_BOMBS; i++) si_bombs[i].active = false;
}

inline void siReset() {
  si_state = 0;
  si_px = 60; // Középen kezd
  si_wave = 0;
  si_lives = SI_DIFF[g_difficulty].lives;
  si_invulnUntil = 0;
  g_hud.score = 0;
  g_hud.lives = si_lives;
  si_newBest = false;
  si_bestAtStart = g_highScore[g_curGame][g_difficulty];
  siSpawnWave();
}

inline void invadersInit() { siReset(); }

inline void siGameOver() {
  si_state = 2;
  si_deadAt = millis();
  si_newBest = (g_hud.score > si_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(SI_SFX_DEAD);
}

// Egy idegen lövedék indítása (az oszlopa legalsó élő tagjából), ha a szabályok engedik
inline void siTryFire() {
  const SiDiff& D = SI_DIFF[g_difficulty];
  int8_t slot = -1; uint8_t active = 0;
  for (uint8_t i = 0; i < SI_MAX_BOMBS; i++) {
    if (si_bombs[i].active) active++;
    else if (slot < 0) slot = i;
  }
  if (slot < 0 || active >= D.maxBombs) return;
  if (random(1000) >= D.fireRate) return;

  uint8_t startC = random(SI_COLS);
  for (uint8_t k = 0; k < SI_COLS; k++) {
    uint8_t c = (startC + k) % SI_COLS;
    for (int8_t r = SI_ROWS - 1; r >= 0; r--) {
      if (si_aliens[r * SI_COLS + c]) {
        si_bombs[slot].x = si_ax + c * 11 + 3;     // Alien közepe
        si_bombs[slot].y = si_ay + r * 8 + 5;      // Alien alja
        si_bombs[slot].active = true;
        return;
      }
    }
  }
}

inline void invadersUpdate() {
  if (si_state == 2) {
    if (btnPressed(BTN_CENTER) && millis() - si_deadAt > 600) siReset();
    return;
  }

  // READY Állapot
  if (si_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP)) {
      si_state = 1;
      si_lastMove = millis();
    }
    return;
  }

  // --- JÁTÉKOS MOZGATÁSA ---
  if (btnDown(BTN_LEFT) && si_px > 0) si_px -= 1.5f;
  if (btnDown(BTN_RIGHT) && si_px < 121) si_px += 1.5f; // 128 - 7 (hajó szélessége)

  // --- LÖVÉS (Csak 1 lövedék lehet a képernyőn egyszerre) ---
  if ((btnPressed(BTN_CENTER) || btnPressed(BTN_UP)) && !si_bActive) {
    si_bActive = true;
    si_bx = si_px + 3.0f; // A hajó közepe
    si_by = 45.0f;
    audioPlay(SI_SFX_SHOOT);
  }

  // Játékos lövedékének mozgatása
  if (si_bActive) {
    si_by -= 3.5f;
    if (si_by < -2) si_bActive = false;
  }

  // --- IDEGEN LÖVEDÉKEK ---
  siTryFire();
  bool invuln = (int32_t)(si_invulnUntil - millis()) > 0;
  for (uint8_t i = 0; i < SI_MAX_BOMBS; i++) {
    SiBomb& b = si_bombs[i];
    if (!b.active) continue;
    b.y += 1.2f;
    if (b.y > 52) { b.active = false; continue; }

    // Ütközés a hajóval (lövedék 2x4, hajó 7x5)
    if (!invuln && b.x + 2 > si_px && b.x < si_px + 7 &&
        b.y + 4 > SI_SHIP_Y && b.y < SI_SHIP_Y + 5) {
      b.active = false;
      si_lives--;
      g_hud.lives = si_lives;
      if (si_lives == 0) {
        siGameOver();
        return;
      }
      si_invulnUntil = millis() + 1500;
      invuln = true;
      for (uint8_t k = 0; k < SI_MAX_BOMBS; k++) si_bombs[k].active = false;
      audioPlay(SI_SFX_HIT);
      break;
    }
  }

  // --- IDEGENEK MOZGATÁSA (Klasszikus ugráló rács) ---
  uint8_t waveCap = si_wave > 4 ? 4 : si_wave;                  // hullámonként 8%-kal gyorsabb
  uint32_t stepMs = (uint32_t)SI_DIFF[g_difficulty].baseDelay * si_aliveCount / SI_TOTAL;
  stepMs = stepMs * (100 - 8 * waveCap) / 100 + 20;             // egyre rövidebb a szünet

  if (millis() - si_lastMove > stepMs) {
    si_lastMove = millis();
    si_anim++;

    // Klasszikus Space Invaders tempó-hang (dobogás) - csak ha nem szól épp más hang
    static const uint16_t STEP_PITCH[4] = { 110, 104, 98, 92 };
    if (!audioBusy()) audioTone(STEP_PITCH[si_anim % 4], 20);

    // Megkeressük a flotta valós bal és jobb szélét, valamint az alját
    float minX = 128.0f, maxX = 0.0f, maxY = 0.0f;
    for (uint8_t i = 0; i < SI_TOTAL; i++) {
      if (si_aliens[i]) {
        uint8_t c = i % SI_COLS;
        uint8_t r = i / SI_COLS;
        float ax = si_ax + c * 11; // 11px távolság (7px test + 4px szünet)
        float ay = si_ay + r * 8;  // 8px sortávolság
        if (ax < minX) minX = ax;
        if (ax > maxX) maxX = ax;
        if (ay > maxY) maxY = ay;
      }
    }

    // Ütköztek-e a fallal?
    bool hitWall = false;
    if (si_adir == 1 && maxX > 118) hitWall = true; // 128 - 7(szélesség) - 3(margó)
    if (si_adir == -1 && minX < 3) hitWall = true;

    if (hitWall) {
      si_adir = -si_adir; // Irányváltás
      si_ay += 4;         // Ugrás lefelé

      // Ha az aljuk hozzáér a hajó magasságához (Y=46) -> Game Over (azonnali, életektől függetlenül)
      if (maxY + 4 + 5 >= SI_SHIP_Y) {
        siGameOver();
        return;
      }
    } else {
      si_ax += si_adir * 4; // Ugrás oldalra
    }
  }

  // --- ÜTKÖZÉS: Játékos lövedéke <-> Idegenek ---
  if (si_bActive) {
    for (uint8_t i = 0; i < SI_TOTAL; i++) {
      if (si_aliens[i]) {
        uint8_t c = i % SI_COLS;
        uint8_t r = i / SI_COLS;
        float ax = si_ax + c * 11;
        float ay = si_ay + r * 8;

        // A lövedék 4 px hosszú függőleges vonal: sávként vizsgáljuk (nem "alagutazik" át)
        if (si_bx >= ax && si_bx <= ax + 6 && si_by <= ay + 4 && si_by + 3 >= ay) {
          si_aliens[i] = false;  // Megölve
          si_bActive = false;    // Lézer eltüntetve
          si_aliveCount--;

          g_hud.score += 10 + (2 - r) * 10; // Felső sorok több pont
          audioPlay(SI_SFX_BOOM);
          break;
        }
      }
    }
  }

  // --- KÖVETKEZŐ HULLÁM ---
  if (si_aliveCount == 0) {
    si_wave++;
    siSpawnWave();
    g_hud.score += 100;
    audioPlay(SI_SFX_WAVE);
  }
}

inline void invadersDraw() {
  g_hud.over = (si_state == 2);
  // 1. Játékos űrhajó (Mérete 7x5) - sérthetetlenség alatt villog
  bool invuln = (int32_t)(si_invulnUntil - millis()) > 0;
  if (si_state != 2 && (!invuln || ((millis() / 100) & 1))) {
    u8g2.drawXBM(g_ox + (int16_t)si_px, g_oy + SI_SHIP_Y, 7, 5, SI_SHIP);
  }

  // 2. Idegen flotta (Mérete 7x5)
  const uint8_t* alienSpr = (si_anim % 2) ? SI_ALIEN_A : SI_ALIEN_B;
  for (uint8_t i = 0; i < SI_TOTAL; i++) {
    if (si_aliens[i]) {
      uint8_t c = i % SI_COLS;
      uint8_t r = i / SI_COLS;
      int16_t ax = (int16_t)(si_ax + c * 11);
      int16_t ay = (int16_t)(si_ay + r * 8);
      u8g2.drawXBM(g_ox + ax, g_oy + ay, 7, 5, alienSpr);
    }
  }

  // 3. Lövedékek
  if (si_bActive)  gVLine((int16_t)si_bx, (int16_t)si_by, 4);      // Játékos vékony lézere
  for (uint8_t i = 0; i < SI_MAX_BOMBS; i++)
    if (si_bombs[i].active) gBox((int16_t)si_bombs[i].x, (int16_t)si_bombs[i].y, 2, 4);   // Alien vastagabb lézere

  // 4. Game Over / Ready UI
  if (si_state == 0) {
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* msg = "Press FIRE to Start";
    int16_t w = u8g2.getStrWidth(msg);
    int16_t tx = (128 - w) / 2;

    // Fekete háttérdoboz a szöveg mögé, hogy kitakarja az űrlényeket
    gColor(0);
    gBox(tx - 4, 27, w + 8, 12);
    // Vékony fehér keret a doboz köré
    gColor(1);
    gFrame(tx - 4, 27, w + 8, 12);

    // Szöveg rárajzolása
    gText(tx, 36, msg);

  } else if (si_state == 2) {
    drawGameOverPanel("GAME OVER", si_newBest);
  }
}
