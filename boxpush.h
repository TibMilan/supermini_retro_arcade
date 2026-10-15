#pragma once
// ============================================================
//  boxpush.h - Box Push (Sokoban-style, endless generated puzzles, time attack)
//  Layout: WIDE (128x52 arena), 6x6 px tiles
//
//  Controls:  Arrows = walk / push a box (hold = auto-repeat)
//             Center short = undo one move
//             Center hold ~0.6 s (beep) + release = restart this puzzle
//  Goal: push every box onto a goal (dots). Boxes can only be pushed, not pulled.
//  Every puzzle is generated on the spot by playing a solution BACKWARDS
//  (pulling boxes away from the goals), so every puzzle is solvable.
//  Time attack: the clock runs, each solved puzzle adds +15 s.
//  Score per puzzle = (100 + 100*boxes + 3*seconds saved under 60 s) * (difficulty+1).
//  Difficulty: room size, number of internal walls, number of boxes
//  (Easy 2, Medium/Hard 3 at the start, growing to 5 as you solve puzzles).
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"
#include <string.h>

constexpr uint8_t  BP_MAXW   = 14;       // incl. border walls
constexpr uint8_t  BP_MAXH   = 8;
constexpr uint8_t  BP_MAXBOX = 5;
constexpr uint8_t  BP_TILE   = 6;
constexpr uint16_t BP_HIST   = 300;
constexpr uint16_t BP_RESTART_HOLD = 600;
constexpr uint16_t BP_BONUS_MS = 15000;

struct BpDiff { uint8_t iw, ih; uint8_t walls; uint8_t boxes; uint8_t grow1, grow2; uint16_t timeS; };
//                         room   walls boxes grow  grow  time
static const BpDiff BP_DIFF[DIFF_COUNT] = {
  {  8, 5,  4, 2,  5, 10, 240 },   // Easy
  { 10, 6,  7, 3,  4,  8, 180 },   // Medium
  { 12, 6, 10, 3,  3,  6, 150 },   // Hard
};

static uint8_t  bp_w, bp_h;
static uint8_t  bp_wall[BP_MAXH][BP_MAXW];
static uint8_t  bp_n;
static uint8_t  bp_gx[BP_MAXBOX], bp_gy[BP_MAXBOX];     // goals
static uint8_t  bp_bx[BP_MAXBOX], bp_by[BP_MAXBOX];     // boxes now
static uint8_t  bp_sx[BP_MAXBOX], bp_sy[BP_MAXBOX];     // boxes at puzzle start
static uint8_t  bp_px, bp_py, bp_spx, bp_spy;           // player now / at start
static uint8_t  bp_hist[BP_HIST];
static uint16_t bp_histN;
static uint16_t bp_moves;
static uint8_t  bp_state;                 // 0 ready, 1 play, 2 solved flash, 3 time up
static int32_t  bp_timeMs;
static uint32_t bp_puzzleMs;
static uint16_t bp_solved;
static uint32_t bp_stateAt;
static uint32_t bp_lastGain;
static bool     bp_newBest;
static uint32_t bp_bestAtStart;
static uint8_t  bp_repDir;                // 0 none, 1 up, 2 down, 3 left, 4 right
static uint32_t bp_repAt;
static uint32_t bp_cDownMs;
static bool     bp_cHeld, bp_cArmed;

static const int8_t BP_DX[4] = { 0, 0, -1, 1 };          // up, down, left, right
static const int8_t BP_DY[4] = { -1, 1, 0, 0 };

static const Note BP_SFX_STEP[]  = { {700, 6} };
static const Note BP_SFX_PUSH[]  = { {350, 20} };
static const Note BP_SFX_UNDO[]  = { {500, 15}, {400, 15} };
static const Note BP_SFX_SOLVE[] = { {1047, 70}, {1319, 70}, {1568, 70}, {2093, 160} };
static const Note BP_SFX_OVER[]  = { {440, 100}, {330, 100}, {220, 300} };
static const Note BP_SFX_NEW[]   = { {900, 20} };

inline bool bpWallAt(int8_t x, int8_t y) {
  return x < 0 || y < 0 || x >= bp_w || y >= bp_h || bp_wall[y][x];
}
inline int8_t bpBoxAt(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < bp_n; i++) if (bp_bx[i] == x && bp_by[i] == y) return i;
  return -1;
}
inline bool bpGoalAt(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < bp_n; i++) if (bp_gx[i] == x && bp_gy[i] == y) return true;
  return false;
}
inline bool bpAllOnGoals() {
  for (uint8_t i = 0; i < bp_n; i++) if (!bpGoalAt(bp_bx[i], bp_by[i])) return false;
  return true;
}

// All floor cells reachable from one floor cell? (used while adding internal walls)
inline bool bpConnected() {
  uint8_t q[BP_MAXW * BP_MAXH][2]; uint8_t seen[BP_MAXH][BP_MAXW];
  memset(seen, 0, sizeof seen);
  uint16_t total = 0; int8_t sx = -1, sy = -1;
  for (uint8_t y = 0; y < bp_h; y++)
    for (uint8_t x = 0; x < bp_w; x++)
      if (!bp_wall[y][x]) { total++; if (sx < 0) { sx = x; sy = y; } }
  if (sx < 0) return false;
  uint16_t head = 0, tail = 0, cnt = 1;
  q[tail][0] = sx; q[tail][1] = sy; tail++; seen[sy][sx] = 1;
  while (head < tail) {
    uint8_t x = q[head][0], y = q[head][1]; head++;
    for (uint8_t d = 0; d < 4; d++) {
      int8_t nx = x + BP_DX[d], ny = y + BP_DY[d];
      if (bpWallAt(nx, ny) || seen[ny][nx]) continue;
      seen[ny][nx] = 1; cnt++;
      q[tail][0] = nx; q[tail][1] = ny; tail++;
    }
  }
  return cnt == total;
}

inline uint8_t bpBoxCount() {
  const BpDiff& D = BP_DIFF[g_difficulty];
  uint8_t n = D.boxes + (bp_solved >= D.grow1) + (bp_solved >= D.grow2);
  return n > BP_MAXBOX ? BP_MAXBOX : n;
}

// Builds one solvable puzzle by pulling boxes away from their goals.
inline void bpGenerate() {
  const BpDiff& D = BP_DIFF[g_difficulty];
  bp_n = bpBoxCount();
  for (uint8_t attempt = 0; attempt < 40; attempt++) {
    bp_w = D.iw + 2; bp_h = D.ih + 2;
    for (uint8_t y = 0; y < BP_MAXH; y++)
      for (uint8_t x = 0; x < BP_MAXW; x++)
        bp_wall[y][x] = (x == 0 || y == 0 || x == bp_w - 1 || y == bp_h - 1 || x >= bp_w || y >= bp_h) ? 1 : 0;

    // internal walls (never disconnect the room)
    uint8_t want = D.walls + (bp_n > D.boxes ? 1 : 0), placed = 0;
    for (uint8_t t = 0; t < 60 && placed < want; t++) {
      uint8_t x = 1 + random(D.iw), y = 1 + random(D.ih);
      if (bp_wall[y][x]) continue;
      bp_wall[y][x] = 1;
      if (bpConnected()) placed++; else bp_wall[y][x] = 0;
    }

    // goals = boxes at the start of the reverse play
    uint8_t cx[BP_MAXW * BP_MAXH], cy[BP_MAXW * BP_MAXH]; uint8_t nc = 0;
    for (uint8_t y = 1; y < bp_h - 1; y++)
      for (uint8_t x = 1; x < bp_w - 1; x++)
        if (!bp_wall[y][x]) { cx[nc] = x; cy[nc] = y; nc++; }
    if (nc < bp_n + 3) continue;
    for (uint8_t i = 0; i < bp_n; i++) {
      uint8_t k = random(nc - i) + i;
      uint8_t tx = cx[k], ty = cy[k]; cx[k] = cx[i]; cy[k] = cy[i]; cx[i] = tx; cy[i] = ty;
      bp_gx[i] = bp_bx[i] = tx; bp_gy[i] = bp_by[i] = ty;
    }
    uint8_t pk = bp_n + random(nc - bp_n);
    bp_px = cx[pk]; bp_py = cy[pk];

    // reverse play: random walk, pulling a box when one is right behind the player
    uint16_t target = bp_n * (18 + 8 * g_difficulty), pulls = 0;
    for (uint16_t step = 0; step < 4000 && pulls < target; step++) {
      uint8_t d = random(4);
      int8_t nx = bp_px + BP_DX[d], ny = bp_py + BP_DY[d];
      if (bpWallAt(nx, ny) || bpBoxAt(nx, ny) >= 0) continue;
      int8_t bi = bpBoxAt(bp_px - BP_DX[d], bp_py - BP_DY[d]);
      if (bi >= 0 && random(100) < 70) { bp_bx[bi] = bp_px; bp_by[bi] = bp_py; pulls++; }
      bp_px = nx; bp_py = ny;
    }

    // quality: boxes must really be away from the goals
    uint16_t disp = 0; uint8_t off = 0;
    for (uint8_t i = 0; i < bp_n; i++) {
      disp += abs((int)bp_bx[i] - bp_gx[i]) + abs((int)bp_by[i] - bp_gy[i]);
      if (!bpGoalAt(bp_bx[i], bp_by[i])) off++;
    }
    if (off >= (bp_n > 2 ? bp_n - 1 : bp_n) && disp >= (uint16_t)bp_n * (2 + g_difficulty)) break;
  }
  for (uint8_t i = 0; i < bp_n; i++) { bp_sx[i] = bp_bx[i]; bp_sy[i] = bp_by[i]; }
  bp_spx = bp_px; bp_spy = bp_py;
  bp_histN = 0; bp_moves = 0; bp_puzzleMs = 0;
  bp_repDir = 0; bp_cHeld = false; bp_cArmed = false;
}

inline void bpSetTag() {
  g_hud.tag = "T:";
  g_hud.tagVal = (int16_t)((bp_timeMs + 999) / 1000);
}

inline void bpReset() {
  bp_solved = 0;
  bp_state = 0; bp_stateAt = millis();
  bp_timeMs = (int32_t)BP_DIFF[g_difficulty].timeS * 1000;
  g_hud.score = 0; g_hud.showNext = false;
  bp_newBest = false;
  bp_bestAtStart = g_highScore[g_curGame][g_difficulty];
  bpGenerate();
  bpSetTag();
}

inline void boxpushInit() { bpReset(); }

inline void bpRestartPuzzle() {
  for (uint8_t i = 0; i < bp_n; i++) { bp_bx[i] = bp_sx[i]; bp_by[i] = bp_sy[i]; }
  bp_px = bp_spx; bp_py = bp_spy;
  bp_histN = 0; bp_moves = 0;
  audioPlay(BP_SFX_NEW);
}

inline void bpUndo() {
  if (!bp_histN) return;
  uint8_t h = bp_hist[--bp_histN];
  uint8_t d = h & 3; bool pushed = (h >> 2) & 1;
  if (pushed) {
    int8_t bi = bpBoxAt(bp_px + BP_DX[d], bp_py + BP_DY[d]);
    if (bi >= 0) { bp_bx[bi] = bp_px; bp_by[bi] = bp_py; }
  }
  bp_px -= BP_DX[d]; bp_py -= BP_DY[d];
  if (bp_moves) bp_moves--;
  audioPlay(BP_SFX_UNDO);
}

inline void bpTimeUp() {
  bp_state = 3; bp_stateAt = millis();
  bp_timeMs = 0; bpSetTag();
  bp_newBest = (g_hud.score > bp_bestAtStart);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  audioPlay(BP_SFX_OVER);
}

inline void bpMove(uint8_t d) {
  int8_t nx = bp_px + BP_DX[d], ny = bp_py + BP_DY[d];
  if (bpWallAt(nx, ny)) return;
  bool pushed = false;
  int8_t bi = bpBoxAt(nx, ny);
  if (bi >= 0) {
    int8_t bx = nx + BP_DX[d], by = ny + BP_DY[d];
    if (bpWallAt(bx, by) || bpBoxAt(bx, by) >= 0) return;
    bp_bx[bi] = bx; bp_by[bi] = by;
    pushed = true;
  }
  bp_px = nx; bp_py = ny;
  if (bp_histN >= BP_HIST) { memmove(bp_hist, bp_hist + 1, BP_HIST - 1); bp_histN = BP_HIST - 1; }
  bp_hist[bp_histN++] = d | (pushed ? 4 : 0);
  bp_moves++;
  audioPlay(pushed ? BP_SFX_PUSH : BP_SFX_STEP);

  if (pushed && bpAllOnGoals()) {
    uint32_t secs = bp_puzzleMs / 1000;
    uint32_t bonus = secs < 60 ? (60 - secs) * 3 : 0;
    bp_lastGain = (100UL + 100UL * bp_n + bonus) * (g_difficulty + 1);
    g_hud.score += bp_lastGain;
    bp_solved++;
    bp_timeMs += BP_BONUS_MS;
    bp_state = 2; bp_stateAt = millis();
    audioPlay(BP_SFX_SOLVE);
    bpSetTag();
  }
}

inline void boxpushUpdate() {
  uint32_t now = millis();

  if (bp_state == 3) {
    if (btnPressed(BTN_CENTER) && now - bp_stateAt > 600) bpReset();
    return;
  }
  if (bp_state == 0) {
    if (btnPressed(BTN_CENTER) || btnPressed(BTN_UP) || btnPressed(BTN_DOWN) ||
        btnPressed(BTN_LEFT) || btnPressed(BTN_RIGHT)) { bp_state = 1; audioPlay(BP_SFX_NEW); }
    return;
  }
  if (bp_state == 2) {                                       // short celebration, then a new puzzle
    bp_timeMs -= FRAME_MS;
    if (now - bp_stateAt > 900) { bpGenerate(); bp_state = 1; audioPlay(BP_SFX_NEW); }
    bpSetTag();
    return;
  }

  // ---- play ----
  bp_timeMs -= FRAME_MS;
  bp_puzzleMs += FRAME_MS;
  bpSetTag();
  if (bp_timeMs <= 0) { bpTimeUp(); return; }

  // Center: short = undo, long = restart puzzle
  if (btnPressed(BTN_CENTER)) { bp_cDownMs = now; bp_cHeld = true; bp_cArmed = false; }
  if (bp_cHeld && btnDown(BTN_CENTER) && !bp_cArmed) {
    uint32_t held = now - bp_cDownMs;
    if (held >= BP_RESTART_HOLD && held < 1500) { bp_cArmed = true; audioTone(1500, 25); }
  }
  if (bp_cHeld && btnReleased(BTN_CENTER)) {
    uint32_t held = now - bp_cDownMs;
    bp_cHeld = false;
    if (bp_cArmed)          bpRestartPuzzle();
    else if (held < BP_RESTART_HOLD) bpUndo();
    bp_cArmed = false;
  }

  // movement with auto-repeat
  uint8_t pressed = 0;
  if (btnPressed(BTN_UP))         pressed = 1;
  else if (btnPressed(BTN_DOWN))  pressed = 2;
  else if (btnPressed(BTN_LEFT))  pressed = 3;
  else if (btnPressed(BTN_RIGHT)) pressed = 4;
  if (pressed) {
    bp_repDir = pressed; bp_repAt = now + 230;
    bpMove(pressed - 1);
  } else if (bp_repDir) {
    bool still = (bp_repDir == 1 && btnDown(BTN_UP))   || (bp_repDir == 2 && btnDown(BTN_DOWN)) ||
                 (bp_repDir == 3 && btnDown(BTN_LEFT)) || (bp_repDir == 4 && btnDown(BTN_RIGHT));
    if (!still) bp_repDir = 0;
    else if ((int32_t)(now - bp_repAt) >= 0) { bp_repAt = now + 120; bpMove(bp_repDir - 1); }
  }
}

inline void boxpushDraw() {
  g_hud.over = (bp_state == 3);
  uint32_t now = millis();
  int16_t ox = (128 - bp_w * BP_TILE) / 2, oy = (52 - bp_h * BP_TILE) / 2;
  bool flash = (bp_state == 2) && ((now / 120) & 1);

  gColor(1);
  for (uint8_t y = 0; y < bp_h; y++) {
    for (uint8_t x = 0; x < bp_w; x++) {
      int16_t px = ox + x * BP_TILE, py = oy + y * BP_TILE;
      if (bp_wall[y][x]) {
        for (uint8_t yy = 0; yy < BP_TILE; yy++)
          for (uint8_t xx = (yy & 1); xx < BP_TILE; xx += 2) gPixel(px + xx, py + yy);
      } else if (bpGoalAt(x, y)) {
        gBox(px + 2, py + 2, 2, 2);
      }
    }
  }
  for (uint8_t i = 0; i < bp_n; i++) {
    int16_t px = ox + bp_bx[i] * BP_TILE, py = oy + bp_by[i] * BP_TILE;
    if (bpGoalAt(bp_bx[i], bp_by[i]) || flash) gBox(px, py, 6, 6);
    else { gFrame(px, py, 6, 6); gBox(px + 2, py + 2, 2, 2); }
  }
  {
    int16_t px = ox + bp_px * BP_TILE, py = oy + bp_py * BP_TILE;
    gBox(px + 1, py + 1, 4, 4);
    gColor(0); gPixel(px + 2, py + 2); gPixel(px + 3, py + 2); gColor(1);
  }

  // side info: puzzle number, boxes done, moves
  u8g2.setFont(u8g2_font_4x6_tr);
  char b[12];
  uint8_t done = 0;
  for (uint8_t i = 0; i < bp_n; i++) if (bpGoalAt(bp_bx[i], bp_by[i])) done++;
  snprintf(b, sizeof b, "#%u", bp_solved + 1);   gText(1, 7, b);
  snprintf(b, sizeof b, "%u/%u", done, bp_n);    gText(1, 15, b);
  snprintf(b, sizeof b, "%um", bp_moves);        gText(127 - u8g2.getStrWidth(b), 7, b);
  if (bp_state == 1 && bp_cArmed) gText(127 - u8g2.getStrWidth("RESET"), 15, "RESET");
  if (bp_state == 2) {
    snprintf(b, sizeof b, "+%lu", (unsigned long)bp_lastGain);
    gText(1, 23, b);
  }

  if (bp_state == 0 && ((now / 450) & 1)) {
    const char* m = "Press CENTER";
    int16_t w = u8g2.getStrWidth(m);
    gColor(0); gBox((128 - w) / 2 - 2, 21, w + 4, 9);
    gColor(1); gFrame((128 - w) / 2 - 2, 21, w + 4, 9); gText((128 - w) / 2, 28, m);
  }
  if (bp_state == 3) {
    gColor(0); gBox(22, 6, 84, 42);
    gColor(1); gFrame(22, 6, 84, 42);
    u8g2.setFont(u8g2_font_6x10_tr);
    const char* t = "TIME UP!";
    gText((128 - u8g2.getStrWidth(t)) / 2, 18, t);
    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(b, sizeof b, "Solved: %u", bp_solved);
    const char* a = (bp_newBest && ((now / 400) & 1)) ? "NEW BEST!" : b;
    gText((128 - u8g2.getStrWidth(a)) / 2, 28, a);
    const char* c = "Center: again";
    gText((128 - u8g2.getStrWidth(c)) / 2, 36, c);
    gText((128 - u8g2.getStrWidth("Left: menu")) / 2, 44, "Left: menu");
  }
}
