#pragma once
// ============================================================
//  config.h - PIN kiosztas es globalis konstansok
// ============================================================
#include <Arduino.h>

// ---- Kijelzo (I2C) ----
constexpr uint8_t  PIN_SDA = 13;
constexpr uint8_t  PIN_SCL = 12;
constexpr uint32_t I2C_CLOCK_HZ = 800000;   // 400000 a biztos; ha 800k-nal hibas a kep, ird vissza

// ---- 5-utas gomb (INPUT_PULLUP, aktiv LOW) ----
constexpr uint8_t PIN_BTN_UP     = 7;
constexpr uint8_t PIN_BTN_DOWN   = 5;
constexpr uint8_t PIN_BTN_LEFT   = 6;
constexpr uint8_t PIN_BTN_RIGHT  = 10;
constexpr uint8_t PIN_BTN_CENTER = 9;

// ---- Piezo (hid-meghajtas, ellentetes fazis) ----
constexpr uint8_t PIN_BZ_A = 1;
constexpr uint8_t PIN_BZ_B = 2;

// ---- Akkumulator meres ----
constexpr uint8_t PIN_BAT = 8;
constexpr float   BAT_DIV_RATIO = 2.0f;     // 2x100k osztó -> 2.0 (finomhangolhato multimeterrel)

// ---- Akku vedelem ----
constexpr uint16_t BAT_CRITICAL_MV  = 3300; // ez ala (tartosan) -> lemerult kijelzes + kikapcsolas
constexpr uint16_t BAT_MIN_VALID_MV = 2000; // ez alatt nincs akku csatlakoztatva (pl. csak USB) -> figyelmen kivul

// ---- Idozitesek ----
constexpr uint8_t  TARGET_FPS     = 30;     // 30 vagy 60
constexpr uint32_t FRAME_MS       = 1000 / TARGET_FPS;
constexpr uint8_t  DEBOUNCE_MS    = 20;   // elengedes
constexpr uint8_t  PRESS_DEBOUNCE_MS = 35; // lenyomas: hosszabb, hogy a zaj/csatolt tüske (pl. piezo) ne szamitson gombnak
constexpr uint16_t LONG_PRESS_MS  = 2000;   // kozepso gomb: kenyszeritett kilepes
constexpr uint32_t IDLE_SLEEP_MS  = 30000;  // ennyi tetlenseg utan indul a kepernyokimelo
constexpr uint32_t SAVER_MS       = 7000;   // roboeyes hossza, utana deep-sleep

// ---- Hangero ----
constexpr uint8_t VOL_MAX     = 10;
constexpr uint8_t VOL_DEFAULT = 5;

// ---- Tipusok ----
enum Button : uint8_t { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_CENTER, BTN_COUNT };

// Minden layoutban van statuszsav (akku, hangero)
enum Layout : uint8_t {
  LAYOUT_SQUARE,   // A) 64x64 kozepen, oldalsavok (Tetris)
  LAYOUT_WIDE      // B) 128x52 alul, felso info sor (a tobbi jatek)
};

enum Difficulty : uint8_t { DIFF_EASY, DIFF_MEDIUM, DIFF_HARD, DIFF_COUNT };

constexpr uint8_t GAME_COUNT = 13;
