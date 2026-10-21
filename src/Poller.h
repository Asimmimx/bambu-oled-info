#pragma once

// Talks to local MQTT brokers (TLS :8883, user "bblp") or the shared Bambu Cloud broker.
//
// An ESP32-C3 cannot hold 6 TLS sessions at once, so a background task visits the printers one
// at a time: connect, subscribe, (maybe) ask for a full status, listen, retain until another is due.
// The last known state of every printer is kept in state::, so the overview always has data.
namespace poller {

void begin();

// Prefer this printer when background checks are equally overdue; never suspend other checks.
// -1 = no preference. The poller retains one session for live updates between scheduled checks.
void focus(int idx);

// Index of the printer the poller is currently talking to (or last talked to), -1 before the first.
int visiting();

// At power-up the poller reads every printer once before anything is shown. True until that is done.
bool warmingUp();
void warmupProgress(int& done, int& total);

}  // namespace poller
