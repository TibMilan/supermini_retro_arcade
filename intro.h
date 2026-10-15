#pragma once
// ============================================================
//  intro.h  -  "ARCADE BIG BANG"  (SSD1306 128x64, U8g2)
//
//  Történet (10,4 s):
//   0.0 -  4.2 s  Attract mód: Pac-Man megeszi a szellemet, a Dino átugorja a
//                 leeső Tetris-elemet, az Invader lelövi a következőt, a Snake
//                 körbe-körbe fut a kijelző szélén és almát eszik, a Flappy
//                 madár átszeli az egészet.
//   4.2 -  5.6 s  Egy Minesweeper-akna jelenik meg középen és szó szerint
//                 beszippantja az összes szereplőt (spirál + zsugorodás).
//   5.6 -  6.4 s  BUMM: villanás, lökéshullám, rázkódás, törmelékpixelek.
//   6.4 -  8.6 s  A törmelékpixelek visszaáramlanak és összeállnak a főcímmé.
//   8.6 - 10.4 s  Fényvillanás a feliraton, sarokkeretek, villogó PRESS START.
//
//  v2 javítások:
//   - Zene újraírva: egy hangnem (A-moll -> C-dúr), tiszta ritmus, a jelenetek
//     eseményeihez igazítva; nincs 50 ms-nál rövidebb hang, mert az I2C kijelző-
//     küldés alatt (~25 ms) az audioUpdate() nem futott, így a rövid hangok
//     csúsztak / kiestek. A küldés most soronként történik, közben hangfrissítéssel.
//   - Minden rajzolás levágva a képernyőre: a u8g2 koordinátái előjel nélküliek,
//     ezért a negatív x/y (belépő sprite-ok, rázkódás) átfordult és villogott.
//   - Lökéshullám: saját körrajzoló (a u8g2 drawCircle 63 px felett elromlik).
//   - Fényfutam a címen: közvetlen pufferművelet (negatív koordinátás vonalak nélkül).
//   - Részecskék: a főcím összes pixele megkapja a saját részecskéjét (régen a
//     felirat és a vonalak a 1024-es korlát miatt kimaradtak, majd "bepattantak").
//   - Tetris-elemek egyenletesen esnek (a 4 px / 26 ms lépcső 30 FPS-nél ugrált).
//
//  Beállítás: INTRO_TITLE / INTRO_SUBTITLE / INTRO_PROMPT, illetve
//  INTRO_SKIP_PRESSED() (pl. gomb olvasás) az #include előtt definiálható.
//  Az animáció az idő (millis) függvénye, nem a képkockaszámé, így lassú
//  rajzolásnál sem csúszik el a zenétől.
// ============================================================
#include "config.h"
#include "ui.h"
#include "audio.h"
#include <math.h>
#include <stdlib.h>

#ifndef INTRO_TITLE
#define INTRO_TITLE    "SUPERMINI"
#endif
#ifndef INTRO_SUBTITLE
#define INTRO_SUBTITLE "RETRO ARCADE"
#endif
#ifndef INTRO_PROMPT
#define INTRO_PROMPT   "PRESS ANY KEY"   // "" = nincs
#endif
#include "input.h"
#ifndef INTRO_SKIP_PRESSED
#define INTRO_SKIP_PRESSED() inputAnyRawDown()
#endif

// --- idővonal (ms) ---
#define IN_T_SUCK   4200   // aknamegjelenés, szippantás kezdete
#define IN_T_BOOM   5600   // robbanás
#define IN_T_ASM    6400   // törmelék visszaáramlás kezdete
#define IN_T_DONE   8600   // főcím kész
#define IN_TOTAL   10400

#define IN_MAXP     2048   // max törmelékpixel (~40 KB heap, az intro végén felszabadul)
#ifndef IN_SPLIT_SEND
#define IN_SPLIT_SEND  1   // 1 = kijelző-küldés soronként, közben audioUpdate() (pontosabb zene)
#endif
#define IN_LAMBDA    400.0f
#define IN_CX         64.0f
#define IN_CY         32.0f

// ============================================================
//  SPRITE-OK (ASCII-ből generálva; MSB = bal szélső pixel)
// ============================================================
struct IntroSpr { uint8_t w, h; const uint16_t* r; };
// PAC_OPEN (8x8)
//   ..####..
//   .######.
//   #####...
//   ####....
//   ####....
//   #####...
//   .######.
//   ..####..
static const uint16_t INR_PAC_OPEN[8] = {0x003C,0x007E,0x00F8,0x00F0,0x00F0,0x00F8,0x007E,0x003C};
static const IntroSpr INSPR_PAC_OPEN = {8, 8, INR_PAC_OPEN};
// PAC_MID (8x8)
//   ..####..
//   .######.
//   #######.
//   #####...
//   #####...
//   #######.
//   .######.
//   ..####..
static const uint16_t INR_PAC_MID[8] = {0x003C,0x007E,0x00FE,0x00F8,0x00F8,0x00FE,0x007E,0x003C};
static const IntroSpr INSPR_PAC_MID = {8, 8, INR_PAC_MID};
// PAC_SHUT (8x8)
//   ..####..
//   .######.
//   ########
//   ########
//   ########
//   ########
//   .######.
//   ..####..
static const uint16_t INR_PAC_SHUT[8] = {0x003C,0x007E,0x00FF,0x00FF,0x00FF,0x00FF,0x007E,0x003C};
static const IntroSpr INSPR_PAC_SHUT = {8, 8, INR_PAC_SHUT};
// GHOST (8x8)
//   ..####..
//   .######.
//   ##.##.##
//   ##.##.##
//   ########
//   ########
//   ########
//   ##.##.##
static const uint16_t INR_GHOST[8] = {0x003C,0x007E,0x00DB,0x00DB,0x00FF,0x00FF,0x00FF,0x00DB};
static const IntroSpr INSPR_GHOST = {8, 8, INR_GHOST};
// GHOST_SC (8x8)
//   ..####..
//   .######.
//   ########
//   ##.##.##
//   ########
//   #.#.#.#.
//   ########
//   ##.##.##
static const uint16_t INR_GHOST_SC[8] = {0x003C,0x007E,0x00FF,0x00DB,0x00FF,0x00AA,0x00FF,0x00DB};
static const IntroSpr INSPR_GHOST_SC = {8, 8, INR_GHOST_SC};
// EYES (8x8)
//   ........
//   ........
//   .##..##.
//   .##..##.
//   .#....#.
//   ........
//   ........
//   ........
static const uint16_t INR_EYES[8] = {0x0000,0x0000,0x0066,0x0066,0x0042,0x0000,0x0000,0x0000};
static const IntroSpr INSPR_EYES = {8, 8, INR_EYES};
// INV_A (8x8)
//   ...##...
//   ..####..
//   .######.
//   ##.##.##
//   ########
//   ..#..#..
//   .#.##.#.
//   #.#..#.#
static const uint16_t INR_INV_A[8] = {0x0018,0x003C,0x007E,0x00DB,0x00FF,0x0024,0x005A,0x00A5};
static const IntroSpr INSPR_INV_A = {8, 8, INR_INV_A};
// INV_B (8x8)
//   ...##...
//   ..####..
//   .######.
//   ##.##.##
//   ########
//   .#.##.#.
//   #......#
//   .#....#.
static const uint16_t INR_INV_B[8] = {0x0018,0x003C,0x007E,0x00DB,0x00FF,0x005A,0x0081,0x0042};
static const IntroSpr INSPR_INV_B = {8, 8, INR_INV_B};
// BIRD_DN (16x12)
//   ...######.......
//   .##########.....
//   ########.###....
//   ########..##....
//   ############....
//   ###############.
//   #.#############.
//   ##.###.####.....
//   ###...########..
//   .##########.....
//   ...######.......
//   ................
static const uint16_t INR_BIRD_DN[12] = {0x1F80,0x7FE0,0xFF70,0xFF30,0xFFF0,0xFFFE,0xBFFE,0xDDE0,0xE3FC,0x7FE0,0x1F80,0x0000};
static const IntroSpr INSPR_BIRD_DN = {16, 12, INR_BIRD_DN};
// BIRD_MD (16x12)
//   ...######.......
//   .##########.....
//   ########.###....
//   ########..##....
//   ############....
//   #.####.########.
//   ##....#########.
//   ###########.....
//   ##############..
//   .##########.....
//   ...######.......
//   ................
static const uint16_t INR_BIRD_MD[12] = {0x1F80,0x7FE0,0xFF70,0xFF30,0xFFF0,0xBDFE,0xC3FE,0xFFE0,0xFFFC,0x7FE0,0x1F80,0x0000};
static const IntroSpr INSPR_BIRD_MD = {16, 12, INR_BIRD_MD};
// BIRD_UP (16x12)
//   ...######.......
//   .##########.....
//   ########.###....
//   ########..##....
//   #.#...######....
//   ##.###.########.
//   ###############.
//   ###########.....
//   ##############..
//   .##########.....
//   ...######.......
//   ................
static const uint16_t INR_BIRD_UP[12] = {0x1F80,0x7FE0,0xFF70,0xFF30,0xA3F0,0xDDFE,0xFFFE,0xFFE0,0xFFFC,0x7FE0,0x1F80,0x0000};
static const IntroSpr INSPR_BIRD_UP = {16, 12, INR_BIRD_UP};
// DINO_A (16x16)
//   ........#######.
//   ........########
//   ........##.#####
//   ........########
//   ........########
//   ........#####...
//   #.......####....
//   #......#####....
//   ##.....######.#.
//   ###...#########.
//   .###.##########.
//   ..#############.
//   ...###########..
//   ....########....
//   .....##..##.....
//   .....##..#......
static const uint16_t INR_DINO_A[16] = {0x00FE,0x00FF,0x00DF,0x00FF,0x00FF,0x00F8,0x80F0,0x81F0,0xC1FA,0xE3FE,0x77FE,0x3FFE,0x1FFC,0x0FF0,0x0660,0x0640};
static const IntroSpr INSPR_DINO_A = {16, 16, INR_DINO_A};
// DINO_B (16x16)
//   ........#######.
//   ........########
//   ........##.#####
//   ........########
//   ........########
//   ........#####...
//   #.......####....
//   #......#####....
//   ##.....######.#.
//   ###...#########.
//   .###.##########.
//   ..#############.
//   ...###########..
//   ....########....
//   .....#...##.....
//   .....##..##.....
static const uint16_t INR_DINO_B[16] = {0x00FE,0x00FF,0x00DF,0x00FF,0x00FF,0x00F8,0x80F0,0x81F0,0xC1FA,0xE3FE,0x77FE,0x3FFE,0x1FFC,0x0FF0,0x0460,0x0660};
static const IntroSpr INSPR_DINO_B = {16, 16, INR_DINO_B};
// DINO_J (16x16)
//   ........#######.
//   ........########
//   ........##.#####
//   ........########
//   ........########
//   ........#####...
//   #.......####....
//   #......#####....
//   ##.....######.#.
//   ###...#########.
//   .###.##########.
//   ..#############.
//   ...###########..
//   ....########....
//   .....##..##.....
//   .....##..##.....
static const uint16_t INR_DINO_J[16] = {0x00FE,0x00FF,0x00DF,0x00FF,0x00FF,0x00F8,0x80F0,0x81F0,0xC1FA,0xE3FE,0x77FE,0x3FFE,0x1FFC,0x0FF0,0x0660,0x0660};
static const IntroSpr INSPR_DINO_J = {16, 16, INR_DINO_J};

// ============================================================
//  ZENE ({frekvencia, ms}) - A-moll -> C-dúr, a jelenetekhez igazítva.
//  Szakaszhatárok (ms): 900 | 1350 | 1500 | 2800 | 4200 | 5600 | 6400 | 8600 | 10400
//  Minden szakasz hossza pontosan annyi, mint a hozzá tartozó jelenet.
// ============================================================
static const Note INTRO_SONG[] = {
  // 0.0-0.9 s  Pac-Man üldözés: A-moll arpeggio (6 x 150)
  {440,150}, {523,150}, {659,150}, {880,150}, {659,150}, {523,150},
  // 0.9-1.35 s  power pellet: A-moll skála felfelé (9 x 50)
  {330,50}, {392,50}, {440,50}, {494,50}, {523,50}, {587,50}, {659,50}, {784,50}, {880,50},
  // 1.35-1.5 s  szellem megevése (70 + 80)
  {1319,70}, {1760,80},
  // 1.5-2.8 s  vidám arcade téma, C-dúr (1300)
  {523,100}, {659,100}, {784,100}, {659,100}, {523,100}, {659,100}, {784,100}, {1047,100},
  {784,100}, {659,100}, {523,100}, {392,200},
  // 2.8-3.4 s  feszültség: lecsúszó kromatikus lépések (a Dino ugrik)
  {659,100}, {622,100}, {587,100}, {554,100}, {523,100}, {494,100},
  // 3.4-3.55 s  lézerlövés (az Invader tüzel)
  {1760,50}, {1175,50}, {784,50},
  // 3.55-3.75 s  sorfelszámolás (a Tetris-elem felrobban): C-dúr csillanás
  {523,50}, {659,50}, {784,50}, {1047,50},
  // 3.75-4.2 s  emelkedés az akna megjelenéséig (A harmonikus moll)
  {440,50}, {494,50}, {523,50}, {587,50}, {659,50}, {698,50}, {831,50}, {880,50}, {988,50},
  // 4.2-5.5 s  szippantás: egyre gyorsuló, felfelé futó A-moll pentaton (4x150, 4x100, 6x50)
  {440,150}, {523,150}, {587,150}, {659,150},
  {784,100}, {880,100}, {1047,100}, {1175,100},
  {1319,50}, {1568,50}, {1760,50}, {2093,50}, {2349,50}, {2637,50},
  // 5.5-5.6 s  csend a robbanás előtt (a fehér gömb nő)
  {0,100},
  // 5.6-6.4 s  BUMM: lefelé zuhanó söprés + mély dörej (10 x 50 + 150 + 150)
  {1700,50}, {1276,50}, {958,50}, {719,50}, {540,50}, {405,50}, {304,50}, {228,50}, {171,50}, {129,50},
  {110,150}, {0,150},
  // 6.4-8.6 s  összeállás: C - F - G - C arpeggiók, felfelé lépcsőzve (21 x 100 + 100)
  {523,100}, {659,100}, {784,100}, {1047,100}, {784,100}, {659,100},
  {698,100}, {880,100}, {1047,100}, {1397,100}, {1047,100}, {880,100},
  {784,100}, {988,100}, {1175,100}, {1568,100}, {1175,100}, {988,100},
  {1047,100}, {1319,100}, {1568,100}, {0,100},
  // 8.6-10.4 s  főcím fanfár
  {523,150}, {659,150}, {784,150}, {1047,450}, {0,100}, {784,100}, {1047,700},
};

// ============================================================
//  Segédek
// ============================================================
static int8_t   inOx = 0, inOy = 0;     // rázkódás
static float    inE = 0;                // szippantás előrehaladása 0..1
static float    inPacMax = -100;        // meddig jutott Pac-Man (pöttyök)
static uint32_t inRngS = 0x1badf00d;

static inline uint32_t inRnd() { inRngS ^= inRngS << 13; inRngS ^= inRngS >> 17; inRngS ^= inRngS << 5; return inRngS; }
static inline float inRf() { return (inRnd() >> 8) * (1.0f / 16777216.0f); }

// Elnyelés: spirál a közép körül + zsugorodás. false = elnyelte a lyuk.
static bool inXf(float &cx, float &cy, float &f) {
  f = 1.0f;
  if (inE <= 0.0f) return true;
  if (inE > 0.965f) return false;
  float k = 1.0f - inE, th = 2.8f * inE;
  float dx = cx - IN_CX, dy = cy - IN_CY, c = cosf(th), s = sinf(th);
  cx = IN_CX + (dx * c - dy * s) * k;
  cy = IN_CY + (dx * s + dy * c) * k;
  f = 1.0f - 0.6f * inE;
  return true;
}

// --- Biztonságos rajzolók -----------------------------------------------
// A u8g2 koordinátái előjel nélküliek, ezért egy negatív x/y (pl. a bal szélen
// belépő sprite, vagy a rázkódás) átfordul, és a forma eltűnik / villog.
// Itt minden a 128x64-es képernyőre vágva kerül a u8g2-be.
static inline void inPx(int x, int y) {
  if ((unsigned)x < 128u && (unsigned)y < 64u) u8g2.drawPixel(x, y);
}
static inline void inDot(int x, int y) { inPx(x + inOx, y + inOy); }

static void inBox(int x, int y, int w, int h) {
  int x1 = x + w, y1 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > 128) x1 = 128;
  if (y1 > 64) y1 = 64;
  if (x1 <= x || y1 <= y) return;
  u8g2.drawBox(x, y, x1 - x, y1 - y);
}

static void inHLine(int x, int y, int len) {
  if ((unsigned)y >= 64u) return;
  int x1 = x + len;
  if (x < 0) x = 0;
  if (x1 > 128) x1 = 128;
  if (x1 > x) u8g2.drawHLine(x, y, x1 - x);
}

// Körvonal tetszőleges sugárral (a u8g2 drawCircle 8 biten ~63 px fölött hibás)
static void inRing(int cx, int cy, int r) {
  if (r < 1) return;
  int x = 0, y = r, d = 1 - r;
  while (x <= y) {
    inPx(cx + x, cy + y); inPx(cx - x, cy + y); inPx(cx + x, cy - y); inPx(cx - x, cy - y);
    inPx(cx + y, cy + x); inPx(cx - y, cy + x); inPx(cx + y, cy - x); inPx(cx - y, cy - x);
    if (d < 0) d += 2 * x + 3; else { d += 2 * (x - y) + 5; y--; }
    x++;
  }
}

// Ferde, 8 px széles fénysáv a főcím sávjában (y 12..51): a pufferben invertál
static void inShine(int xs) {
  uint8_t *b = u8g2.getBufferPtr();
  const int stride = u8g2.getBufferTileWidth() * 8;
  for (int y = 12; y < 52; y++) {
    int x0 = xs + (18 * (52 - y)) / 40;
    for (int x = x0; x < x0 + 8; x++)
      if ((unsigned)x < 128u) b[(y >> 3) * stride + x] ^= (uint8_t)(1 << (y & 7));
  }
}

// Kijelző-küldés: soronként, közben a hang is frissül (a teljes sendBuffer ~25 ms-ig blokkol)
static void inSend() {
#if IN_SPLIT_SEND
  const uint8_t th = u8g2.getBufferTileHeight(), tw = u8g2.getBufferTileWidth();
  for (uint8_t r = 0; r < th; r++) { u8g2.updateDisplayArea(0, r, tw, 1); audioUpdate(); }
#else
  u8g2.sendBuffer();
#endif
}

static void inBlock(float x, float y, int w, int h) {
  float cx = x + w * 0.5f, cy = y + h * 0.5f, f;
  if (!inXf(cx, cy, f)) return;
  int ww = (int)lroundf(w * f); if (ww < 1) ww = 1;
  int hh = (int)lroundf(h * f); if (hh < 1) hh = 1;
  inBox((int)lroundf(cx - ww * 0.5f) + inOx, (int)lroundf(cy - hh * 0.5f) + inOy, ww, hh);
}

static void inSpriteRaw(const IntroSpr &s, float x, float y, float sc, bool flip, int ox, int oy) {
  for (int r = 0; r < s.h; r++) {
    uint16_t row = s.r[r];
    int y0 = (int)lroundf(y + r * sc) + oy + inOy;
    int y1 = (int)lroundf(y + (r + 1) * sc) + oy + inOy; if (y1 <= y0) y1 = y0 + 1;
    int d = 0;
    while (d < s.w) {
      int c = flip ? (s.w - 1 - d) : d;
      if (!((row >> (s.w - 1 - c)) & 1)) { d++; continue; }
      int d0 = d;
      while (d < s.w) { int cc = flip ? (s.w - 1 - d) : d; if (!((row >> (s.w - 1 - cc)) & 1)) break; d++; }
      int x0 = (int)lroundf(x + d0 * sc) + ox + inOx;
      int x1 = (int)lroundf(x + d * sc) + ox + inOx; if (x1 <= x0) x1 = x0 + 1;
      inBox(x0, y0, x1 - x0, y1 - y0);
    }
  }
}

// x,y = bal-felső sarok; halo = fekete körvonal, hogy az átfedő sprite-ok elváljanak
static void inSprite(const IntroSpr &s, float x, float y, float sc = 1.0f, bool flip = false, bool halo = false) {
  float cx = x + s.w * sc * 0.5f, cy = y + s.h * sc * 0.5f, f;
  if (!inXf(cx, cy, f)) return;
  sc *= f; x = cx - s.w * sc * 0.5f; y = cy - s.h * sc * 0.5f;
  if (halo) {
    u8g2.setDrawColor(0);
    inSpriteRaw(s, x, y, sc, flip, -1, 0); inSpriteRaw(s, x, y, sc, flip, 1, 0);
    inSpriteRaw(s, x, y, sc, flip, 0, -1); inSpriteRaw(s, x, y, sc, flip, 0, 1);
    u8g2.setDrawColor(1);
  }
  inSpriteRaw(s, x, y, sc, flip, 0, 0);
}

struct InKP { int16_t t; int16_t v; };
static float inKP(const InKP *k, uint8_t n, float t) {
  if (t <= k[0].t) return k[0].v;
  for (uint8_t i = 1; i < n; i++)
    if (t <= k[i].t) { float u = (t - k[i - 1].t) / (float)(k[i].t - k[i - 1].t); return k[i - 1].v + (k[i].v - k[i - 1].v) * u; }
  return k[n - 1].v;
}
#define IN_N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

// --- szereplők kulcspontjai (idő ms, x px) ---
static const InKP INKP_PAC[]    = {{0, -4}, {900, 68}, {1350, 34}, {1500, 34}, {4200, 92}};
static const InKP INKP_GHOST[]  = {{0, -28}, {900, 44}, {1350, 24}};
static const InKP INKP_EYES[]   = {{1350, 24}, {1900, -20}};
static const InKP INKP_GHOST2[] = {{3600, -20}, {4200, 56}};
static const InKP INKP_DINO[]   = {{1000, -18}, {1700, 6}};
static const InKP INKP_BIRD[]   = {{1900, -20}, {4200, 64}};

// Tetris elemek (blokk-koordináták, 4 px rács, 3x3 px-es blokkok)
static const int8_t INPC_L[4][2] = {{0, 0}, {0, 1}, {0, 2}, {1, 2}};
static const int8_t INPC_T[4][2] = {{0, 0}, {1, 0}, {2, 0}, {1, 1}};
static void inPiece(const int8_t (*b)[2], float x, float y) {
  for (int i = 0; i < 4; i++) inBlock(x + b[i][0] * 4, y + b[i][1] * 4, 3, 3);
}
// leesés: egyenletes, ~154 px/s (a régi 4 px / 26 ms lépcső 30 FPS-nél 4-8 px-es
// ugrásokat adott, ami szaggatottnak látszott)
static float inFallY(int32_t t, int32_t t0, float y0, float yEnd, int32_t *tLand) {
  const float v = 4.0f / 26.0f;
  float y = (t < t0) ? y0 : y0 + v * (float)(t - t0);
  if (y >= yEnd) { if (tLand) *tLand = t0 + (int32_t)ceilf((yEnd - y0) / v); return yEnd; }
  if (tLand) *tLand = -1;
  return y;
}

// Snake útvonal a kijelző szélén (3x3-as szegmensek középpontja)
static void inPathPos(int di, int &x, int &y) {
  const int W = 125, H = 61, P = 2 * (W + H);
  di = ((di % P) + P) % P;
  if (di < W) { x = 1 + di; y = 1; }
  else if ((di -= W) < H) { x = 126; y = 1 + di; }
  else if ((di -= H) < W) { x = 126 - di; y = 62; }
  else { di -= W; x = 1; y = 62 - di; }
}

// ============================================================
//  1. RÉSZ: attract mód jelenet (t >= IN_T_SUCK-nál a pozíciók befagynak,
//  és az inE-vel arányosan a közép felé szívódnak)
// ============================================================
static void inScene(int32_t t) {
  const int32_t tt = t < IN_T_SUCK ? t : IN_T_SUCK;
  const float ft = (float)tt;

  // ---- talaj (a Dino sávja) ----
  if (tt >= 950) {
    float scroll = (ft - 1000.0f) * 0.1f;
    int w = (int)((ft - 950.0f) * 0.9f); if (w > 128) w = 128;
    int g0 = (int)(inE * 64), g1 = (w < 128 ? w : 127) - g0;
    if (g1 > g0) {
      inHLine(g0 + inOx, 58 + inOy, g1 - g0 + 1);
      if (inE <= 0.0f)
        for (int i = 0; i < 9; i++) {
          int bx = (((i * 29 + 1024 - (int)scroll) % 128) + 128) % 128;
          if (bx < w) { inDot(bx, 60); inDot(bx + 1, 60); }
        }
    }
  }

  // ---- Pac-Man pöttyök + nagy pötty ----
  for (int i = 0; i < 14; i++) {
    int dx = 14 + i * 8;
    if (dx < inPacMax + 12) continue;
    inBlock((float)dx, 35, 2, 2);
  }
  if (inPacMax < 68 && ((tt / 150) & 1) == 0) {
    float cx = 87, cy = 36, f;
    if (inXf(cx, cy, f)) u8g2.drawDisc((int)cx + inOx, (int)cy + inOy, f > 0.7f ? 3 : 2);
  }

  // ---- Snake: kerülgeti a kijelző szélét, almát eszik ----
  {
    const float D = (ft - 300.0f) * 0.09f;          // fej útja px-ben
    const int APPLE[2] = {140, 290};
    for (int a = 0; a < 2; a++) {
      if (D >= APPLE[a]) continue;
      if (((tt / 200) & 1) == 0 || tt < 300) { int ax, ay; inPathPos(APPLE[a], ax, ay); inBlock(ax - 1, ay - 1, 3, 3); }
    }
    if (D >= 0) {
      int len = 6 + 4 * ((D >= APPLE[0]) + (D >= APPLE[1]));
      for (int k = len - 1; k >= 0; k--) {
        float d = D - k * 4.0f; if (d < 0) continue;
        int x, y; inPathPos((int)d, x, y);
        inBlock(x - 1, y - 1, 3, 3);
      }
    }
  }

  // ---- Tetris elemek ----
  {
    int32_t land1, land2;
    float y1 = inFallY(tt, 1800, -12, 47, &land1);
    float x1 = 108.0f; if (land1 >= 0 && tt >= land1) x1 -= 0.1f * (tt - land1);
    if (tt >= 1800 && x1 > -12) inPiece(INPC_L, x1, y1);

    float y2 = inFallY(tt, 2800, -12, 51, &land2);
    float x2 = 100.0f; if (land2 >= 0 && tt >= land2) x2 -= 0.1f * (tt - land2);
    const int32_t HIT = 3562;
    if (tt >= 2800) {
      if (tt < HIT) inPiece(INPC_T, x2, y2);
      else if (tt < HIT + 120) { if (((tt - HIT) / 40) & 1) inPiece(INPC_T, x2, y2); }
      else {  // "sorfelszámolás": pixel-szétrobbanás
        float tau = (float)(tt - HIT - 120);
        if (tau < 360) {
          float hx = 100.0f - 0.1f * (float)(HIT - land2) + 6, hy = 54;
          for (int k = 0; k < 22; k++) {
            float a = k * 0.2856f + 0.3f, v = 0.05f + 0.03f * (k % 4);
            int px = (int)(hx + cosf(a) * v * tau), py = (int)(hy + sinf(a) * v * tau + 0.0004f * tau * tau);
            if (k & 1) inDot(px, py); else inBlock((float)px, (float)py, 2, 2);
          }
        }
      }
    }
  }

  // ---- Space Invaders: balra lépdel, lelövi a Tetris-elemet ----
  {
    float steps = ft < 1200 ? 0 : floorf(0.036f * (ft - 1200.0f) / 4.0f);
    float X = 130.0f - 4.0f * steps;
    const IntroSpr &fr = (((int)steps) & 1) ? INSPR_INV_B : INSPR_INV_A;
    for (int i = 0; i < 3; i++) {
      float x = X + i * 14;
      if (x > -8 && x < 128) inSprite(fr, x, 4);
    }
    const int32_t FIRE = 3420, HIT = 3562;
    if (tt >= FIRE && tt < HIT) {
      float sx = 130.0f - 4.0f * floorf(0.036f * (FIRE - 1200.0f) / 4.0f) + 14 + 4;   // a lövő (2.) invader közepe
      float by = 14 + 0.26f * (tt - FIRE);
      int zig = (((tt - FIRE) / 40) & 1) ? 1 : 0;
      inBlock(sx - 1 + zig, by, 1, 3);
      inBlock(sx + zig, by + 2, 1, 3);
    }
  }

  // ---- Flappy bird ----
  if (tt >= 1900) {
    static const uint8_t SEQ[4] = {1, 2, 1, 0};
    const IntroSpr *bf[3] = {&INSPR_BIRD_DN, &INSPR_BIRD_MD, &INSPR_BIRD_UP};
    float bx = inKP(INKP_BIRD, IN_N(INKP_BIRD), ft);
    float by = 16.0f + 4.0f * sinf(ft * 0.011f);
    inSprite(*bf[SEQ[(t / 80) & 3]], bx, by, 1.0f, false, true);
  }

  // ---- szellem(ek) ----
  {
    if (tt < 900) {
      inSprite(INSPR_GHOST, inKP(INKP_GHOST, IN_N(INKP_GHOST), ft), 28, 2, false, true);
    } else if (tt < 1350) {
      bool blink = tt > 1150 && (((tt - 1150) / 70) & 1);
      inSprite(blink ? INSPR_GHOST : INSPR_GHOST_SC, inKP(INKP_GHOST, IN_N(INKP_GHOST), ft), 28, 2, false, true);
    } else if (tt < 1900) {
      inSprite(INSPR_EYES, inKP(INKP_EYES, IN_N(INKP_EYES), ft), 28, 2, false, false);
    } else if (tt >= 3600) {
      inSprite(INSPR_GHOST, inKP(INKP_GHOST2, IN_N(INKP_GHOST2), ft), 28, 2, false, true);
    }
  }

  // ---- Pac-Man ----
  {
    static const IntroSpr *PF[4] = {&INSPR_PAC_OPEN, &INSPR_PAC_MID, &INSPR_PAC_SHUT, &INSPR_PAC_MID};
    float px = inKP(INKP_PAC, IN_N(INKP_PAC), ft);
    if (t < IN_T_SUCK && px > inPacMax) inPacMax = px;
    bool left = (tt >= 900 && tt < 1500);
    inSprite(*PF[(t / 70) & 3], px, 28, 2, left, true);
    if (tt >= 1350 && tt < 1850) {   // "200" pontfelugró
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(30 + inOx, 25 + inOy - (int)((tt - 1350) / 100), "200");
    }
  }

  // ---- Dino ----
  if (tt >= 1000) {
    float dx = inKP(INKP_DINO, IN_N(INKP_DINO), ft), dy = 42;
    const IntroSpr *f;
    if (tt >= 2850 && tt < 3450) { float u = (tt - 2850) / 600.0f; dy -= 80.0f * u * (1.0f - u); f = &INSPR_DINO_J; }
    else f = ((t / 90) & 1) ? &INSPR_DINO_A : &INSPR_DINO_B;
    inSprite(*f, dx, dy, 1, false, true);
  }
}

// ============================================================
//  2. RÉSZ: a Minesweeper-akna + szippantás effektek
// ============================================================
struct InStar { float a, r; };
static InStar inStars[40];

static void inMine(int32_t t) {
  float u = (t - IN_T_SUCK) / (float)(IN_T_BOOM - IN_T_SUCK); if (u > 1) u = 1;
  float spawn = (t - IN_T_SUCK) / 150.0f; if (spawn > 1) spawn = 1;
  float rm = (7.0f + 4.0f * u) * spawn;
  int cx = (int)IN_CX + inOx, cy = (int)IN_CY + inOy;
  bool blink = u > 0.55f && (((t / 70) & 1) != 0);
  u8g2.setDrawColor(0);                                  // fekete udvar: elválik a beszippantott szereplőktől
  u8g2.drawDisc(cx, cy, (int)(rm + 7));
  uint8_t col = 1;
  if (blink) { u8g2.setDrawColor(1); u8g2.drawDisc(cx, cy, (int)(rm + 4)); col = 0; }
  u8g2.setDrawColor(col);
  u8g2.drawDisc(cx, cy, (int)rm);
  float rot = u * 6.0f;
  for (int k = 0; k < 8; k++) {
    float a = rot + k * 0.7854f, l1 = rm - 1, l2 = rm + 3.5f + 1.5f * sinf(t * 0.03f + k);
    if (blink && l2 > rm + 4) l2 = rm + 4;
    u8g2.drawLine(cx + (int)(cosf(a) * l1), cy + (int)(sinf(a) * l1), cx + (int)(cosf(a) * l2), cy + (int)(sinf(a) * l2));
  }
  u8g2.setDrawColor(col ? 0 : 1);                        // csillanás
  u8g2.drawBox(cx - (int)(rm * 0.5f), cy - (int)(rm * 0.5f), 2, 2);
  u8g2.setDrawColor(1);
}

static void inStarsDraw() {
  if (inE < 0.03f) return;
  for (int i = 0; i < 40; i++) {
    float k = 1.0f - inE, th = inStars[i].a + 3.2f * inE, r = inStars[i].r * k * k;
    if (r < 3.0f) continue;
    inDot((int)(IN_CX + cosf(th) * r), (int)(IN_CY + sinf(th) * r * 0.6f + 0.0f));
  }
}

// ============================================================
//  3. RÉSZ: törmelék -> főcím
// ============================================================
struct InDbr {
  float   vx, vy;      // kirepülési sebesség (px/ms)
  int16_t x0, y0;      // pozíció a visszaáramlás kezdetekor (1/16 px)
  int8_t  cx, cy;      // ívelés (0.5 px)
  uint8_t tx, ty;      // cél pixel a főcímben
  uint16_t dl, du;     // késleltetés és időtartam a visszaáramlásnál
};

static const uint8_t *IN_FONT_TITLE = u8g2_font_logisoso16_tr;
static const uint8_t *IN_FONT_SUB   = u8g2_font_6x10_tr;

static void inDrawTitle() {
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);
  u8g2.setFont(IN_FONT_TITLE);
  u8g2.drawStr((128 - u8g2.getStrWidth(INTRO_TITLE)) / 2, 29, INTRO_TITLE);
  u8g2.setFont(IN_FONT_SUB);
  u8g2.drawStr((128 - u8g2.getStrWidth(INTRO_SUBTITLE)) / 2, 49, INTRO_SUBTITLE);
  u8g2.drawHLine(16, 34, 96);
  u8g2.drawHLine(16, 36, 96);
}

// kirepülés: lassuló mozgás + enyhe kavargás a közép körül
static void inFlight(float vx, float vy, float tau, float &x, float &y) {
  float k = IN_LAMBDA * (1.0f - expf(-tau / IN_LAMBDA));
  float dx = vx * k, dy = vy * k, th = 0.0006f * tau, c = cosf(th), sn = sinf(th);
  x = IN_CX + dx * c - dy * sn;
  y = IN_CY + dx * sn + dy * c;
}

// A főcím minden pixeléből részecske lesz. Először megszámoljuk, hány pixel van,
// és pontosan ennyit foglalunk (IN_MAXP fölött egyenletesen ritkítunk, nem a
// legalsó sorokat hagyjuk el, mint régen).
static uint16_t inBuildParticles(InDbr **out) {
  *out = nullptr;
  u8g2.clearBuffer();
  inDrawTitle();
  const uint8_t *buf = u8g2.getBufferPtr();
  const int stride = u8g2.getBufferTileWidth() * 8;
  uint32_t total = 0;
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      if (buf[(y >> 3) * stride + x] & (1 << (y & 7))) total++;
  if (!total) return 0;
  const uint32_t cap = total > IN_MAXP ? IN_MAXP : total;
  InDbr *P = (InDbr *)malloc(sizeof(InDbr) * cap);
  if (!P) return 0;
  *out = P;

  uint32_t acc = 0, n = 0;
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      if (buf[(y >> 3) * stride + x] & (1 << (y & 7))) {
        acc += cap;
        if (acc >= total) { acc -= total; if (n < cap) { P[n].tx = x; P[n].ty = y; n++; } }
      }
  for (int i = (int)n - 1; i > 0; i--) {            // célok összekeverése
    int j = inRnd() % (i + 1);
    uint8_t ax = P[i].tx, ay = P[i].ty; P[i].tx = P[j].tx; P[i].ty = P[j].ty; P[j].tx = ax; P[j].ty = ay;
  }
  for (uint32_t i = 0; i < n; i++) {
    float a = inRf() * 6.2831853f;
    float s = 0.03f + 0.15f * powf(inRf(), 0.8f);
    P[i].vx = cosf(a) * s; P[i].vy = sinf(a) * s * 0.55f;   // a 2:1 képernyőhöz igazított felhő
    P[i].dl = (uint16_t)(inRf() * 900.0f);
    P[i].du = (uint16_t)(700.0f + inRf() * 600.0f);       // dl+du <= 2200 = IN_T_DONE-IN_T_ASM
    float x0, y0;
    inFlight(P[i].vx, P[i].vy, (float)(IN_T_ASM + P[i].dl - IN_T_BOOM), x0, y0);
    P[i].x0 = (int16_t)(x0 * 16.0f); P[i].y0 = (int16_t)(y0 * 16.0f);
    float dx = P[i].tx - x0, dy = P[i].ty - y0, L = sqrtf(dx * dx + dy * dy) + 0.001f;
    float cv = (inRf() < 0.5f ? -1.0f : 1.0f) * (6.0f + inRf() * 16.0f) * 2.0f;
    P[i].cx = (int8_t)(-dy / L * cv); P[i].cy = (int8_t)(dx / L * cv);
  }
  return (uint16_t)n;
}

static void inPartPos(const InDbr &p, int32_t t, float &x, float &y) {
  int32_t t0 = IN_T_ASM + p.dl;
  if (t < t0) {
    inFlight(p.vx, p.vy, (float)(t - IN_T_BOOM), x, y);
    return;
  }
  float u = (t - t0) / (float)p.du; if (u > 1) u = 1;
  float e = u < 0.5f ? 2 * u * u : 1 - 2 * (1 - u) * (1 - u);
  float x0 = p.x0 * (1.0f / 16.0f), y0 = p.y0 * (1.0f / 16.0f);
  float sw = sinf(3.14159265f * e) * 0.5f;
  x = x0 + (p.tx - x0) * e + p.cx * sw;
  y = y0 + (p.ty - y0) * e + p.cy * sw;
}

static void inBoom(int32_t t, uint32_t frame, const InDbr *P, uint16_t n) {
  const int32_t tau = t - IN_T_BOOM;
  if (tau < 45) { u8g2.drawBox(0, 0, 128, 64); return; }           // vakító villanás
  const int cx = (int)IN_CX + inOx, cy = (int)IN_CY + inOy;
  if (tau < 300) u8g2.drawDisc(cx, cy, (int)(18.0f * (1.0f - tau / 300.0f)));   // tűzgömb
  if (tau < 800) {
    inRing(cx, cy, (int)(tau * 0.17f));
    if (tau > 120 && (frame & 1)) inRing(cx, cy, (int)((tau - 120) * 0.14f));
  }
  for (uint16_t i = 0; i < n; i++) {
    float x, y; inPartPos(P[i], t, x, y);
    int xi = (int)lroundf(x) + inOx, yi = (int)lroundf(y) + inOy;
    if (xi < 0 || xi > 127 || yi < 0 || yi > 63) continue;
    bool flying = t < IN_T_ASM + P[i].dl + (int32_t)(P[i].du * 0.8f);
    if (flying && ((frame + i) % 5) == 0) continue;                  // szikra-villódzás
    if ((i & 3) == 0 && tau < 300) inBox(xi, yi, 2, 2);
    else u8g2.drawPixel(xi, yi);
  }
}

static void inFinal(int32_t t) {
  const int32_t f = t - IN_T_DONE;
  inDrawTitle();
  // sarokkeretek
  {
    int L = f < 300 ? (int)(10.0f * f / 300.0f) : 10;
    if (L > 0) {
      u8g2.drawHLine(1, 1, L);       u8g2.drawVLine(1, 1, L);
      u8g2.drawHLine(127 - L, 1, L); u8g2.drawVLine(126, 1, L);
      u8g2.drawHLine(1, 62, L);      u8g2.drawVLine(1, 63 - L, L);
      u8g2.drawHLine(127 - L, 62, L); u8g2.drawVLine(126, 63 - L, L);
    }
  }
  // fényvillanás a feliraton (XOR sáv)
  if (f >= 100 && f < 750) {
    int xs = -30 + (int)((f - 100) * (190.0f / 650.0f));
    inShine(xs);
  }
  // villogó PRESS START
  if (f >= 600 && INTRO_PROMPT[0] && (((t) / 400) & 1) == 0) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr((128 - u8g2.getStrWidth(INTRO_PROMPT)) / 2, 60, INTRO_PROMPT);
  }
}

// ============================================================
//  FŐ BELÉPÉSI PONT
// ============================================================
inline void introPlay() {
  inRngS = 0x1badf00d;
  inPacMax = -100;
  for (int i = 0; i < 40; i++) { inStars[i].a = inRf() * 6.2831853f; inStars[i].r = 16.0f + inRf() * 100.0f; }

  InDbr *P = nullptr;
  uint16_t n = inBuildParticles(&P);

  audioPlay(INTRO_SONG);
  const uint32_t start = millis();
  uint32_t lastF = 0, frame = 0;

  while (true) {
    audioUpdate();
    uint32_t now = millis();
    if (now - lastF < 33) { delay(1); continue; }       // ~30 FPS
    lastF = now;
    int32_t t = (int32_t)(now - start);
    if (INTRO_SKIP_PRESSED()) break;
    frame++;

    // szippantás előrehaladása + rázkódás
    inE = 0;
    if (t >= IN_T_SUCK && t < IN_T_BOOM) { float u = (t - IN_T_SUCK) / (float)(IN_T_BOOM - IN_T_SUCK); inE = u * u; }
    int amp = 0;
    if (t >= 3900 && t < IN_T_SUCK) amp = 1;
    else if (t >= IN_T_SUCK && t < IN_T_BOOM) amp = 1 + (int)(3.0f * inE);
    else if (t >= IN_T_BOOM && t < IN_T_BOOM + 500) amp = 5 - (int)((t - IN_T_BOOM) * 5 / 500);
    inOx = amp ? (int)(inRnd() % (2 * amp + 1)) - amp : 0;
    inOy = amp ? (int)(inRnd() % (2 * amp + 1)) - amp : 0;

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFontMode(1);

    if (t < IN_T_BOOM) {
      inScene(t);
      if (t >= IN_T_SUCK) { inStarsDraw(); inMine(t); }
      if (t >= IN_T_BOOM - 100) {                        // feltöltődés: a villanás előtti fehér gömb
        float r = 8.0f + (t - (IN_T_BOOM - 100)) * 0.2f;
        u8g2.drawDisc((int)IN_CX + inOx, (int)IN_CY + inOy, (int)r);
      }
    } else if (t < IN_T_DONE) {
      if (n) inBoom(t, frame, P, n);
      else { inDrawTitle(); }
    } else {
      inFinal(t);
    }
    if (t >= IN_T_SUCK && t < IN_T_SUCK + 50) {           // akna-megjelenés: egy képkocka inverz villanás
      u8g2.setDrawColor(2); u8g2.drawBox(0, 0, 128, 64); u8g2.setDrawColor(1);
    }
    inSend();
  }

  audioStop();
  if (P) free(P);
  inOx = inOy = 0; inE = 0;
  u8g2.setMaxClipWindow();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(0);
}
