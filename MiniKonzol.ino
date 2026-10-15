// ============================================================
//  MiniKonzol.ino - ESP32-S3 Supermini retro zsebkonzol
// ============================================================
#include "esp_sleep.h"
#include "config.h"
#include "storage.h"
#include "input.h"
#include "audio.h"
#include "battery.h"
#include "ui.h"
#include "games.h"
#include "menu.h"
#include "title.h"
#include "calib.h"
#include "settings.h"
#include "intro.h"
#include "saver.h"
#include "power.h"

enum State : uint8_t { ST_MENU, ST_TITLE, ST_GAME, ST_POPUP, ST_CALIB, ST_SETTINGS, ST_STATS };

static State    g_state = ST_MENU;
static uint32_t g_lastFrame = 0;
static State    g_calibReturn = ST_MENU;
static State    g_settingsReturn = ST_MENU;
static bool     g_overSeen = false;
static uint32_t g_overAt = 0;

static void enterMenu() {
  statFlush();                     
  inputSetStrict(true);            
  inputFlush();
  g_state = ST_MENU;
}

static void startGame(uint8_t id) {
  g_curGame = id;
  g_difficulty = (Difficulty)g_diffSel[id];
  uiSetLayout(GAMES[id].layout);
  g_hud = HudInfo();
  GAMES[id].init();
  statAddPlay(id);
  g_hud.hiScore = max(g_highScore[id][g_difficulty], g_hud.score);
  inputSetStrict(false);           
  inputFlush();
  g_state = ST_GAME;
}

static void handleIdle() {
  if (g_state == ST_GAME || g_state == ST_POPUP)
    storageSubmitScore(g_curGame, g_difficulty, g_hud.score);
  storageFlush();
  if (saverRun()) {                              
    uint32_t t = millis();
    while (inputAnyRawDown() && millis() - t < 1500) delay(10); 
    
    inputFlush();
    if (g_state == ST_POPUP) { g_state = ST_GAME; inputSetStrict(false); }
    g_lastFrame = millis();
    return;
  }
  powerDeepSleep();
}

void setup() {
  setCpuFrequencyMhz(80);          
  bool woke = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);

  storageBegin();
  inputBegin();
  audioBegin();
  batBegin();
  uiBegin();

  if (g_batMv >= BAT_MIN_VALID_MV && g_batMv < BAT_CRITICAL_MV) powerLowBattery();

  if (woke) {
    wakeAnimation();               
    uint32_t t = millis();
    while (inputAnyRawDown() && millis() - t < 1500) delay(10);
  } else {
    introPlay();                   
    uint32_t t = millis();
    while (inputAnyRawDown() && millis() - t < 1500) delay(10);
  }

  enterMenu();
  g_lastFrame = millis();
}

void loop() {
  inputPoll();
  audioUpdate();
  storageUpdate();
  batUpdate();

  if (g_batCritical) powerLowBattery();          

  {
    uint32_t lim = SAVER_TIMES[g_saverSel];      
    if (lim && inputIdleMs() >= lim) { handleIdle(); return; }
  }

  uint32_t now = millis();
  if (now - g_lastFrame < FRAME_MS) { delay(1); return; }
  g_lastFrame += FRAME_MS;
  if (now - g_lastFrame >= FRAME_MS) g_lastFrame = now;   

  inputLatch();

  switch (g_state) {
    case ST_MENU: {
      int8_t g = menuUpdate();
      if (g == MENU_RESULT_SETTINGS) {
        settingsOpen();
        inputFlush();
        g_settingsReturn = ST_MENU; // <--- A LOGIKAI HIBA JAVÍTVA (Itt mondjuk meg, hogy a menüből jöttünk)
        g_state = ST_SETTINGS;
      } else if (g == MENU_RESULT_STATS) {
        statsOpen();
        inputFlush();
        g_state = ST_STATS;
      } else if (g >= 0) {
        g_curGame = g;
        titleOpen(g);
        inputFlush();
        g_state = ST_TITLE;
      } else {
        menuDraw();
      }
      break;
    }

    case ST_TITLE: {
      TitleResult r = titleUpdate();
      if (r == TITLE_START)     startGame(g_curGame);
      else if (r == TITLE_BACK) enterMenu();
      else                      titleDraw();
      break;
    }

    case ST_SETTINGS: {
      SettingsResult r = settingsUpdate();
      if (r == SET_BACK) {
        if (g_settingsReturn == ST_POPUP) {
          inputFlush();
          uiSetLayout(GAMES[g_curGame].layout);
          uiFrameBegin();
          GAMES[g_curGame].draw(); 
          popupOpen(); 
          g_state = ST_POPUP;
        } else {
          enterMenu();
        }
      } else if (r == SET_CALIB) {
        calibOpen();
        inputFlush();
        g_calibReturn = ST_SETTINGS;
        g_state = ST_CALIB;
      } else {
        settingsDraw();
      }
      break;
    }

    case ST_STATS: {
      if (statsUpdate()) enterMenu();
      else               statsDraw();
      break;
    }

    case ST_CALIB: {
      if (calibUpdate()) {
        if (g_calibReturn == ST_SETTINGS) { inputFlush(); g_state = ST_SETTINGS; }
        else                              enterMenu();
        g_calibReturn = ST_MENU;
      } else {
        calibDraw();
      }
      break;
    }

    case ST_GAME: {
      if (centerLongPressed()) {                 
        inputSetStrict(true);                    
        popupOpen();
        g_state = ST_POPUP;
        break;
      }
      if (g_hud.over) {
        if (!g_overSeen) { g_overSeen = true; g_overAt = millis(); }
        // SZINTAKTIKAI HIBA JAVÍTVA: millis() - g_overAt
        if (btnPressed(BTN_LEFT) && millis() - g_overAt > 500) {
          audioPlay(SFX_CANCEL);
          g_overSeen = false;
          enterMenu();
          break;
        }
      } else {
        g_overSeen = false;
      }
      statAddTime(g_curGame, FRAME_MS);
      GAMES[g_curGame].update();
      g_hud.hiScore = max(g_highScore[g_curGame][g_difficulty], g_hud.score);
      uiFrameBegin();
      GAMES[g_curGame].draw();
      uiFrameEnd();
      break;
    }

    case ST_POPUP: {                              
      PopupResult r = popupUpdate();
      if (r == POPUP_EXIT) {
        storageSubmitScore(g_curGame, g_difficulty, g_hud.score);   
        audioPlay(SFX_CANCEL);
        enterMenu();
      } else if (r == POPUP_RESUME) {
        audioPlay(SFX_OK);
        inputSetStrict(false);
        inputFlush();
        g_state = ST_GAME;
      } else if (r == POPUP_SETTINGS) {
        audioPlay(SFX_OK);
        settingsOpen();
        inputFlush();
        g_settingsReturn = ST_POPUP; 
        g_state = ST_SETTINGS;
      }
      break;
    }
  }
}