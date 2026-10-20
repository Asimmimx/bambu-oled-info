#pragma once
#include <stddef.h>
#include <stdint.h>
#include "settings.h"

constexpr size_t MAX_PRINTERS = 8;

// ip: the printer's LAN address, or "" for a printer reached through Bambu Cloud
// (e.g. one in another house). Cloud printers need BAMBU_CLOUD_USER / BAMBU_CLOUD_TOKEN.
struct PrinterConfig {
  char name[32];
  char shortName[12];
  char ip[32];
  char serial[24];
  char accessCode[16];
};

// Runtime-adjustable device, audio, display and input settings (stored in NVS).
struct DeviceSettings {
  // Buzzer & audio
  bool muted = false;
  bool startupChirp = true;
  bool buzzerPassive = (BUZZER_PASSIVE != 0);
  bool buzzerActiveLow = (BUZZER_ACTIVE_LOW != 0);
  uint8_t finishMelody = 0;  // 0..4 preset, 5 = customFinish
  uint8_t failedMelody = 0;  // 0..3 preset, 4 = customFailed
  char customFinish[96] = "1047:140,0:70,1319:140,0:70,1568:280";
  char customFailed[96] = "880:220,0:90,660:220,0:90,880:220,0:90,660:220";

  // Display & UI behaviour
  uint8_t oledContrast = 255;  // 1..255
  bool oledFlip = false;       // rotate 180 deg
  bool oledInvert = false;     // invert pixels
  uint8_t defaultView = 0;     // 0=Progress, 1=Overview, 2=Details, 3=Info
  bool autoCycle = true;
  bool autoJumpAlert = true;   // jump to printer when a print finishes or fails
  uint16_t carouselSec = CAROUSEL_MS / 1000;
  uint16_t manualTimeoutSec = MANUAL_TIMEOUT_MS / 1000;
  uint16_t doneAlertSec = DONE_ALERT_MS / 1000;
  uint16_t screenSleepSec = SCREEN_SLEEP_MS / 1000;

  // Knob & time
  bool encReverse = (ENC_REVERSE != 0);
  uint8_t encEdges = ENC_EDGES_PER_DETENT;
  char timezone[40] = TIMEZONE;
};

namespace config {
void begin();

size_t printerCount();
const PrinterConfig& printer(size_t i);
bool isCloud(size_t i);
const char* wifiSsid();
const char* wifiPassword();

bool cloudConfigured();  // user and token both present
const char* cloudHost();
const char* cloudUser();
const char* cloudToken();

DeviceSettings& settings();
bool saveSettings();

bool hasCustomConfig();
bool save(const char* ssid, const char* pass, const char* cHost, const char* cUser, const char* cToken,
          const PrinterConfig* printers, size_t count);
void resetToDefaults();
}  // namespace config
