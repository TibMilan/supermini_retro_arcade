#pragma once
// ============================================================
//  tetris.h - Classic Tetris
//  Layout: SQUARE (64x64 arena, oldalsávok a rendszerből)
//
//  Irányítás: 
//    Bal / Jobb : Mozgatás (nyomva tartva gyorsan csúszik - DAS)
//    Fel        : Forgatás
//    Le         : Gyorsítás (Soft Drop)
//    Közép      : Azonnali leejtés (Hard Drop)
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

// ---------------- Tetromino Formák (4x4 Bitmaszkok) ----------------
// bit 0 = bal felső (r=0, c=0), bit 15 = jobb alsó (r=3, c=3)
static const uint16_t T_SHAPES[7][4] = {
  { 0x0F00, 0x2222, 0x0F00, 0x2222 }, // I
  { 0x0071, 0x0223, 0x0047, 0x0322 }, // J
  { 0x0074, 0x0622, 0x0017, 0x0113 }, // L
  { 0x0066, 0x0066, 0x0066, 0x0066 }, // O
  { 0x0036, 0x0231, 0x0036, 0x0231 }, // S
  { 0x0072, 0x0232, 0x0027, 0x0262 }, // T
  { 0x0063, 0x0132, 0x0063, 0x0132 }  // Z
};

struct TetrisDiff { uint16_t baseSpeed; };
static const TetrisDiff T_DIFF[DIFF_COUNT] = { 
  {700}, // Easy: Lassú
  {450}, // Medium: Normál
  {250}  // Hard: Gyors
};

// --- Állapotváltozók ---
static uint16_t t_grid[20];            // 10x20-as pálya (10 bit / sor)
static uint8_t  t_state;               // 0: Ready, 1: Play, 2: Game Over
static uint8_t  t_shape, t_rot, t_nextShape;
static int8_t   t_x, t_y;
static uint16_t t_lines;
static uint32_t t_lastDrop;
static uint32_t t_dasStart, t_lastMove; // Gombnyomva-tartás időzítők
static uint32_t t_deadAt;
static bool     t_newBest;
static uint32_t t_bestAtStart;

// --- Hangok ---
static const Note T_SFX_MOVE[]  = { {800, 10} };
static const Note T_SFX_ROT[]   = { {1000, 15} };
static const Note T_SFX_LOCK[]  = { {400, 30} };
static const Note T_SFX_LINE[]  = { {800, 40}, {1200, 60}, {1600, 80} };
static const Note T_SFX_DEAD[]  = { {300, 100}, {200, 150}, {100, 250} };
static const Note T_SFX_HARD[]  = { {1500, 20} };

// ---------------- Segédfüggvények ----------------
// Ütközésvizsgálat
inline bool tHit(uint8_t shape, uint8_t rot, int8_t tx, int8_t ty) {
  uint16_t mask = T_SHAPES[shape][rot];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((mask >> (r * 4 + c)) & 1) {
        int8_t nx = tx + c;
        int8_t ny = ty + r;
        if (nx < 0 || nx >= 10 || ny >= 20) return true; // Falak és padló
        if (ny >= 0 && ((t_grid[ny] >> nx) & 1)) return true; // Lerakott blokkok
      }
    }
  }
  return false;
}

inline void tGameOver() {
  t_state = 2; // Game Over
  t_deadAt = millis();
  t_newBest = (g_hud.score > t_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(T_SFX_DEAD);
}

inline void tSpawn() {
  t_shape = t_nextShape;
  t_nextShape = random(7);
  g_hud.nextShape = T_SHAPES[t_nextShape][0]; // A UI automatikusan kirajzolja
  t_rot = 0;
  t_x = 3;
  t_y = -2;
  
  if (tHit(t_shape, t_rot, t_x, t_y)) tGameOver();
}

inline void tetrisReset() {
  for (uint8_t i = 0; i < 20; i++) t_grid[i] = 0;
  t_state = 0;
  t_lines = 0;
  g_hud.score = 0;
  g_hud.showNext = true;
  g_hud.tag = "LINES";
  g_hud.tagVal = 0;
  t_nextShape = random(7);
  t_newBest = false;
  t_bestAtStart = g_highScore[g_curGame][g_difficulty];
  tSpawn();
}

inline void tetrisInit() { tetrisReset(); }

inline void tetrisUpdate() {
  if (t_state == 2) {
    if (btnPressed(BTN_CENTER) && millis() - t_deadAt > 600) tetrisReset();
    return;
  }
  
  if (t_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN)) {
      t_state = 1;
      t_lastDrop = millis();
    }
    return;
  }

  // --- MOZGATÁS (DAS logikával a folyékony csúszásért) ---
  if (btnPressed(BTN_LEFT)) {
    if (!tHit(t_shape, t_rot, t_x - 1, t_y)) { t_x--; audioPlay(T_SFX_MOVE); }
    t_dasStart = millis(); 
  } else if (btnPressed(BTN_RIGHT)) {
    if (!tHit(t_shape, t_rot, t_x + 1, t_y)) { t_x++; audioPlay(T_SFX_MOVE); }
    t_dasStart = millis(); 
  } else if (btnDown(BTN_LEFT)) {
    if (millis() - t_dasStart > 180 && millis() - t_lastMove > 50) {
      if (!tHit(t_shape, t_rot, t_x - 1, t_y)) { t_x--; audioPlay(T_SFX_MOVE); }
      t_lastMove = millis();
    }
  } else if (btnDown(BTN_RIGHT)) {
    if (millis() - t_dasStart > 180 && millis() - t_lastMove > 50) {
      if (!tHit(t_shape, t_rot, t_x + 1, t_y)) { t_x++; audioPlay(T_SFX_MOVE); }
      t_lastMove = millis();
    }
  }

  // --- FORGATÁS ---
  if (btnPressed(BTN_UP)) {
    uint8_t nr = (t_rot + 1) % 4;
    static const int8_t KICKS[5] = { 0, -1, 1, -2, 2 };     // fal mellett is elforduljon (wall kick)
    for (uint8_t k = 0; k < 5; k++) {
      if (!tHit(t_shape, nr, t_x + KICKS[k], t_y)) {
        t_x += KICKS[k];
        t_rot = nr;
        audioPlay(T_SFX_ROT);
        break;
      }
    }
  }

  // --- HARD DROP (Azonnali leejtés) ---
  if (btnPressed(BTN_CENTER)) {
    uint8_t dropDist = 0;
    while (!tHit(t_shape, t_rot, t_x, t_y + 1)) {
      t_y++;
      dropDist++;
    }
    if (dropDist > 0) {
      g_hud.score += dropDist * 2; // Bónusz pont a Hard Dropért
      audioPlay(T_SFX_HARD);
    }
    t_lastDrop = 0; // Kényszerített lerakás a következő ciklusban
  }

  // --- GRAVITÁCIÓ (Soft Drop gyorsítással) ---
  // 5 soronként gyorsul (régen soronként -12 ms: Hard-on 17 sornál már 50 ms/sor volt)
  int speed = (int)T_DIFF[g_difficulty].baseSpeed - (t_lines / 5) * 35;
  if (speed < 90) speed = 90;
  if (btnDown(BTN_DOWN)) speed = 40; // Soft Drop

  if (millis() - t_lastDrop > speed) {
    t_lastDrop = millis();
    
    if (tHit(t_shape, t_rot, t_x, t_y + 1)) {
      // Elem lerakása a rácsra
      uint16_t mask = T_SHAPES[t_shape][t_rot];
      bool toppedOut = false;
      for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
          if ((mask >> (r * 4 + c)) & 1) {
            int ny = t_y + r;
            int nx = t_x + c;
            if (ny < 0) toppedOut = true;                  // a pálya fölött rögzült -> vége
            else if (ny < 20 && nx >= 0 && nx < 10) {
              t_grid[ny] |= (1 << nx);
            }
          }
        }
      }
      if (toppedOut) { tGameOver(); return; }

      // Vonalak törlése
      uint8_t cleared = 0;
      for (int r = 0; r < 20; r++) {
        if (t_grid[r] == 0x03FF) { // Minden 10 bit be van állítva
          for (int k = r; k > 0; k--) t_grid[k] = t_grid[k - 1]; // Csúsztatás lefelé
          t_grid[0] = 0;
          cleared++;
        }
      }

      if (cleared > 0) {
        t_lines += cleared;
        g_hud.tagVal = t_lines;
        uint16_t pts = (cleared == 1) ? 100 : (cleared == 2) ? 300 : (cleared == 3) ? 500 : 800;
        g_hud.score += pts * (g_difficulty + 1);
        audioPlay(T_SFX_LINE);
      } else {
        audioPlay(T_SFX_LOCK);
      }

      tSpawn(); // Új elem
      
    } else {
      t_y++;
      if (btnDown(BTN_DOWN)) g_hud.score += 1; // Soft drop bónusz
    }
  }
}

inline void tetrisDraw() {
  g_hud.over = (t_state == 2);
  // Pálya határok (64x64-es arénában középre igazítva)
  gColor(1);
  gVLine(15, 0, 64); // Bal fal
  gVLine(48, 0, 64); // Jobb fal

  // Grid (Rögzített elemek)
  for (int r = 0; r < 20; r++) {
    for (int c = 0; c < 10; c++) {
      if ((t_grid[r] >> c) & 1) {
        gBox(17 + c * 3, 2 + r * 3, 2, 2); // 3x3-as lépésközökkel 2x2 pixeles négyzetek
      }
    }
  }

  gHLine(15, 62, 34);                         // padló

  // Szellem-elem (landolási hely) - Hard-on nincs
  if (t_state == 1 && g_difficulty != DIFF_HARD) {
    int8_t gy = t_y;
    while (!tHit(t_shape, t_rot, t_x, gy + 1)) gy++;
    if (gy > t_y) {
      uint16_t gm = T_SHAPES[t_shape][t_rot];
      for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
          if (((gm >> (r * 4 + c)) & 1) && gy + r >= 0) {
            gPixel(17 + (t_x + c) * 3,     2 + (gy + r) * 3);
            gPixel(17 + (t_x + c) * 3 + 1, 2 + (gy + r) * 3 + 1);
          }
    }
  }

  // Aktuális mozgó elem
  if (t_state != 2) {
    uint16_t mask = T_SHAPES[t_shape][t_rot];
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        if ((mask >> (r * 4 + c)) & 1) {
          int ny = t_y + r;
          if (ny >= 0) {
            gBox(17 + (t_x + c) * 3, 2 + ny * 3, 2, 2);
          }
        }
      }
    }
  }
  
  // Game Over / Ready UI (A négyzetes arénához igazítva)
  if (t_state == 0) {
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* msg = "Press UP";
    int16_t w = u8g2.getStrWidth(msg);
    int16_t tx = (64 - w) / 2;
    gColor(0); gBox(tx - 3, 27, w + 6, 12);
    gColor(1); gFrame(tx - 3, 27, w + 6, 12);
    gText(tx, 36, msg);
  } 
  else if (t_state == 2) {
    gColor(0); gBox(4, 18, 56, 30);
    gColor(1); gFrame(4, 18, 56, 30);
    u8g2.setFont(u8g2_font_4x6_tr);
    gText(13, 27, "GAME OVER");
    const char* sub = (t_newBest && ((millis() / 400) & 1)) ? "NEW BEST!" : "C: retry";
    gText(11, 35, sub);
    gText(11, 43, "L: menu");
  }
}