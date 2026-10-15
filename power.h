#pragma once
// ============================================================
//  power.h - deep-sleep (gombnyomasra ebred) + lemerult akku kijelzes
//  Ebredes: barmelyik gomb LOW (ext1, ANY_LOW), RTC pull-up-okkal.
//  Ebredeskor a program elorol indul (setup()).
// ============================================================
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "config.h"
#include "ui.h"
#include "audio.h"
#include "storage.h"
#include "input.h"

inline void powerDeepSleep() {
  storageFlush();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  u8g2.setPowerSave(1);                 // OLED ki
  audioShutdown();

  // Ha meg nyomva van valamelyik gomb, megvarjuk az elengedest (max 3 mp),
  // kulonben azonnal felebrednenk.
  uint32_t t = millis();
  while (inputAnyRawDown() && millis() - t < 3000) delay(10);

  uint64_t mask = 0;
  for (uint8_t i = 0; i < BTN_COUNT; i++) mask |= (1ULL << BTN_PINS[i]);
  esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_PINS[i]);
    rtc_gpio_pullup_en((gpio_num_t)BTN_PINS[i]);        // alaphelyzet HIGH alvas kozben is
  }
  esp_deep_sleep_start();
}

// Nagy lemerult elem a kijelzo kozepen, majd kikapcsolas
inline void powerLowBattery() {
  storageFlush();
  audioPlay(SFX_LOWBAT);
  for (uint8_t n = 0; n < 6; n++) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.drawFrame(30, 12, 60, 34);         // elem teste (2 px vastag)
    u8g2.drawFrame(31, 13, 58, 32);
    u8g2.drawBox(90, 23, 5, 12);            // sarok
    if (n % 2 == 0) u8g2.drawBox(36, 19, 9, 20);   // az utolso csik villog
    u8g2.setFont(u8g2_font_6x10_tr);
    const char* s = "LOW BATTERY";
    u8g2.drawStr((128 - u8g2.getStrWidth(s)) / 2, 61, s);
    u8g2.sendBuffer();
    uint32_t t = millis();
    while (millis() - t < 450) { audioUpdate(); delay(5); }
  }
  powerDeepSleep();
}
