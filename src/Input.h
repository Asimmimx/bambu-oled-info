#pragma once

namespace input {

enum class Event : unsigned char { None, Left, Right, Click, LongPress };

void begin();

// Call often (every few ms). Returns at most one event per call.
Event poll();

}  // namespace input
