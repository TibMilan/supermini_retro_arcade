#pragma once
// ============================================================
//  ui.h - grafikai motor: 2 layout (A/B) statuszsavval, arena-segedek,
//         ikonok, kilepesi pop-up
//
//  A jatek MINDIG arena-koordinatakkal rajzol (0,0 = arena bal felso),
//  az eltolast es a vagast az uiSetLayout() altal beallitott layout intezi.
//    A) SQUARE: arena 64x64, offset (32,0)  - oldalsavok
//    B) WIDE  : arena 128x52, offset (0,12) - felso statuszsav
// ============================================================
#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"
#include "storage.h"
#include "input.h"
#include "battery.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

// Amit a layout sávjaiban megjelenik
struct HudInfo {
  uint32_t score = 0;
  uint32_t hiScore = 0;
  uint8_t  lives = 0;
  bool     showNext = false;     
  uint16_t nextShape = 0;        
  const char* tag = nullptr;     
  int16_t  tagVal = 0;
  bool     over = false;         
};
static HudInfo g_hud;

static Layout  g_layout = LAYOUT_WIDE;
static int16_t g_ox = 0, g_oy = 12, g_aw = 128, g_ah = 52;

inline int16_t uiArenaW() { return g_aw; }
inline int16_t uiArenaH() { return g_ah; }

inline void uiSetLayout(Layout l) {
  g_layout = l;
  if (l == LAYOUT_SQUARE) { g_ox = 32; g_oy = 0;  g_aw = 64;  g_ah = 64; }
  else                    { g_ox = 0;  g_oy = 12; g_aw = 128; g_ah = 52; }
}

inline void uiApplyBrightness() { u8g2.setContrast(BRI_VALUES[g_brightness]); }

inline void uiBegin() {
  u8g2.setBusClock(I2C_CLOCK_HZ);
  u8g2.begin();
  u8g2.setPowerSave(0);
  u8g2.setContrast(BRI_VALUES[g_brightness]);
}

// ---------------- Arena-rajzolo segedek (vagott, eltolt) ----------------
inline void gColor(uint8_t c) { u8g2.setDrawColor(c); }

inline void gBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  int16_t x1 = x + w, y1 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > g_aw) x1 = g_aw;
  if (y1 > g_ah) y1 = g_ah;
  if (x1 <= x || y1 <= y) return;
  u8g2.drawBox(g_ox + x, g_oy + y, x1 - x, y1 - y);
}
inline void gPixel(int16_t x, int16_t y)            { gBox(x, y, 1, 1); }
inline void gHLine(int16_t x, int16_t y, int16_t l) { gBox(x, y, l, 1); }
inline void gVLine(int16_t x, int16_t y, int16_t l) { gBox(x, y, 1, l); }
inline void gFrame(int16_t x, int16_t y, int16_t w, int16_t h) {
  gHLine(x, y, w); gHLine(x, y + h - 1, w);
  gVLine(x, y, h); gVLine(x + w - 1, y, h);
}
inline void gText(int16_t x, int16_t y, const char* s) { u8g2.drawStr(g_ox + x, g_oy + y, s); }

// ---------------- Ikonok ----------------
static const uint8_t HEART_XBM[5] = { 0x0A, 0x1F, 0x1F, 0x0E, 0x04 }; 

inline void drawBatteryIcon(int16_t x, int16_t y, uint8_t pct) {       
  u8g2.drawFrame(x, y, 11, 6);
  u8g2.drawBox(x + 11, y + 2, 1, 2);
  uint8_t w = (uint16_t)pct * 9 / 100;
  if (w) u8g2.drawBox(x + 1, y + 1, w, 4);
}

inline void drawSpeakerIcon(int16_t x, int16_t y, uint8_t vol) {       
  u8g2.drawBox(x, y + 2, 2, 3);
  u8g2.drawVLine(x + 2, y + 1, 5);
  u8g2.drawVLine(x + 3, y, 7);
  if (vol == 0) {
    u8g2.drawLine(x + 5, y + 1, x + 8, y + 5);
    u8g2.drawLine(x + 5, y + 5, x + 8, y + 1);
  } else {
    u8g2.drawVLine(x + 5, y + 2, 3);
    if (vol >= 5) u8g2.drawVLine(x + 7, y + 1, 5);
  }
}

inline void drawNum(int16_t x, int16_t y, uint32_t v) {
  char b[12];
  if (v > 999999UL) v = 999999UL;
  snprintf(b, sizeof b, "%lu", (unsigned long)v);
  u8g2.drawStr(x, y, b);
}

// ---------------- Kozos statuszsav: bal = hangero, kozep = tartalom, jobb = akku ----------------
constexpr int16_t BAR_MID_X = 13;
constexpr int16_t BAR_MID_W = 82;

inline void drawStatusLeft() { drawSpeakerIcon(2, 2, g_volume); }

inline void drawStatusRight() {
  drawBatteryIcon(96, 3, g_batPercent);
  u8g2.setFont(u8g2_font_4x6_tr);
  char b[6];
  snprintf(b, sizeof b, "%u%%", g_batPercent);
  u8g2.drawStr(111, 8, b);
}

inline void drawStatusBar() {
  drawStatusLeft();
  drawStatusRight();
  u8g2.drawHLine(0, 11, 128);
}

// ---------------- A) Negyzetes kozponti sablon ----------------
inline void drawChromeSquare() {
  u8g2.drawVLine(31, 0, 64);
  u8g2.drawVLine(96, 0, 64);
  u8g2.setFont(u8g2_font_4x6_tr);

  // Bal sav
  u8g2.drawStr(2, 7, "SCORE");
  drawNum(2, 15, g_hud.score);
  if (g_hud.tag) {                                   
    u8g2.drawStr(2, 27, g_hud.tag);
    drawNum(2, 35, g_hud.tagVal < 0 ? 0 : (uint32_t)g_hud.tagVal);
  } else {
    u8g2.drawStr(2, 27, "LIVES");
    uint8_t lives = g_hud.lives > 8 ? 8 : g_hud.lives;
    for (uint8_t i = 0; i < lives; i++)
      u8g2.drawXBM(3 + (i % 4) * 7, 31 + (i / 4) * 7, 5, 5, HEART_XBM);
  }

  // Jobb sav
  u8g2.drawStr(99, 7, "HI");
  drawNum(99, 15, g_hud.hiScore);
  drawBatteryIcon(99, 21, g_batPercent);
  char b[6];
  snprintf(b, sizeof b, "%u%%", g_batPercent);
  u8g2.drawStr(113, 27, b);
  drawSpeakerIcon(99, 33, g_volume);
  if (g_hud.showNext) {
    u8g2.drawStr(99, 49, "NEXT");
    for (uint8_t r = 0; r < 4; r++)
      for (uint8_t c = 0; c < 4; c++)
        if ((g_hud.nextShape >> (r * 4 + c)) & 1)
          u8g2.drawBox(99 + c * 3, 51 + r * 3, 3, 3);
  }
}

// ---------------- B) Horizontalis szeles sablon ----------------
inline void drawChromeWide() {
  u8g2.setFont(u8g2_font_4x6_tr);
  char b[40];
  uint32_t s = g_hud.score > 999999UL ? 999999UL : g_hud.score;
  uint32_t h = g_hud.hiScore > 999999UL ? 999999UL : g_hud.hiScore;
  uint8_t lv = g_hud.lives > 5 ? 5 : g_hud.lives;                 
  int16_t hw = lv ? lv * 6 - 1 : 0, gap = lv ? 4 : 0;
  const char* tg = g_hud.tag ? g_hud.tag : "";
  if (g_hud.tag) snprintf(b, sizeof b, "S:%lu H:%lu %s%d", (unsigned long)s, (unsigned long)h, tg, (int)g_hud.tagVal);
  else           snprintf(b, sizeof b, "S:%lu H:%lu", (unsigned long)s, (unsigned long)h);
  if (u8g2.getStrWidth(b) + gap + hw > BAR_MID_W)                  
    snprintf(b, sizeof b, "S:%lu", (unsigned long)s);
  int16_t w = u8g2.getStrWidth(b);
  int16_t x0 = BAR_MID_X + (BAR_MID_W - (w + gap + hw)) / 2;
  u8g2.drawStr(x0, 8, b);
  for (uint8_t i = 0; i < lv; i++) u8g2.drawXBM(x0 + w + gap + i * 6, 3, 5, 5, HEART_XBM);
  drawStatusBar();
}

inline void drawGameOverPanel(const char* title, bool newBest, const char* line1 = "Center: again") {
  gColor(0); gBox(22, 8, 84, 36);
  gColor(1); gFrame(22, 8, 84, 36);
  u8g2.setFont(u8g2_font_6x10_tr);
  gText((128 - u8g2.getStrWidth(title)) / 2, 20, title);
  u8g2.setFont(u8g2_font_4x6_tr);
  const char* a = (newBest && ((millis() / 400) & 1)) ? "NEW BEST!" : line1;
  gText((128 - u8g2.getStrWidth(a)) / 2, 30, a);
  const char* b = "Left: menu";
  gText((128 - u8g2.getStrWidth(b)) / 2, 39, b);
}

inline void uiFrameBegin() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  if (g_layout == LAYOUT_SQUARE) drawChromeSquare();
  else                           drawChromeWide();
  u8g2.setDrawColor(1);
}
inline void uiFrameEnd() { u8g2.sendBuffer(); }

// ---------------- PAUSE MENÜ (Középső gomb hosszú nyomása után) ----------------
enum PopupResult : uint8_t { POPUP_PENDING, POPUP_EXIT, POPUP_RESUME, POPUP_SETTINGS };
static bool g_popupArmed = false;

inline void popupOpen() {
  g_popupArmed = false;
  // A játékkép már a pufferben van, erre rajzolunk rá
  u8g2.setDrawColor(0);
  u8g2.drawBox(14, 8, 100, 48); // Nagyobb doboz a 3 régiónak
  u8g2.setDrawColor(1);
  u8g2.drawFrame(14, 8, 100, 48);
  
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* t = "- PAUSED -";
  u8g2.drawStr((128 - u8g2.getStrWidth(t)) / 2, 19, t);
  
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(20, 31, "Left  : Exit");
  u8g2.drawStr(20, 41, "Center: Resume");
  u8g2.drawStr(20, 51, "Right : Settings");
  u8g2.sendBuffer(); // Kiküldjük a kijelzőre
}

inline PopupResult popupUpdate() {
  if (!g_popupArmed) {
    if (!btnDown(BTN_CENTER)) g_popupArmed = true;   // Megvárjuk, amíg felengedi a gombot a hosszú nyomás után
    return POPUP_PENDING;
  }
  
  if (btnPressed(BTN_LEFT))   return POPUP_EXIT;
  if (btnPressed(BTN_CENTER)) return POPUP_RESUME;
  if (btnPressed(BTN_RIGHT))  return POPUP_SETTINGS;
  
  return POPUP_PENDING;
}