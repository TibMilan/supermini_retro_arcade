#pragma once
// ============================================================
//  input.h - gombok: pergesmentesites, gomb-utkozes szures
// ============================================================
#include "config.h"

constexpr uint8_t  CHORD_WINDOW_MS        = 40;    
constexpr uint8_t  CHORD_WINDOW_STRICT_MS = 80;    
constexpr uint16_t CENTER_GUARD_MS        = 200;   

static const uint8_t BTN_PINS[BTN_COUNT] = {
  PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_CENTER
};

static bool     in_stable[BTN_COUNT];
static bool     in_lastRaw[BTN_COUNT];
static uint32_t in_lastChange[BTN_COUNT];
static uint32_t in_pressAt[BTN_COUNT];
static bool     in_blocked[BTN_COUNT];                  

static uint8_t  in_winMask = 0;                         
static uint32_t in_winStart = 0;
static uint32_t in_lastDirAt = 0;
static bool     in_strict = false;

static uint8_t  in_pendPress = 0, in_pendRelease = 0;   
static uint8_t  in_snapPress = 0, in_snapRelease = 0;   
static uint8_t  in_downMask  = 0;                       

static uint32_t in_centerDownAt = 0;
static bool     in_centerLongFired = false;
static bool     in_pendLong = false, in_snapLong = false;
static uint32_t in_lastActivity = 0;                    

inline void inputSetStrict(bool s) { in_strict = s; }

inline void inputBegin() {
  in_lastActivity = millis();
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    in_stable[i] = false;
    in_lastRaw[i] = false;
    in_lastChange[i] = 0;
    in_pressAt[i] = 0;
    in_blocked[i] = false;
  }
}

inline void inputAccept(uint8_t i, uint32_t now) {
  in_pendPress |= (1 << i);
  in_downMask  |= (1 << i);
  if (i == BTN_CENTER) { in_centerDownAt = now; in_centerLongFired = false; }
  else                 { in_lastDirAt = now; }
}

inline void inputFinalize(uint32_t now) {
  uint8_t m = in_winMask;
  in_winMask = 0;
  if (!m) return;

  int8_t pick = -1;
  for (uint8_t i = 0; i < BTN_CENTER; i++) {             
    if (m & (1 << i)) {
      if (pick < 0 || in_pressAt[i] < in_pressAt[pick]) pick = i;
    }
  }
  if (pick < 0) {                                         
    bool tooSoon = in_strict && in_lastDirAt != 0 && (now - in_lastDirAt) < CENTER_GUARD_MS;
    if (!tooSoon) pick = BTN_CENTER;
  }
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    if (!(m & (1 << i))) continue;
    if (i == pick) inputAccept(i, now);
    else           in_blocked[i] = true;
  }
}

inline void inputPoll() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    bool raw = (digitalRead(BTN_PINS[i]) == LOW);
    if (raw != in_lastRaw[i]) {
      in_lastRaw[i] = raw;
      in_lastChange[i] = now;
      in_lastActivity = now;
    } else if (raw != in_stable[i] && (now - in_lastChange[i]) >= (raw ? PRESS_DEBOUNCE_MS : DEBOUNCE_MS)) {
      in_stable[i] = raw;
      if (raw) {                                          
        in_pressAt[i] = now;
        if (!in_winMask) in_winStart = now;
        in_winMask |= (1 << i);
      } else {                                            
        if (in_winMask & (1 << i)) inputFinalize(now);    
        if (in_blocked[i]) {
          in_blocked[i] = false;
        } else if ((in_downMask >> i) & 1) {
          in_pendRelease |= (1 << i);
          in_downMask    &= ~(1 << i);
        }
      }
    }
  }

  if (in_winMask && (now - in_winStart) >=
      (in_strict ? CHORD_WINDOW_STRICT_MS : CHORD_WINDOW_MS))
    inputFinalize(now);

  if (((in_downMask >> BTN_CENTER) & 1) && !in_centerLongFired &&
      (now - in_centerDownAt) >= LONG_PRESS_MS) {
    in_centerLongFired = true;
    in_pendLong = true;
  }
}

inline void inputLatch() {
  in_snapPress = in_pendPress;     in_pendPress = 0;
  in_snapRelease = in_pendRelease; in_pendRelease = 0;
  in_snapLong = in_pendLong;       in_pendLong = false;
}

inline void inputFlush() {
  in_pendPress = in_pendRelease = in_snapPress = in_snapRelease = 0;
  in_pendLong = in_snapLong = false;
}

inline bool btnPressed(Button b)  { return (in_snapPress   >> b) & 1; }
inline bool btnReleased(Button b) { return (in_snapRelease >> b) & 1; }
inline bool btnDown(Button b)     { return (in_downMask    >> b) & 1; }
inline bool centerLongPressed()   { return in_snapLong; }

inline uint32_t inputIdleMs()     { return millis() - in_lastActivity; }

inline bool inputAnyRawDown() {
  for (uint8_t i = 0; i < BTN_COUNT; i++)
    if (digitalRead(BTN_PINS[i]) == LOW) return true;
  return false;
}