#pragma once
#include <stdint.h>

// Pure timing policy; unsigned subtraction also works across millis() rollover.
namespace pollSchedule {
inline bool due(uint32_t now, uint32_t last, uint32_t interval) {
  return uint32_t(now - last) >= interval;
}

inline bool nearFinish(bool running, int32_t remainingMin, uint32_t remainingAt,
                       uint32_t now, uint32_t windowMin) {
  if (!running || remainingMin < 0) return false;
  return int64_t(remainingMin) * 60000 - uint32_t(now - remainingAt) <= int64_t(windowMin) * 60000;
}

inline uint32_t interval(bool cloud, bool near, uint32_t cloudNormal, uint32_t cloudNear,
                         uint32_t lanNormal, uint32_t lanNear) {
  return cloud ? (near ? cloudNear : cloudNormal) : (near ? lanNear : lanNormal);
}

struct Next {
  int index = -1;
  uint32_t overdue = 0;

  void consider(int candidate, bool visited, uint32_t now, uint32_t last,
                uint32_t intervalMs, int focus) {
    const uint32_t age = now - last;
    if (visited && age < intervalMs) return;
    const uint32_t late = visited ? age - intervalMs : UINT32_MAX;
    if (index < 0 || late > overdue || (late == overdue && candidate == focus)) {
      index = candidate;
      overdue = late;
    }
  }
};
}  // namespace pollSchedule
