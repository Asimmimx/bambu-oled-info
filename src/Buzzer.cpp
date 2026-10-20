#include "Buzzer.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "settings.h"

namespace {

struct Step {
  uint16_t freq;  // Hz, 0 = silence
  uint16_t ms;    // duration in ms
  uint8_t vol;    // step volume 1..100% (0 = auto-derive from freq)
};

struct Pattern {
  const Step* steps;
  uint8_t count;
};

const Step kChirp[] = {{1400, 55, 55}, {0, 25, 0}, {1900, 75, 100}};

// ---- Finish Melodies (0..6, 7 = Custom) ----
// 0: Classic Rising (C6-E6-G6 with rising volume 40% -> 70% -> 100%)
const Step kFinClassic[] = {
    {1047, 140, 40}, {0, 65, 0}, {1319, 140, 70}, {0, 65, 0}, {1568, 280, 100}};

// 1: Victory Fanfare (C6-E6-G6-C7 with dynamic accents)
const Step kFinFanfare[] = {
    {1047, 105, 45}, {0, 35, 0}, {1319, 105, 65}, {0, 35, 0},
    {1568, 105, 85}, {0, 45, 0}, {2093, 300, 100}, {0, 50, 0}, {1568, 160, 40}};

// 2: Soft-Loud Double Bell (gentle first chime, bright second chime + soft echo)
const Step kFinDouble[] = {
    {1319, 110, 38}, {0, 70, 0}, {1976, 180, 100}, {0, 55, 0}, {1976, 140, 28}};

// 3: Crescendo Arpeggio (5-step volume wave 22% -> 40% -> 60% -> 82% -> 100%)
const Step kFinArpeggio[] = {
    {784, 80, 22},  {0, 25, 0}, {1047, 80, 40}, {0, 25, 0}, {1319, 80, 60},
    {0, 25, 0},     {1568, 85, 82}, {0, 25, 0}, {2093, 240, 100}};

// 4: Wave Echo (smooth swell up and fade-out tail — crafted for active buzzers)
const Step kFinWave[] = {
    {659, 90, 18},  {0, 20, 0}, {880, 95, 38},  {0, 20, 0}, {1175, 105, 65},
    {0, 20, 0},     {1568, 140, 92}, {0, 25, 0}, {1976, 200, 100}, {0, 45, 0},
    {1568, 110, 55}, {0, 35, 0}, {1175, 110, 30}, {0, 35, 0}, {880, 130, 15}};

// 5: Retro Game Stage Clear (rhythmic volume + pitch groove)
const Step kFinRetro[] = {
    {784, 90, 50},  {0, 30, 0}, {1047, 90, 70}, {0, 30, 0}, {1319, 90, 85},
    {0, 30, 0},     {1568, 150, 100}, {0, 60, 0}, {1319, 120, 55}, {0, 35, 0},
    {1568, 300, 100}};

// 6: Workshop Long Alert (3 strong pulses with echo)
const Step kFinWorkshop[] = {
    {1760, 280, 100}, {0, 90, 0}, {1760, 280, 100}, {0, 90, 0}, {1760, 420, 100}};

const Pattern kFinishPresets[] = {
    {kFinClassic, sizeof kFinClassic / sizeof kFinClassic[0]},
    {kFinFanfare, sizeof kFinFanfare / sizeof kFinFanfare[0]},
    {kFinDouble, sizeof kFinDouble / sizeof kFinDouble[0]},
    {kFinArpeggio, sizeof kFinArpeggio / sizeof kFinArpeggio[0]},
    {kFinWave, sizeof kFinWave / sizeof kFinWave[0]},
    {kFinRetro, sizeof kFinRetro / sizeof kFinRetro[0]},
    {kFinWorkshop, sizeof kFinWorkshop / sizeof kFinWorkshop[0]},
};

// ---- Failed / Error Melodies (0..4, 5 = Custom) ----
// 0: Classic Alternating Siren (Loud-Soft-Loud-Soft)
const Step kErrSiren[] = {
    {980, 200, 100}, {0, 75, 0}, {660, 200, 42}, {0, 75, 0},
    {980, 200, 100}, {0, 75, 0}, {660, 220, 42}};

// 1: Urgent SOS (... --- ...)
const Step kErrSos[] = {
    {1200, 85, 65},  {0, 55, 0},  {1200, 85, 65},  {0, 55, 0},  {1200, 85, 65},  {0, 140, 0},
    {1500, 230, 100}, {0, 75, 0}, {1500, 230, 100}, {0, 75, 0}, {1500, 230, 100}, {0, 140, 0},
    {1200, 85, 65},  {0, 55, 0},  {1200, 85, 65},  {0, 55, 0},  {1200, 85, 65}};

// 2: Descending Fade-Out (100% -> 75% -> 50% -> 25%)
const Step kErrSad[] = {
    {880, 170, 100}, {0, 45, 0}, {740, 170, 72}, {0, 45, 0},
    {622, 180, 48},  {0, 45, 0}, {523, 380, 24}};

// 3: Heartbeat Warning (thump-THUMP ... thump-THUMP)
const Step kErrPulse[] = {
    {600, 90, 38},  {0, 45, 0},  {1100, 160, 100}, {0, 220, 0},
    {600, 90, 38},  {0, 45, 0},  {1100, 160, 100}, {0, 220, 0},
    {600, 90, 38},  {0, 45, 0},  {1100, 220, 100}};

// 4: Rapid Alarm (6 fast escalating beeps)
const Step kErrRapid[] = {
    {1500, 80, 50}, {0, 45, 0}, {1500, 80, 65}, {0, 45, 0}, {1500, 80, 80},  {0, 45, 0},
    {1500, 80, 90}, {0, 45, 0}, {1500, 80, 100}, {0, 45, 0}, {1500, 160, 100}};

const Pattern kFailedPresets[] = {
    {kErrSiren, sizeof kErrSiren / sizeof kErrSiren[0]},
    {kErrSos, sizeof kErrSos / sizeof kErrSos[0]},
    {kErrSad, sizeof kErrSad / sizeof kErrSad[0]},
    {kErrPulse, sizeof kErrPulse / sizeof kErrPulse[0]},
    {kErrRapid, sizeof kErrRapid / sizeof kErrRapid[0]},
};

const Pattern kChirpPattern = {kChirp, sizeof kChirp / sizeof kChirp[0]};

constexpr uint8_t kMaxCustomSteps = 28;
Step g_customSteps[kMaxCustomSteps];
Pattern g_customPattern = {g_customSteps, 0};

volatile int8_t g_pendingSound = -1;   // Sound enum, or 10 for already-parsed g_customPattern
volatile int8_t g_pendingPreset = -1;  // -1 = use config::settings()
const Pattern* g_current = nullptr;
uint8_t g_step = 0;
uint32_t g_stepAt = 0;

void outputStep(const Step& st) {
#if PIN_BUZZER >= 0
  const DeviceSettings& ds = config::settings();
  if (st.freq == 0) {
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? 256 : 0);
    return;
  }

  const uint32_t masterVol = (uint32_t)constrain((int)ds.buzzerVolume, 1, 100);
  // If st.vol is 0 (not explicitly set in a custom pattern), derive a 28..100% contour from freq (400..2000 Hz)
  const uint32_t stepVol = st.vol > 0 ? constrain((int)st.vol, 1, 100)
                                      : (uint32_t)constrain(28 + ((int)st.freq - 400) * 72 / 1600, 25, 100);
  const uint32_t mode = ds.buzzerMode;

  if (mode == 1) {
    // 1 = Passive buzzer: real note frequency, quadratic PWM duty cycle (1..128)
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    ledcChangeFrequency(PIN_BUZZER, st.freq, 8);
    const uint32_t duty = 1 + (127 * effVol * effVol) / 10000;
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else if (mode == 2) {
    // 2 = Active buzzer (Volume Melody - Smooth): high 12 kHz PWM carrier so tone is pure,
    // while step volume creates clear musical dynamics (soft/medium/loud notes & echoes).
    // Map stepVol (15..100) strongly across the active buzzer's audible range (6..256).
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    ledcChangeFrequency(PIN_BUZZER, 12000, 8);
    const uint32_t duty = (effVol >= 99) ? 256 : (4 + (252 * effVol * effVol * effVol) / 1000000);
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else if (mode == 3) {
    // 3 = Active buzzer (Chiptune Pitch + Volume Modulation): chops the active buzzer at a sub-harmonic
    // of the note frequency while varying duty cycle, producing distinct multi-tone chiptune notes!
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    const uint32_t chopFreq = (uint32_t)constrain((int)st.freq / 2, 160, 1200);
    ledcChangeFrequency(PIN_BUZZER, chopFreq, 8);
    const uint32_t duty = 8 + (220 * effVol * effVol) / 10000;
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else {
    // 0 = Active buzzer (Classic flat): all notes at masterVol
    ledcChangeFrequency(PIN_BUZZER, 12000, 8);
    const uint32_t duty = (masterVol >= 99) ? 256 : (8 + (248 * masterVol * masterVol) / 10000);
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  }
#else
  (void)st;
#endif
}

// Supports:
//   "1047:140"        -> freq=1047, ms=140, vol=0 (auto-mapped from freq)
//   "1047:140@45"     -> freq=1047, ms=140, vol=45%
//   "@45:140"         -> freq=1000, ms=140, vol=45% (pure volume step)
//   "0:70"            -> pause 70 ms
bool parseCustomSteps(const char* str) {
  if (!str || !*str) return false;
  uint8_t count = 0;
  const char* p = str;
  while (*p && count < kMaxCustomSteps) {
    while (*p == ' ' || *p == ',' || *p == ';') p++;
    if (!*p) break;
    char* end = nullptr;
    long f = 1000;
    long v = 0;
    if (*p == '@') {
      p++;
      v = strtol(p, &end, 10);
      if (end == p || *end != ':') break;
      p = end + 1;
    } else {
      f = strtol(p, &end, 10);
      if (end == p || *end != ':') break;
      p = end + 1;
    }
    const long ms = strtol(p, &end, 10);
    if (end == p || ms <= 0) break;
    p = end;
    if (*p == '@') {
      p++;
      v = strtol(p, &end, 10);
      p = end;
    }
    g_customSteps[count].freq = (uint16_t)constrain(f, 0, 10000);
    g_customSteps[count].ms = (uint16_t)constrain(ms, 10, 5000);
    g_customSteps[count].vol = (uint8_t)constrain(v, 0, 100);
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
    if (p == 7 && parseCustomSteps(ds.customFinish)) return &g_customPattern;
    const int idx = constrain(p, 0, (int)(sizeof kFinishPresets / sizeof kFinishPresets[0]) - 1);
    return &kFinishPresets[idx];
  }
  const int p = presetOverride >= 0 ? presetOverride : (int)ds.failedMelody;
  if (p == 5 && parseCustomSteps(ds.customFailed)) return &g_customPattern;
  const int idx = constrain(p, 0, (int)(sizeof kFailedPresets / sizeof kFailedPresets[0]) - 1);
  return &kFailedPresets[idx];
}

}  // namespace

namespace buzzer {

bool available() { return PIN_BUZZER >= 0; }

void begin() {
#if PIN_BUZZER >= 0
  ledcAttach(PIN_BUZZER, 12000, 8);
  outputStep({0, 0, 0});
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
  outputStep({0, 0, 0});
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
      outputStep(g_current->steps[0]);
    } else {
      g_current = nullptr;
    }
  }
  if (!g_current) return;
  if (now - g_stepAt < g_current->steps[g_step].ms) return;
  g_stepAt = now;
  if (++g_step >= g_current->count) {
    g_current = nullptr;
    outputStep({0, 0, 0});
    return;
  }
  outputStep(g_current->steps[g_step]);
#endif
}

void setMuted(bool m) {
  config::settings().muted = m;
  config::saveSettings();
  if (m) stop();
}

bool muted() { return config::settings().muted; }

}  // namespace buzzer
