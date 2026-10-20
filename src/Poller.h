#pragma once

// Talks to the printers over their local MQTT broker (TLS, port 8883, user "bblp").
//
// An ESP32-C3 cannot hold 6 TLS sessions at once, so a background task visits the printers one
// at a time: connect, subscribe, (maybe) ask for a full status, listen for a few seconds, leave.
// The last known state of every printer is kept in state::, so the overview always has data.
namespace poller {

void begin();

// Stay connected to one printer for live updates while you browse. -1 = go back to the carousel.
void focus(int idx);

// Index of the printer the poller is currently talking to (or last talked to), -1 before the first.
int visiting();

// At power-up the poller reads every printer once before anything is shown. True until that is done.
bool warmingUp();
void warmupProgress(int& done, int& total);

}  // namespace poller
