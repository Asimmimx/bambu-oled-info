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
  int8_t speedLevel = 0;      // spd_lvl: 1 silent, 2 standard, 3 sport, 4 ludicrous, 0 unknown
  int16_t speedPct = -1;      // spd_mag, e.g. 100
  int8_t partFanPct = -1;     // cooling_fan_speed (0..15 on the printer) as percent
  int8_t auxFanPct = -1;      // big_fan1_speed
  int8_t stage = -1;          // stg_cur: what the printer is doing right now (see Ui.cpp), -1 none
  int16_t trayNow = -1;       // ams.tray_now: 0..15 AMS slot, 254 external spool, 255 none
  char trayType[16][8] = {};  // filament type per AMS slot ("PLA"...)
  char extType[8] = "";       // filament type of the external spool
  char nozzleDia[6] = "";     // "0.4"
  char file[40] = "";         // subtask_name (falls back to gcode_file)
  uint32_t updatedMs = 0;     // millis() of the last report, 0 = never

  bool hasData() const { return updatedMs != 0; }
  bool active() const { return phase == Phase::Running || phase == Phase::Paused || phase == Phase::Prepare; }
  bool needsAttention() const { return phase == Phase::Failed || printError != 0; }

  // Filament type in use right now ("PLA"), or "" when unknown / nothing loaded.
  const char* filament() const {
    if (trayNow >= 0 && trayNow < 16) return trayType[trayNow];
    if (trayNow == 254) return extType;
    return "";
  }
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
