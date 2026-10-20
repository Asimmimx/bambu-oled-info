#pragma once

namespace input {

// LongPress: held 0.8-2.5 s, reported on release. VeryLongPress: reported as soon as 2.5 s are reached.
enum class Event : unsigned char { None, Left, Right, Click, LongPress, VeryLongPress };

void begin();

// Call often (every few ms). Returns at most one event per call.
Event poll();

}  // namespace input
