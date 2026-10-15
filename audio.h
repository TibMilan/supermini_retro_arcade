#pragma once
// ============================================================
//  audio.h - piezo hid-meghajtas (nem blokkolo hang)
//
//  Egyetlen LEDC csatorna hajtja a PIN_BZ_A labat; a PIN_BZ_B ugyanazt a
//  jelet HARDVERESEN INVERTALVA kapja (GPIO matrix) -> tokeletes ellenfazis.
//  Hangero = kitoltesi tenyezo (50% = maximalis kulonbsegi amplitudo).
//  Arduino-ESP32 core 3.x API-t hasznal.
// ============================================================
#include "config.h"
#include "storage.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"

struct Note { uint16_t freq; uint16_t ms; };   // freq == 0 -> csend

static const Note SFX_CLICK[]  = { {1800, 12} };
static const Note SFX_OK[]     = { {988, 60}, {1319, 90} };
static const Note SFX_CANCEL[] = { {660, 60}, {440, 90} };
static const Note SFX_LOWBAT[] = { {523, 160}, {392, 160}, {262, 340} };

static const Note* au_seq = nullptr;
static uint8_t     au_len = 0, au_idx = 0;
static uint32_t    au_noteEnd = 0;
static bool        au_playing = false;
static Note        au_single;
static Note        au_copy[12];                 // rovid sorozatokat lemasoljuk -> lokalis tombbel is biztonsagos

// 8 bites felbontas: 128 = 50% (max). Index = hangero (0 = nema).
static const uint8_t VOL_DUTY[VOL_MAX + 1] = { 0, 3, 6, 10, 16, 24, 36, 52, 76, 100, 128 };

// Nyugalmi allapot: MINDKET lab LOW (az invertalt B lab nem maradhat HIGH-on,
// kulonben allando egyenfeszultseg + kattogas/zaj kerul a piezora es a gombokra)
inline void audioIdleLevel() {
  ledcWrite(PIN_BZ_A, 0);
  esp_rom_gpio_connect_out_signal(PIN_BZ_B, SIG_GPIO_OUT_IDX, false, false);
  digitalWrite(PIN_BZ_B, LOW);
}
inline void audioActiveRoute() {
  esp_rom_gpio_connect_out_signal(PIN_BZ_B, LEDC_LS_SIG_OUT0_IDX + 0, true, false);
}

inline void audioBegin() {
  ledcAttachChannel(PIN_BZ_A, 2000, 8, 0);
  pinMode(PIN_BZ_B, OUTPUT);
  audioIdleLevel();
}

inline void audioStartNote(const Note& n) {
  if (n.freq == 0 || g_volume == 0) {
    audioIdleLevel();
  } else {
    audioActiveRoute();
    ledcChangeFrequency(PIN_BZ_A, n.freq, 8);
    ledcWrite(PIN_BZ_A, VOL_DUTY[g_volume]);
  }
  au_noteEnd = millis() + n.ms;
}

inline void audioPlay(const Note* seq, uint8_t len) {
  if (!len) return;
  if (len <= sizeof(au_copy) / sizeof(au_copy[0])) {          // rovid: masolat (a hivo tombje lehet lokalis)
    for (uint8_t i = 0; i < len; i++) au_copy[i] = seq[i];
    seq = au_copy;
  }                                                           // hosszu: static const tomb kell!
  au_seq = seq; au_len = len; au_idx = 0; au_playing = true;
  audioStartNote(seq[0]);
}

template <size_t N>
inline void audioPlay(const Note (&arr)[N]) { audioPlay(arr, (uint8_t)N); }

inline void audioTone(uint16_t freq, uint16_t ms) {
  au_single = { freq, ms };
  audioPlay(&au_single, 1);
}

inline bool audioBusy() { return au_playing; }

inline void audioStop() {
  au_playing = false;
  audioIdleLevel();
}

// Alvas elott: mindket piezo labat egyszeru LOW kimenetre allitjuk
inline void audioShutdown() {
  audioStop();
  ledcDetach(PIN_BZ_A);
  pinMode(PIN_BZ_A, OUTPUT);
  digitalWrite(PIN_BZ_A, LOW);
  esp_rom_gpio_connect_out_signal(PIN_BZ_B, SIG_GPIO_OUT_IDX, false, false);
  pinMode(PIN_BZ_B, OUTPUT);
  digitalWrite(PIN_BZ_B, LOW);
}

// loop()-bol, minden korben
inline void audioUpdate() {
  if (!au_playing) return;
  if ((int32_t)(millis() - au_noteEnd) >= 0) {
    if (++au_idx >= au_len) audioStop();
    else audioStartNote(au_seq[au_idx]);
  }
}
