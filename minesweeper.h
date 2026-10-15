#pragma once
// ============================================================
//  minesweeper.h - Mine Sweeper
//  Layout: WIDE (128x52 arena)
//
//  Controls:
//    Arrows : move cursor (hold = auto-repeat, wraps around the edges)
//    Center : short press (<300 ms)  = reveal
//             hold ~300 ms (beep + cursor stops blinking) and release = flag / unflag
//             (hold 2 s = quit pop-up, handled by the framework)
//  Status bar shows  M:<mines left>  (mines - flags placed).
// ============================================================
#include "config.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "storage.h"

constexpr uint8_t M_COLS = 15;
constexpr uint8_t M_ROWS = 6;
constexpr int16_t M_OFF_X = 4;
constexpr int16_t M_OFF_Y = 2;

constexpr uint16_t M_FLAG_HOLD_MS = 300;    // from here a hold means "flag"
constexpr uint16_t M_IGNORE_MS    = 1500;   // longer = quit gesture, do nothing
constexpr uint16_t M_REPEAT_FIRST = 260;
constexpr uint16_t M_REPEAT_NEXT  = 90;
constexpr uint16_t M_PANEL_MS     = 1500;   // result panel, then only a blinking banner

struct MinesDiff { uint8_t mines; };
static const MinesDiff M_DIFF[DIFF_COUNT] = { {10}, {15}, {20} }; // Easy, Medium, Hard

// Cell bits (low 4 bits: neighbouring mines 0-8)
#define M_MINE 0x10
#define M_REV  0x20
#define M_FLAG 0x40

static uint8_t  m_grid[M_COLS][M_ROWS];
static uint8_t  m_cx, m_cy;
static bool     m_firstClick;
static uint8_t  m_state;            // 0: Play, 1: Win, 2: Dead
static uint32_t m_startTime, m_endAt;
static uint8_t  m_revCount;
static uint8_t  m_flags;
static uint32_t m_btnDownTime;
static bool     m_btnHeld;          // center went down during play
static bool     m_armed;            // flag hold reached
static bool     m_newBest;
static uint8_t  m_repDir;           // 0 none, 1 L, 2 R, 3 U, 4 D
static uint32_t m_repAt;

// Icons (8x8)
static const uint8_t M_FLAG_XBM[8] = { 0x08, 0x18, 0x38, 0x78, 0x38, 0x08, 0x08, 0x1C };
static const uint8_t M_BOMB_XBM[8] = { 0x08, 0x24, 0x3C, 0x7E, 0x7E, 0x3C, 0x24, 0x08 };

// Must be static: audioPlay keeps the pointer while the tune is playing
static const Note M_WIN_TUNE[]  = { {1047, 100}, {1319, 100}, {1568, 100}, {2093, 200} };
static const Note M_LOSE_TUNE[] = { {300, 120}, {200, 160}, {120, 260} };

inline void msUpdateTag() {
  g_hud.tag = "M:";
  g_hud.tagVal = (int16_t)M_DIFF[g_difficulty].mines - (int16_t)m_flags;
}

inline void msReset() {
  for (uint8_t c = 0; c < M_COLS; c++)
    for (uint8_t r = 0; r < M_ROWS; r++)
      m_grid[c][r] = 0;
  m_cx = M_COLS / 2; m_cy = M_ROWS / 2;
  m_firstClick = true;
  m_state = 0;
  m_revCount = 0;
  m_flags = 0;
  m_startTime = 0;
  m_newBest = false;
  m_btnHeld = false;
  m_armed = false;
  m_repDir = 0;
  g_hud.score = 0;
  g_hud.showNext = false;
  msUpdateTag();
}

inline void msInit() { msReset(); }

inline void msPlaceMines(uint8_t safeX, uint8_t safeY) {
  uint8_t minesToPlace = M_DIFF[g_difficulty].mines;
  while (minesToPlace > 0) {
    uint8_t x = random(M_COLS);
    uint8_t y = random(M_ROWS);
    // 3x3 safe zone around the first reveal so it always opens an area
    if (abs((int)x - (int)safeX) <= 1 && abs((int)y - (int)safeY) <= 1) continue;
    if (!(m_grid[x][y] & M_MINE)) {
      m_grid[x][y] |= M_MINE;
      minesToPlace--;
    }
  }

  for (uint8_t c = 0; c < M_COLS; c++) {
    for (uint8_t r = 0; r < M_ROWS; r++) {
      if (m_grid[c][r] & M_MINE) continue;
      uint8_t count = 0;
      for (int8_t dx = -1; dx <= 1; dx++) {
        for (int8_t dy = -1; dy <= 1; dy++) {
          int8_t nx = c + dx, ny = r + dy;
          if (nx >= 0 && nx < M_COLS && ny >= 0 && ny < M_ROWS)
            if (m_grid[nx][ny] & M_MINE) count++;
        }
      }
      m_grid[c][r] |= count;
    }
  }
}

// Recursive flood fill (max depth = 90 cells, tiny frames)
inline void msReveal(uint8_t cx, uint8_t cy) {
  if (cx >= M_COLS || cy >= M_ROWS) return;
  if (m_grid[cx][cy] & (M_REV | M_FLAG)) return;

  m_grid[cx][cy] |= M_REV;
  m_revCount++;
  g_hud.score = m_revCount * 10;

  if ((m_grid[cx][cy] & 0x0F) == 0) {
    for (int8_t dx = -1; dx <= 1; dx++)
      for (int8_t dy = -1; dy <= 1; dy++) {
        if (dx == 0 && dy == 0) continue;
        msReveal(cx + dx, cy + dy);   // negative wraps to 255 -> rejected above
      }
  }
}

inline void msMove(uint8_t dir) {
  if (dir == 1) m_cx = (m_cx + M_COLS - 1) % M_COLS;
  if (dir == 2) m_cx = (m_cx + 1) % M_COLS;
  if (dir == 3) m_cy = (m_cy + M_ROWS - 1) % M_ROWS;
  if (dir == 4) m_cy = (m_cy + 1) % M_ROWS;
  audioTone(800, 8);
}

inline void msFinish(uint8_t state) {
  m_state = state;
  m_endAt = millis();
  m_btnHeld = false;
  uint32_t best = g_highScore[g_curGame][g_difficulty];
  m_newBest = (g_hud.score > best);
  storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
}

inline void msDoReveal() {
  if (m_grid[m_cx][m_cy] & (M_REV | M_FLAG)) return;

  if (m_firstClick) {
    msPlaceMines(m_cx, m_cy);
    m_firstClick = false;
    m_startTime = millis();
  }

  if (m_grid[m_cx][m_cy] & M_MINE) {
    for (uint8_t c = 0; c < M_COLS; c++)
      for (uint8_t r = 0; r < M_ROWS; r++)
        if (m_grid[c][r] & M_MINE) m_grid[c][r] |= M_REV;
    msFinish(2);
    audioPlay(M_LOSE_TUNE);
    return;
  }

  msReveal(m_cx, m_cy);
  audioTone(1000, 15);

  if (m_revCount == (M_COLS * M_ROWS) - M_DIFF[g_difficulty].mines) {
    uint32_t secs = (millis() - m_startTime) / 1000;
    uint32_t bonus = (secs < 300) ? (300 - secs) * 3 : 0;
    g_hud.score += bonus + 500;
    msFinish(1);
    audioPlay(M_WIN_TUNE);
  }
}

inline void msUpdate() {
  if (m_state > 0) {
    if (btnPressed(BTN_CENTER) && millis() - m_endAt > 600) msReset();
    return;
  }

  uint32_t now = millis();

  // Cursor with auto-repeat (one direction at a time)
  uint8_t pressed = 0;
  if (btnPressed(BTN_LEFT))  pressed = 1;
  else if (btnPressed(BTN_RIGHT)) pressed = 2;
  else if (btnPressed(BTN_UP))    pressed = 3;
  else if (btnPressed(BTN_DOWN))  pressed = 4;

  if (pressed) {
    m_repDir = pressed;
    m_repAt = now + M_REPEAT_FIRST;
    msMove(pressed);
  } else if (m_repDir) {
    bool still = (m_repDir == 1 && btnDown(BTN_LEFT))  || (m_repDir == 2 && btnDown(BTN_RIGHT)) ||
                 (m_repDir == 3 && btnDown(BTN_UP))    || (m_repDir == 4 && btnDown(BTN_DOWN));
    if (!still) m_repDir = 0;
    else if ((int32_t)(now - m_repAt) >= 0) {
      m_repAt = now + M_REPEAT_NEXT;
      msMove(m_repDir);
    }
  }

  // Center: decided on release; flag mode is signalled while holding
  if (btnPressed(BTN_CENTER)) {
    m_btnDownTime = now;
    m_btnHeld = true;
    m_armed = false;
  }
  if (m_btnHeld && btnDown(BTN_CENTER) && !m_armed) {
    uint32_t held = now - m_btnDownTime;
    if (held >= M_FLAG_HOLD_MS && held < M_IGNORE_MS) {
      m_armed = true;
      audioTone(1500, 25);                         // "flag mode" feedback
    }
  }
  if (btnReleased(BTN_CENTER) && m_btnHeld) {
    uint32_t held = now - m_btnDownTime;
    m_btnHeld = false;
    bool wasArmed = m_armed;
    m_armed = false;
    if (held > M_IGNORE_MS) return;
    if (wasArmed || held >= M_FLAG_HOLD_MS) {
      uint8_t& cell = m_grid[m_cx][m_cy];
      if (!(cell & M_REV)) {
        cell ^= M_FLAG;
        if (cell & M_FLAG) m_flags++; else m_flags--;
        msUpdateTag();
        audioTone(1500, 25);
      }
    } else {
      msDoReveal();
    }
  }
}

inline void msDraw() {
  g_hud.over = (m_state > 0);
  for (uint8_t c = 0; c < M_COLS; c++) {
    for (uint8_t r = 0; r < M_ROWS; r++) {
      int16_t x = M_OFF_X + c * 8;
      int16_t y = M_OFF_Y + r * 8;
      uint8_t cell = m_grid[c][r];
      gColor(1);
      gFrame(x, y, 8, 8);
      if (!(cell & M_REV)) {
        if (cell & M_FLAG) u8g2.drawXBM(g_ox + x, g_oy + y, 8, 8, M_FLAG_XBM);
        else               gBox(x + 2, y + 2, 4, 4);
      } else if (cell & M_MINE) {
        u8g2.drawXBM(g_ox + x, g_oy + y, 8, 8, M_BOMB_XBM);
      } else {
        uint8_t n = cell & 0x0F;
        if (n > 0) {
          u8g2.setFont(u8g2_font_4x6_tr);
          char b[2]; b[0] = '0' + n; b[1] = 0;
          gText(x + 2, y + 6, b);
        }
      }
    }
  }

  // Cursor: blinking inverted square; steady while flag mode is armed
  if (m_state == 0 && (m_armed || ((millis() / 250) & 1))) {
    gColor(2);
    gBox(M_OFF_X + m_cx * 8, M_OFF_Y + m_cy * 8, 8, 8);
    gColor(1);
  }

  if (m_state > 0) {
    uint32_t since = millis() - m_endAt;
    const char* title = (m_state == 1) ? "YOU WIN!" : "GAME OVER";
    if (since < M_PANEL_MS) {
      drawGameOverPanel(title, m_newBest);
    } else if (((since - M_PANEL_MS) % 1600) < 1000) {
      // Board stays readable: a small blinking banner instead of the big panel
      gColor(0); gBox(30, 21, 68, 11);
      gColor(1); gFrame(30, 21, 68, 11);
      u8g2.setFont(u8g2_font_4x6_tr);
      const char* msg = "C: again  L: menu";
      gText((128 - u8g2.getStrWidth(msg)) / 2, 28, msg);
    }
  }
}
