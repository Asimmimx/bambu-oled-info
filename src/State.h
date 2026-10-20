#pragma once
#include <Arduino.h>
#include <math.h>
#include "Config.h"

// Reachability of a printer, as last seen by the poller.
enum class Link : uint8_t { Unknown, Connecting, Online, Offline, AuthFailed };

// Simplified "gcode_state" of a printer.
enum class Phase : uint8_t { Unknown, Idle, Prepare, Running, Paused, Finished, Failed };

struct PrinterStatus {
  Link link = Link::Unknown;
  Phase phase = Phase::Unknown;
  int8_t percent = -1;        // 0..100, -1 unknown
  int32_t remainingMin = -1;  // mc_remaining_time, -1 unknown
  int32_t elapsedMin = -1;    // derived from percent + remaining, kept after the print ends
  int16_t layer = -1;
  int16_t totalLayers = -1;
  float nozzle = NAN, nozzleTarget = NAN;
  float bed = NAN, bedTarget = NAN;
  float chamber = NAN;
  int16_t wifiDbm = 0;        // 0 = unknown
  uint32_t printError = 0;    // print_error, 0 = none
  uint16_t hmsCount = 0;      // active HMS notices
  char file[40] = "";         // subtask_name (falls back to gcode_file)
  uint32_t updatedMs = 0;     // millis() of the last report, 0 = never

  bool hasData() const { return updatedMs != 0; }
  bool active() const { return phase == Phase::Running || phase == Phase::Paused || phase == Phase::Prepare; }
  bool needsAttention() const { return phase == Phase::Failed || printError != 0; }
};

namespace state {
void begin();

// Thread-safe copy of one printer's status.
PrinterStatus get(size_t idx);

// Writers (the poller) hold a Guard while touching raw().
struct Guard {
  Guard();
  ~Guard();
};
PrinterStatus& raw(size_t idx);

void setWifiUp(bool up);
bool wifiUp();
}  // namespace state
