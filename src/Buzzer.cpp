#include "Buzzer.h"
#include <Arduino.h>
#include "settings.h"

namespace {

struct Step {
  uint16_t freq;  // Hz, 0 = silence. An active buzzer ignores the pitch and only uses the rhythm.
  uint16_t ms;
};

const Step kChirp[] = {{1800, 80}};
// Rising C-E-G: three beeps, the last one longer
const Step kFinished[] = {{1047, 140}, {0, 70}, {1319, 140}, {0, 70}, {1568, 280}};
// Alternating pitch, four beeps: unmistakably not "finished"
const Step kFailed[] = {{880, 220}, {0, 90}, {660, 220}, {0, 90}, {880, 220}, {0, 90}, {660, 220}};

struct Pattern {
  const Step* steps;
  uint8_t count;
};
const Pattern kPatterns[] = {
    {kChirp, sizeof kChirp / sizeof kChirp[0]},
    {kFinished, sizeof kFinished / sizeof kFinished[0]},
    {kFailed, sizeof kFailed / sizeof kFailed[0]},
};

volatile int8_t g_pending = -1;  // queued pattern, -1 = none
const Pattern* g_current = nullptr;
uint8_t g_step = 0;
uint32_t g_stepAt = 0;
bool g_muted = false;

void output(uint16_t freq) {
#if PIN_BUZZER >= 0
#if BUZZER_PASSIVE
  if (freq) tone(PIN_BUZZER, freq);
  else noTone(PIN_BUZZER);
#else
  const bool on = freq != 0;
  digitalWrite(PIN_BUZZER, (on != (BUZZER_ACTIVE_LOW != 0)) ? HIGH : LOW);
#endif
#else
  (void)freq;
#endif
}

}  // namespace

namespace buzzer {

bool available() { return PIN_BUZZER >= 0; }

void begin() {
#if PIN_BUZZER >= 0
  pinMode(PIN_BUZZER, OUTPUT);
  output(0);
  play(Sound::Chirp);  // proves the wiring at power-up
#endif
}

void play(Sound s) {
  if (!available() || g_muted) return;
  g_pending = (int8_t)s;
}

void update() {
#if PIN_BUZZER >= 0
  const uint32_t now = millis();
  if (g_pending >= 0) {
    g_current = &kPatterns[g_pending];
    g_pending = -1;
    g_step = 0;
    g_stepAt = now;
    output(g_current->steps[0].freq);
  }
  if (!g_current) return;
  if (now - g_stepAt < g_current->steps[g_step].ms) return;
  g_stepAt = now;
  if (++g_step >= g_current->count) {
    g_current = nullptr;
    output(0);
    return;
  }
  output(g_current->steps[g_step].freq);
#endif
}

void setMuted(bool m) {
  g_muted = m;
  if (m) {
    g_pending = -1;
    g_current = nullptr;
    output(0);
  }
}

bool muted() { return g_muted; }

}  // namespace buzzer
