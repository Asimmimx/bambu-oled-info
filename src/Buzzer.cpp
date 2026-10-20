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

const Step kChirp[] = {{1047, 55, 65}, {0, 25, 0}, {1568, 80, 100}};

// ---- Finish Melodies (0..6, 7 = Custom) ----
// 0: Classic Rising (C6-E6-G6 with rising volume & tone 60% -> 80% -> 100%)
const Step kFinClassic[] = {
    {1047, 140, 60}, {0, 65, 0}, {1319, 140, 80}, {0, 65, 0}, {1568, 280, 100}};

// 1: Victory Fanfare (C6-E6-G6-C7 with dynamic accents)
const Step kFinFanfare[] = {
    {1047, 110, 58}, {0, 35, 0}, {1319, 110, 72}, {0, 35, 0},
    {1568, 110, 86}, {0, 45, 0}, {2093, 300, 100}, {0, 50, 0}, {1319, 150, 62}};

// 2: Soft-Loud Double Bell (gentle first chime, full-power second chime + echo)
const Step kFinDouble[] = {
    {1047, 115, 60}, {0, 70, 0}, {1760, 190, 100}, {0, 55, 0}, {1319, 140, 55}};

// 3: Crescendo Arpeggio (5-step rising staircase 55% -> 66% -> 78% -> 90% -> 100%)
const Step kFinArpeggio[] = {
    {784, 85, 55},  {0, 25, 0}, {1047, 85, 66}, {0, 25, 0}, {1319, 85, 78},
    {0, 25, 0},     {1568, 90, 90}, {0, 25, 0}, {2093, 250, 100}};

// 4: Wave Echo (swell up to full power and echo tail)
const Step kFinWave[] = {
    {784, 95, 55},   {0, 25, 0}, {1047, 100, 72}, {0, 25, 0}, {1319, 110, 86},
    {0, 25, 0},      {1760, 220, 100}, {0, 45, 0}, {1319, 115, 75}, {0, 35, 0},
    {988, 130, 55}};

// 5: Retro Game Stage Clear (rhythmic volume + pitch groove)
const Step kFinRetro[] = {
    {784, 90, 62},  {0, 30, 0}, {1047, 90, 76}, {0, 30, 0}, {1319, 90, 88},
    {0, 30, 0},     {1568, 160, 100}, {0, 60, 0}, {1175, 120, 68}, {0, 35, 0},
    {1568, 300, 100}};

// 6: Workshop Long Alert (3 strong 100% pulses)
const Step kFinWorkshop[] = {
    {1760, 300, 100}, {0, 100, 0}, {1760, 300, 100}, {0, 100, 0}, {1760, 460, 100}};

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
// 0: Classic Alternating Siren (Loud High / Medium Low)
const Step kErrSiren[] = {
    {1400, 210, 100}, {0, 75, 0}, {700, 210, 62}, {0, 75, 0},
    {1400, 210, 100}, {0, 75, 0}, {700, 230, 62}};

// 1: Urgent SOS (... --- ...)
const Step kErrSos[] = {
    {1047, 90, 75},  {0, 55, 0},  {1047, 90, 75},  {0, 55, 0},  {1047, 90, 75},  {0, 140, 0},
    {1568, 240, 100}, {0, 75, 0}, {1568, 240, 100}, {0, 75, 0}, {1568, 240, 100}, {0, 140, 0},
    {1047, 90, 75},  {0, 55, 0},  {1047, 90, 75},  {0, 55, 0},  {1047, 90, 75}};

// 2: Descending Sad Trombone (100% -> 84% -> 68% -> 55%)
const Step kErrSad[] = {
    {1175, 180, 100}, {0, 45, 0}, {988, 180, 82}, {0, 45, 0},
    {831, 180, 68},   {0, 45, 0}, {659, 380, 55}};

// 3: Heartbeat Warning (thump-THUMP ... thump-THUMP)
const Step kErrPulse[] = {
    {700, 95, 60},  {0, 45, 0},  {1500, 170, 100}, {0, 210, 0},
    {700, 95, 60},  {0, 45, 0},  {1500, 170, 100}, {0, 210, 0},
    {700, 95, 60},  {0, 45, 0},  {1500, 230, 100}};

// 4: Rapid Alarm (6 fast escalating beeps)
const Step kErrRapid[] = {
    {1000, 85, 65}, {0, 45, 0}, {1150, 85, 74}, {0, 45, 0}, {1300, 85, 82},  {0, 45, 0},
    {1450, 85, 90}, {0, 45, 0}, {1600, 85, 100}, {0, 45, 0}, {1760, 170, 100}};

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
  // If st.vol is 0 (not explicitly set in a custom pattern), derive a 55..100% contour from freq (400..2000 Hz)
  const uint32_t stepVol = st.vol > 0 ? constrain((int)st.vol, 25, 100)
                                      : (uint32_t)constrain(55 + ((int)st.freq - 500) * 45 / 1500, 50, 100);
  const uint32_t mode = ds.buzzerMode;

  if (mode == 1) {
    // 1 = Passive buzzer: real note frequency, PWM duty cycle (8..128)
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    ledcChangeFrequency(PIN_BUZZER, st.freq, 8);
    const uint32_t duty = 8 + (120 * effVol * effVol) / 10000;
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else if (mode == 2) {
    // 2 = Active buzzer (Volume + Pitch Melody):
    // An active buzzer's internal ~2.5 kHz oscillator needs >= 1.2 ms of ON time per pulse to ring loudly.
    // By modulating at a sub-harmonic (160..580 Hz, period 1.7..6.2 ms) with a high duty cycle (120..256),
    // every ON pulse is 1.4..3.5 ms of solid 3.3V power (so it is always loud!), while varying both
    // perceived volume and musical pitch across notes, and hitting 100% pure DC (256) on peak notes.
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    if (effVol >= 97) {
      ledcChangeFrequency(PIN_BUZZER, 500, 8);
      ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? 0 : 256);
    } else {
      const uint32_t modFreq = (uint32_t)constrain((int)st.freq / 3, 160, 580);
      ledcChangeFrequency(PIN_BUZZER, modFreq, 8);
      // Map effVol (15..96) -> duty (90..238) so t_ON is always >= 1.3 ms
      const uint32_t duty = 80 + (162 * effVol) / 100;
      ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
    }
  } else if (mode == 3) {
    // 3 = Active buzzer (Chiptune Multi-Tone):
    // Modulates every note (including 100% peak notes) at st.freq / 3 (150..650 Hz) with strong 115..232 duty
    // so every single note has a distinct 8-bit chiptune musical pitch at strong volume.
    const uint32_t effVol = (masterVol * stepVol + 50) / 100;
    const uint32_t modFreq = (uint32_t)constrain((int)st.freq / 3, 150, 650);
    ledcChangeFrequency(PIN_BUZZER, modFreq, 8);
    const uint32_t duty = 85 + (148 * effVol) / 100;
    ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
  } else {
    // 0 = Active buzzer (Classic flat): pure 100% DC at full volume, or 250 Hz long-pulse PWM when dimmed
    if (masterVol >= 98) {
      ledcChangeFrequency(PIN_BUZZER, 250, 8);
      ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? 0 : 256);
    } else {
      ledcChangeFrequency(PIN_BUZZER, 250, 8);
      const uint32_t duty = 75 + (175 * masterVol) / 100;
      ledcWrite(PIN_BUZZER, ds.buzzerActiveLow ? (256 - duty) : duty);
    }
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
  ledcAttach(PIN_BUZZER, 500, 8);
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
