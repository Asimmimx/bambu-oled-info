#include "Buzzer.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "settings.h"

namespace {

struct Step {
  uint16_t freq;  // Hz, 0 = silence. An active buzzer ignores the pitch and only uses the rhythm.
  uint16_t ms;
};

struct Pattern {
  const Step* steps;
  uint8_t count;
};

const Step kChirp[] = {{1800, 80}};

// ---- Finish Melodies (0..4) ----
// 0: Classic Rising (C6-E6-G6)
const Step kFinClassic[] = {{1047, 140}, {0, 70}, {1319, 140}, {0, 70}, {1568, 280}};
// 1: Victory Fanfare (C6-E6-G6-C7)
const Step kFinFanfare[] = {{1047, 110}, {0, 40}, {1319, 110}, {0, 40}, {1568, 110}, {0, 50}, {2093, 340}};
// 2: Polite Double Beep
const Step kFinDouble[] = {{1568, 100}, {0, 80}, {1976, 170}};
// 3: Cheerful Arpeggio (5 fast notes)
const Step kFinArpeggio[] = {{1047, 75},  {0, 25}, {1319, 75},  {0, 25}, {1568, 75},
                             {0, 25},     {1976, 75}, {0, 25}, {2093, 220}};
// 4: Workshop Long Alert (3 loud long beeps)
const Step kFinWorkshop[] = {{1760, 320}, {0, 120}, {1760, 320}, {0, 120}, {1760, 480}};

const Pattern kFinishPresets[] = {
    {kFinClassic, sizeof kFinClassic / sizeof kFinClassic[0]},
    {kFinFanfare, sizeof kFinFanfare / sizeof kFinFanfare[0]},
    {kFinDouble, sizeof kFinDouble / sizeof kFinDouble[0]},
    {kFinArpeggio, sizeof kFinArpeggio / sizeof kFinArpeggio[0]},
    {kFinWorkshop, sizeof kFinWorkshop / sizeof kFinWorkshop[0]},
};

// ---- Failed / Error Melodies (0..3) ----
// 0: Classic Siren (Alternating 880/660 Hz)
const Step kErrSiren[] = {{880, 220}, {0, 90}, {660, 220}, {0, 90}, {880, 220}, {0, 90}, {660, 220}};
// 1: Urgent SOS (... --- ...)
const Step kErrSos[] = {{1200, 90},  {0, 60},  {1200, 90},  {0, 60},  {1200, 90},  {0, 150},
                        {1200, 240}, {0, 80},  {1200, 240}, {0, 80},  {1200, 240}, {0, 150},
                        {1200, 90},  {0, 60},  {1200, 90},  {0, 60},  {1200, 90}};
// 2: Descending Sad Trombone (G5-F#5-F5-E5)
const Step kErrSad[] = {{784, 180}, {0, 50}, {740, 180}, {0, 50}, {698, 180}, {0, 50}, {659, 420}};
// 3: Rapid Alarm (6 fast beeps)
const Step kErrRapid[] = {{1500, 85}, {0, 50}, {1500, 85}, {0, 50}, {1500, 85}, {0, 50},
                          {1500, 85}, {0, 50}, {1500, 85}, {0, 50}, {1500, 150}};

const Pattern kFailedPresets[] = {
    {kErrSiren, sizeof kErrSiren / sizeof kErrSiren[0]},
    {kErrSos, sizeof kErrSos / sizeof kErrSos[0]},
    {kErrSad, sizeof kErrSad / sizeof kErrSad[0]},
    {kErrRapid, sizeof kErrRapid / sizeof kErrRapid[0]},
};

const Pattern kChirpPattern = {kChirp, sizeof kChirp / sizeof kChirp[0]};

constexpr uint8_t kMaxCustomSteps = 24;
Step g_customSteps[kMaxCustomSteps];
Pattern g_customPattern = {g_customSteps, 0};

volatile int8_t g_pendingSound = -1;   // Sound enum, or 10 for already-parsed g_customPattern
volatile int8_t g_pendingPreset = -1;  // -1 = use config::settings()
const Pattern* g_current = nullptr;
uint8_t g_step = 0;
uint32_t g_stepAt = 0;

void output(uint16_t freq) {
#if PIN_BUZZER >= 0
  const DeviceSettings& ds = config::settings();
  if (freq == 0) {
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? 256 : 0);
    return;
  }
  const uint32_t vol = (uint32_t)constrain((int)ds.buzzerVolume, 1, 100);
  if (ds.buzzerPassive) {
    ledcChangeFrequency(PIN_BUZZER, freq, 8);
    // 1..128 (50% duty cycle is maximum volume on a passive piezo/magnetic buzzer)
    const uint32_t duty = 1 + (127 * vol * vol) / 10000;
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else {
    ledcChangeFrequency(PIN_BUZZER, 4000, 8);
    const uint32_t duty = (vol >= 100) ? 256 : (16 + (240 * vol * vol) / 10000);
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  }
#else
  (void)freq;
#endif
}

bool parseCustomSteps(const char* str) {
  if (!str || !*str) return false;
  uint8_t count = 0;
  const char* p = str;
  while (*p && count < kMaxCustomSteps) {
    while (*p == ' ' || *p == ',' || *p == ';') p++;
    if (!*p) break;
    char* end = nullptr;
    const long f = strtol(p, &end, 10);
    if (end == p || *end != ':') break;
    p = end + 1;
    const long ms = strtol(p, &end, 10);
    if (end == p || ms <= 0) break;
    p = end;
    g_customSteps[count].freq = (uint16_t)constrain(f, 0, 10000);
    g_customSteps[count].ms = (uint16_t)constrain(ms, 10, 5000);
    count++;
  }
  g_customPattern.count = count;
  return count > 0;
}

const Pattern* resolvePattern(buzzer::Sound s, int presetOverride) {
  const DeviceSettings& ds = config::settings();
  if (s == buzzer::Sound::Chirp) return &kChirpPattern;
  if (s == buzzer::Sound::Finished) {
    const int p = presetOverride >= 0 ? presetOverride : (int)ds.finishMelody;
    if (p == 5 && parseCustomSteps(ds.customFinish)) return &g_customPattern;
    const int idx = constrain(p, 0, (int)(sizeof kFinishPresets / sizeof kFinishPresets[0]) - 1);
    return &kFinishPresets[idx];
  }
  const int p = presetOverride >= 0 ? presetOverride : (int)ds.failedMelody;
  if (p == 4 && parseCustomSteps(ds.customFailed)) return &g_customPattern;
  const int idx = constrain(p, 0, (int)(sizeof kFailedPresets / sizeof kFailedPresets[0]) - 1);
  return &kFailedPresets[idx];
}

}  // namespace

namespace buzzer {

bool available() { return PIN_BUZZER >= 0; }

void begin() {
#if PIN_BUZZER >= 0
  ledcAttach(PIN_BUZZER, 2000, 8);
  output(0);
  if (config::settings().startupChirp && !config::settings().muted) {
    play(Sound::Chirp);  // proves the wiring at power-up
  }
#endif
}

void play(Sound s) {
  if (!available() || config::settings().muted) return;
  g_pendingPreset = -1;
  g_pendingSound = (int8_t)s;
}

void test(Sound s, int presetOverride) {
  if (!available()) return;
  g_pendingPreset = (int8_t)presetOverride;
  g_pendingSound = (int8_t)s;
}

void playCustom(const char* patternStr) {
  if (!available()) return;
  stop();
  if (parseCustomSteps(patternStr)) {
    g_pendingSound = 10;
  }
}

void stop() {
  g_pendingSound = -1;
  g_current = nullptr;
  output(0);
}

void update() {
#if PIN_BUZZER >= 0
  const uint32_t now = millis();
  const int8_t pend = g_pendingSound;
  if (pend >= 0) {
    g_pendingSound = -1;
    if (pend == 10) {
      g_current = &g_customPattern;
    } else {
      g_current = resolvePattern((Sound)pend, g_pendingPreset);
    }
    if (g_current && g_current->count > 0) {
      g_step = 0;
      g_stepAt = now;
      output(g_current->steps[0].freq);
    } else {
      g_current = nullptr;
    }
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
  config::settings().muted = m;
  config::saveSettings();
  if (m) stop();
}

bool muted() { return config::settings().muted; }

}  // namespace buzzer
