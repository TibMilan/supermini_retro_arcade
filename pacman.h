#pragma once
// ============================================================
//  pacman.h - Micro Pac-Man
//  Layout: WIDE (128x52 arena) = 32 x 13 tiles of 4x4 px
//
//  Controls:  Arrows = steer (the next turn is buffered, reversing is instant)
//             Center = start / restart
//  Rules:     eat all dots, pellets (big dots) make the ghosts vulnerable.
//             3 lives, extra life every 5000 points. Tunnel on the middle row.
//  Levels:    Easy = 2 ghosts, Medium = 3, Hard = 4 (faster, shorter fright).
//             Every cleared maze makes the ghosts a bit faster.
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

constexpr uint8_t PM_C = 32;
constexpr uint8_t PM_R = 13;

// Left half of the maze (mirrored at build time). # wall, - ghost door,
// . dot, o pellet, space = empty path / ghost house
static const char* const PM_HALF[PM_R] = {
  "################",
  "#o..............",
  "#.##.#####.#####",
  "#.##.#####.#####",
  "#...............",
  "#.##.##.#.##.##-",
  ".............#  ",
  "#.##.##.#.##.###",
  "#...............",
  "#.##.#####.#####",
  "#.##.#####.#####",
  "#o..............",
  "################",
};

enum : uint8_t { PM_EMPTY = 0, PM_DOT = 1, PM_PELLET = 2, PM_WALL = 3, PM_DOOR = 4 };
enum : uint8_t { PM_G_HOUSE = 0, PM_G_OUT = 1, PM_G_EYES = 2 };
enum : uint8_t { PM_READY = 0, PM_PLAY = 1, PM_DYING = 2, PM_CLEAR = 3, PM_OVER = 4 };

struct PmDiff {
  float    ghostSpd;     // px / frame
  float    frightSpd;
  uint16_t frightMs;
  uint16_t scatterMs;
  uint16_t chaseMs;
  uint8_t  ghosts;
};
static const PmDiff PM_DIFF[DIFF_COUNT] = {
  {0.70f, 0.50f, 8000, 7000, 12000, 2},   // Easy
  {0.82f, 0.55f, 6000, 6000, 16000, 3},   // Medium
  {0.92f, 0.60f, 4000, 4000, 22000, 4},   // Hard
};
constexpr float PM_PAC_SPD  = 1.0f;
constexpr float PM_EYES_SPD = 1.6f;

struct PmGhost {
  int16_t  x, y;
  int8_t   dx, dy;
  float    acc;
  uint8_t  st;
  bool     fright;
  uint32_t t;          // house release time
};

static uint8_t  pm_g[PM_R][PM_C];
static uint16_t pm_dots;
static int16_t  pm_x, pm_y;
static int8_t   pm_dx, pm_dy, pm_wx, pm_wy;
static float    pm_acc;
static uint16_t pm_anim;
static PmGhost  pm_gh[4];
static uint8_t  pm_state;
static uint32_t pm_stateAt, pm_modeAt, pm_frightEnd;
static bool     pm_scatter;
static uint8_t  pm_modeIdx;
static uint8_t  pm_lives, pm_level, pm_chain;
static uint32_t pm_nextLife;
static bool     pm_newBest;
static uint32_t pm_bestAtStart;
static int16_t  pm_popX, pm_popY;      // floating score popup
static uint16_t pm_popVal;
static uint32_t pm_popAt;

static const int8_t PM_DX[4] = { 0, -1, 0, 1 };   // up, left, down, right (classic tie-break order)
static const int8_t PM_DY[4] = { -1, 0, 1, 0 };
static const uint16_t PM_REL_MS[4] = { 0, 1500, 4500, 8000 };

static const Note PM_SFX_PELLET[] = { {600, 30}, {900, 40}, {1200, 40} };
static const Note PM_SFX_GHOST[]  = { {800, 40}, {1200, 40}, {1600, 60} };
static const Note PM_SFX_DEATH[]  = { {600, 90}, {500, 90}, {400, 90}, {300, 90}, {200, 220} };
static const Note PM_SFX_CLEAR[]  = { {1047, 80}, {1319, 80}, {1568, 80}, {2093, 200} };
static const Note PM_SFX_LIFE[]   = { {1319, 60}, {1760, 60}, {2349, 120} };

// ---------------- maze helpers ----------------
inline bool pmWall(int16_t c, int16_t r) {
  if (r < 0 || r >= PM_R) return true;
  c = ((c % PM_C) + PM_C) % PM_C;
  return pm_g[r][c] >= PM_WALL;
}
inline bool pmWallish(int16_t c, int16_t r) {      // for drawing: outside = wall (no wrap)
  if (r < 0 || r >= PM_R || c < 0 || c >= PM_C) return true;
  return pm_g[r][c] >= PM_WALL;
}

inline void pmBuild() {
  pm_dots = 0;
  for (uint8_t r = 0; r < PM_R; r++) {
    for (uint8_t c = 0; c < 16; c++) {
      char ch = PM_HALF[r][c];
      uint8_t v = PM_EMPTY;
      if (ch == '#') v = PM_WALL;
      else if (ch == '-') v = PM_DOOR;
      else if (ch == '.') v = PM_DOT;
      else if (ch == 'o') v = PM_PELLET;
      pm_g[r][c] = v;
      pm_g[r][PM_C - 1 - c] = v;
      if (v == PM_DOT || v == PM_PELLET) pm_dots += 2;
    }
  }
  pm_g[8][15] = PM_EMPTY; pm_dots--;      // Pac-Man start tile: no dot
}

inline float pmGhostSpeed() {
  float s = PM_DIFF[g_difficulty].ghostSpd + 0.02f * (pm_level - 1);
  return s > 0.98f ? 0.98f : s;
}

inline void pmResetRound() {
  pm_x = 15 * 4; pm_y = 8 * 4;
  pm_dx = 0; pm_dy = 0; pm_wx = 0; pm_wy = 0;
  pm_acc = 0; pm_anim = 0;
  pm_chain = 0;
  pm_popAt = 0;
  pm_scatter = true; pm_modeIdx = 0;
  pm_frightEnd = 0;
  for (uint8_t i = 0; i < 4; i++) {
    PmGhost& g = pm_gh[i];
    g.acc = 0; g.fright = false; g.dx = 0; g.dy = 0;
    if (i == 0) {                           // Blinky starts outside
      g.st = PM_G_OUT; g.x = 16 * 4; g.y = 4 * 4; g.dx = -1;
    } else {
      g.st = PM_G_HOUSE; g.x = (14 + i) * 4; g.y = 6 * 4;
    }
    g.t = 0;
  }
  pm_state = PM_READY;
  pm_stateAt = millis();
}

inline void pmSetTag() { g_hud.lives = pm_lives; }

inline void pmStartLevel() {
  pmBuild();
  pmResetRound();
}

inline void pmReset() {
  pm_lives = 3; pm_level = 1; pm_nextLife = 5000;
  g_hud.score = 0; g_hud.showNext = false;
  pm_newBest = false;
  pm_bestAtStart = g_highScore[g_curGame][g_difficulty];
  pmSetTag();
  pmStartLevel();
}

inline void pacmanInit() { pmReset(); }

// ---------------- Pac-Man ----------------
inline void pmEatAt(int16_t c, int16_t r) {
  uint8_t& cell = pm_g[r][c];
  if (cell == PM_DOT) {
    cell = PM_EMPTY; pm_dots--; g_hud.score += 10;
    if (!audioBusy()) audioTone(((pm_anim >> 3) & 1) ? 330 : 440, 35);
  } else if (cell == PM_PELLET) {
    cell = PM_EMPTY; pm_dots--; g_hud.score += 50;
    audioPlay(PM_SFX_PELLET);
    int32_t ms = (int32_t)PM_DIFF[g_difficulty].frightMs - 300 * (pm_level - 1);
    if (ms < 1500) ms = 1500;
    pm_frightEnd = millis() + ms;
    pm_chain = 0;
    for (uint8_t i = 0; i < 4; i++) {
      PmGhost& g = pm_gh[i];
      if (g.st == PM_G_OUT) { g.fright = true; g.dx = -g.dx; g.dy = -g.dy; }
    }
  }
  if (g_hud.score >= pm_nextLife) {
    pm_nextLife += 5000;
    if (pm_lives < 5) { pm_lives++; pmSetTag(); audioPlay(PM_SFX_LIFE); }
  }
}

inline void pmPacStep() {
  bool aligned = ((pm_x & 3) == 0) && ((pm_y & 3) == 0);
  if (aligned) {
    int16_t c = pm_x >> 2, r = pm_y >> 2;
    pmEatAt(c, r);
    if ((pm_wx || pm_wy) && !pmWall(c + pm_wx, r + pm_wy)) { pm_dx = pm_wx; pm_dy = pm_wy; }
    if (pmWall(c + pm_dx, r + pm_dy)) { pm_dx = 0; pm_dy = 0; return; }   // blocked: wait for a turn
  }
  if (!pm_dx && !pm_dy) return;
  pm_x += pm_dx; pm_y += pm_dy;
  pm_anim++;
  if (pm_x <= -4) pm_x += 128;           // tunnel wrap
  if (pm_x >= 128) pm_x -= 128;
}

inline void pmReadInput() {
  int8_t nx = 0, ny = 0;
  if (btnPressed(BTN_UP))         { nx = 0;  ny = -1; }
  else if (btnPressed(BTN_DOWN))  { nx = 0;  ny = 1; }
  else if (btnPressed(BTN_LEFT))  { nx = -1; ny = 0; }
  else if (btnPressed(BTN_RIGHT)) { nx = 1;  ny = 0; }
  else if (!(btnDown(BTN_UP) && pm_wy == -1) && !(btnDown(BTN_DOWN) && pm_wy == 1) &&
           !(btnDown(BTN_LEFT) && pm_wx == -1) && !(btnDown(BTN_RIGHT) && pm_wx == 1)) {
    // wanted direction is no longer held: follow whichever key is held
    if (btnDown(BTN_UP)) ny = -1;
    else if (btnDown(BTN_DOWN)) ny = 1;
    else if (btnDown(BTN_LEFT)) nx = -1;
    else if (btnDown(BTN_RIGHT)) nx = 1;
  }
  if (nx || ny) {
    pm_wx = nx; pm_wy = ny;
    // instant reversal on the same axis (valid anywhere between tiles)
    if (nx == -pm_dx && ny == -pm_dy && (pm_dx || pm_dy)) { pm_dx = nx; pm_dy = ny; }
  }
}

// ---------------- Ghosts ----------------
inline void pmGhostTarget(uint8_t i, int16_t& tx, int16_t& ty) {
  int16_t pc = pm_x >> 2, pr = pm_y >> 2;
  if (pm_gh[i].st == PM_G_EYES) { tx = ((pm_gh[i].x >> 2) <= 15) ? 15 : 16; ty = 4; return; }
  if (pm_scatter) {
    static const int16_t CX[4] = { 30, 1, 30, 1 }, CY[4] = { 1, 1, 11, 11 };
    tx = CX[i]; ty = CY[i]; return;
  }
  switch (i) {
    case 0: tx = pc; ty = pr; break;
    case 1: tx = pc + 4 * pm_dx; ty = pr + 4 * pm_dy; break;
    case 2: {
      int16_t bx = pm_gh[0].x >> 2, by = pm_gh[0].y >> 2;
      tx = 2 * (pc + 2 * pm_dx) - bx; ty = 2 * (pr + 2 * pm_dy) - by; break;
    }
    default: {
      int16_t gc = pm_gh[i].x >> 2, gr = pm_gh[i].y >> 2;
      int16_t dx = gc - pc, dy = gr - pr;
      if (dx * dx + dy * dy > 64) { tx = pc; ty = pr; } else { tx = 1; ty = 11; }
    }
  }
}

inline void pmGhostDecide(uint8_t i) {
  PmGhost& g = pm_gh[i];
  int16_t c = g.x >> 2, r = g.y >> 2;
  int8_t cand[4]; uint8_t n = 0;
  for (uint8_t d = 0; d < 4; d++) {
    if (PM_DX[d] == -g.dx && PM_DY[d] == -g.dy && (g.dx || g.dy)) continue;   // no U-turns
    if (pmWall(c + PM_DX[d], r + PM_DY[d])) continue;
    cand[n++] = d;
  }
  if (!n) { g.dx = -g.dx; g.dy = -g.dy; return; }
  uint8_t pick = cand[0];
  if (g.fright && g.st == PM_G_OUT) {
    pick = cand[random(n)];
  } else {
    int16_t tx, ty; pmGhostTarget(i, tx, ty);
    int32_t best = 0x7FFFFFFF;
    for (uint8_t k = 0; k < n; k++) {
      int16_t nc = c + PM_DX[cand[k]], nr = r + PM_DY[cand[k]];
      int32_t d2 = (int32_t)(nc - tx) * (nc - tx) + (int32_t)(nr - ty) * (nr - ty);
      if (d2 < best) { best = d2; pick = cand[k]; }
    }
  }
  g.dx = PM_DX[pick]; g.dy = PM_DY[pick];
}

inline void pmGhostStep(uint8_t i) {
  PmGhost& g = pm_gh[i];
  if (((g.x & 3) == 0) && ((g.y & 3) == 0)) {
    int16_t c = g.x >> 2, r = g.y >> 2;
    if (g.st == PM_G_EYES && r == 4 && (c == 15 || c == 16)) {   // back home
      g.st = PM_G_HOUSE; g.fright = false;
      g.x = (14 + (i ? i : 1)) * 4; g.y = 6 * 4; g.dx = g.dy = 0;
      g.t = millis() + 1200;
      return;
    }
    pmGhostDecide(i);
  }
  g.x += g.dx; g.y += g.dy;
  if (g.x <= -4) g.x += 128;
  if (g.x >= 128) g.x -= 128;
}

inline void pmEndRound(bool dead) {
  if (dead) {
    pm_state = PM_DYING; pm_stateAt = millis();
    audioPlay(PM_SFX_DEATH);
    if (pm_lives) pm_lives--;
    pmSetTag();
  }
}

inline void pmGameOver() {
  pm_state = PM_OVER; pm_stateAt = millis();
  pm_newBest = (g_hud.score > pm_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
}

inline void pmStartPlay() {
  pm_state = PM_PLAY;
  uint32_t now = millis();
  pm_modeAt = now;
  for (uint8_t i = 1; i < 4; i++)
    if (pm_gh[i].st == PM_G_HOUSE) pm_gh[i].t = now + PM_REL_MS[i];
}

inline void pacmanUpdate() {
  uint32_t now = millis();
  const PmDiff& D = PM_DIFF[g_difficulty];

  switch (pm_state) {
    case PM_OVER:
      if (btnPressed(BTN_CENTER) && now - pm_stateAt > 600) pmReset();
      return;

    case PM_READY: {
      bool go = btnPressed(BTN_UP) || btnPressed(BTN_DOWN) || btnPressed(BTN_LEFT) ||
                btnPressed(BTN_RIGHT) || btnPressed(BTN_CENTER);
      if (!go && now - pm_stateAt > 700 &&
          (btnDown(BTN_UP) || btnDown(BTN_DOWN) || btnDown(BTN_LEFT) || btnDown(BTN_RIGHT))) go = true;
      if (go) {
        pmReadInput();
        pmStartPlay();
      }
      return;
    }

    case PM_DYING:
      if (now - pm_stateAt > 1400) {
        if (pm_lives == 0) pmGameOver();
        else pmResetRound();
      }
      return;

    case PM_CLEAR:
      if (now - pm_stateAt > 1600) {
        pm_level++;
        pmStartLevel();
      }
      return;
  }

  // ---- PLAY ----
  pmReadInput();

  // scatter / chase schedule
  uint32_t dur = pm_scatter ? D.scatterMs : D.chaseMs;
  if (now - pm_modeAt >= dur) {
    pm_scatter = !pm_scatter;
    pm_modeAt = now;
    for (uint8_t i = 0; i < D.ghosts; i++) {
      PmGhost& g = pm_gh[i];
      if (g.st == PM_G_OUT && !g.fright) { g.dx = -g.dx; g.dy = -g.dy; }
    }
  }

  bool frightOn = (int32_t)(pm_frightEnd - now) > 0;
  if (!frightOn) {
    for (uint8_t i = 0; i < 4; i++) pm_gh[i].fright = false;
  }

  // Pac-Man
  pm_acc += PM_PAC_SPD;
  while (pm_acc >= 1.0f) { pm_acc -= 1.0f; pmPacStep(); }

  // Ghosts
  for (uint8_t i = 0; i < D.ghosts; i++) {
    PmGhost& g = pm_gh[i];
    if (g.st == PM_G_HOUSE) {
      if ((int32_t)(now - g.t) >= 0 && g.t != 0) {       // release
        g.st = PM_G_OUT;
        g.x = ((i & 1) ? 16 : 15) * 4; g.y = 4 * 4;
        g.dx = (i & 1) ? 1 : -1; g.dy = 0; g.acc = 0;
      }
      continue;
    }
    float spd = (g.st == PM_G_EYES) ? PM_EYES_SPD : (g.fright ? D.frightSpd : pmGhostSpeed());
    g.acc += spd;
    while (g.acc >= 1.0f) {
      g.acc -= 1.0f;
      pmGhostStep(i);
      if (g.st == PM_G_HOUSE) break;
    }
  }

  // Collisions
  for (uint8_t i = 0; i < D.ghosts; i++) {
    PmGhost& g = pm_gh[i];
    if (g.st != PM_G_OUT) continue;
    int16_t dx = g.x - pm_x, dy = g.y - pm_y;
    if (dx > 64) dx -= 128;                 // across the tunnel seam
    if (dx < -64) dx += 128;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx < 3 && dy < 3) {
      if (g.fright) {
        g.st = PM_G_EYES; g.fright = false;
        if (pm_chain < 4) pm_chain++;
        uint16_t pts = 100u << pm_chain;    // 200, 400, 800, 1600
        g_hud.score += pts;
        pm_popX = g.x; pm_popY = g.y; pm_popVal = pts; pm_popAt = now;
        audioPlay(PM_SFX_GHOST);
      } else {
        pmEndRound(true);
        return;
      }
    }
  }

  if (pm_dots == 0) {
    pm_state = PM_CLEAR; pm_stateAt = now;
    g_hud.score += 100 * pm_level;
    audioPlay(PM_SFX_CLEAR);
  }
}

// ---------------- Drawing ----------------
inline void pmSprite2(int16_t x, int16_t y, void (*fn)(int16_t, int16_t, uint8_t), uint8_t a) {
  fn(x, y, a);
  if (x < 0) fn(x + 128, y, a);
  else if (x > 124) fn(x - 128, y, a);
}

inline void pmDrawPac(int16_t x, int16_t y, uint8_t mode) {
  // mode: 0 closed, 1..4 open facing up/left/down/right
  gBox(x + 1, y, 2, 4);
  gBox(x, y + 1, 4, 2);
  if (mode) {
    gColor(0);
    if (mode == 4)      { gBox(x + 2, y + 1, 2, 2); }
    else if (mode == 2) { gBox(x, y + 1, 2, 2); }
    else if (mode == 1) { gBox(x + 1, y, 2, 2); }
    else                { gBox(x + 1, y + 2, 2, 2); }
    gColor(1);
  }
}

// a: bit0 = skirt frame, bit1 = frightened hollow, bit2 = eyes only
inline void pmDrawGhost(int16_t x, int16_t y, uint8_t a) {
  if (a & 4) {                              // eyes
    gPixel(x, y + 1); gPixel(x + 3, y + 1);
    gPixel(x, y + 2); gPixel(x + 3, y + 2);
    return;
  }
  gBox(x + 1, y, 2, 1);
  gBox(x, y + 1, 4, 2);
  if (a & 1) { gPixel(x + 1, y + 3); gPixel(x + 3, y + 3); }
  else       { gPixel(x, y + 3);     gPixel(x + 2, y + 3); }
  gColor(0);
  if (a & 2) { gPixel(x + 1, y + 1); gPixel(x + 2, y + 1); }   // scared face
  else       { gPixel(x, y + 1); gPixel(x + 2, y + 1); }       // eyes
  gColor(1);
}

inline void pmDrawMaze(bool flash) {
  bool pelletOn = ((millis() / 220) & 1);
  for (uint8_t r = 0; r < PM_R; r++) {
    for (uint8_t c = 0; c < PM_C; c++) {
      uint8_t v = pm_g[r][c];
      int16_t x = c * 4, y = r * 4;
      if (v == PM_DOT) {
        gPixel(x + 1, y + 1);
      } else if (v == PM_PELLET) {
        if (pelletOn) gBox(x, y, 3, 3);
        else          gBox(x + 1, y + 1, 1, 1);
      } else if (v == PM_DOOR) {
        gHLine(x, y + 1, 4);
      } else if (v == PM_WALL && !flash) {
        gPixel(x + 1, y + 1);
        // right neighbour: draw the link only if it lies on the outline
        if (c + 1 < PM_C && pm_g[r][c + 1] >= PM_WALL) {
          bool edge = !(pmWallish(c, r - 1) && pmWallish(c + 1, r - 1)) ||
                      !(pmWallish(c, r + 1) && pmWallish(c + 1, r + 1));
          if (edge) gHLine(x + 1, y + 1, 5);
        }
        if (r + 1 < PM_R && pm_g[r + 1][c] >= PM_WALL) {
          bool edge = !(pmWallish(c - 1, r) && pmWallish(c - 1, r + 1)) ||
                      !(pmWallish(c + 1, r) && pmWallish(c + 1, r + 1));
          if (edge) gVLine(x + 1, y + 1, 5);
        }
      }
    }
  }
}

inline void pacmanDraw() {
  g_hud.over = (pm_state == PM_OVER);
  uint32_t now = millis();
  bool flash = (pm_state == PM_CLEAR) && ((now / 200) & 1);
  if (pm_state == PM_CLEAR && flash) {
    // blink the whole maze off every other 200 ms (walls flash)
    gColor(1);
    pmDrawMaze(true);
    return;
  }
  gColor(1);
  pmDrawMaze(false);

  if (pm_state == PM_CLEAR) {
    // maze visible, nothing else
  } else {
    // ghosts
    bool frightOn = (int32_t)(pm_frightEnd - now) > 0;
    bool blink = frightOn && (int32_t)(pm_frightEnd - now) < 1500 && ((now / 180) & 1);
    if (pm_state != PM_DYING) {
      for (uint8_t i = 0; i < PM_DIFF[g_difficulty].ghosts; i++) {
        PmGhost& g = pm_gh[i];
        uint8_t a = (g.st == PM_G_EYES) ? 4 : 0;
        if (g.st == PM_G_OUT && g.fright) a = blink ? 0 : 2;
        a |= (now / 160) & 1;
        int16_t gy = g.y;
        if (g.st == PM_G_HOUSE) gy += (((now / 220) + i) & 1) ? 1 : -1;
        pmSprite2(g.x, gy, pmDrawGhost, a);
      }
    }

    // Pac-Man
    if (pm_state == PM_DYING) {
      uint32_t t = now - pm_stateAt;
      if (t < 1200) {
        uint8_t h = 4 - (t * 4 / 1200);          // shrinks 4 -> 1 px high
        if (h < 1) h = 1;
        int16_t hh = h;
        gBox(pm_x, pm_y + (4 - hh), 4, hh);
      }
    } else {
      uint8_t mode = 0;
      if (pm_dx || pm_dy) {
        bool open = (pm_anim >> 1) & 1;
        if (open) mode = (pm_dy < 0) ? 1 : (pm_dx < 0) ? 2 : (pm_dy > 0) ? 3 : 4;
      } else if (pm_state == PM_READY) {
        mode = 0;
      }
      pmSprite2(pm_x, pm_y, pmDrawPac, mode);
    }
  }

  // ghost score popup
  if (pm_popAt && now - pm_popAt < 700) {
    char b[6]; snprintf(b, sizeof b, "%u", pm_popVal);
    u8g2.setFont(u8g2_font_4x6_tr);
    int16_t px = pm_popX - 4; if (px < 0) px = 0; if (px > 128 - 16) px = 128 - 16;
    gColor(0); gBox(px - 1, pm_popY - 1, 18, 8);
    gColor(1); gText(px, pm_popY + 5, b);
  }

  // messages
  u8g2.setFont(u8g2_font_4x6_tr);
  if (pm_state == PM_READY) {
    const char* m = (now / 400) & 1 ? "READY!" : "";
    if (m[0]) {
      int16_t w = u8g2.getStrWidth(m);
      gColor(0); gBox((128 - w) / 2 - 2, 29, w + 4, 8);
      gColor(1); gText((128 - w) / 2, 35, m);
    }
  } else if (pm_state == PM_OVER) {
    drawGameOverPanel("GAME OVER", pm_newBest);
  }
}
