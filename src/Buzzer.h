#pragma once

// Optional buzzer: short sounds for "print finished" and "print failed". Works with an ACTIVE buzzer
// (one fixed tone, so sounds differ by rhythm) and a PASSIVE one (real notes), configurable live.
namespace buzzer {

enum class Sound : unsigned char { Chirp, Finished, Failed };

void begin();

// Queue a sound (replaces one that is still playing). Safe to call from any task.
void play(Sound s);

// Play a sound for testing (ignores mute; optional preset index override).
void test(Sound s, int presetOverride = -1);

// Parse and play a custom pattern string "freq:ms,0:pause,..." (ignores mute).
void playCustom(const char* patternStr);

// Stop any currently playing sound immediately.
void stop();

// Call often from loop(); plays the queued sound without blocking.
void update();

void setMuted(bool muted);
bool muted();

// False when PIN_BUZZER is -1.
bool available();

}  // namespace buzzer
