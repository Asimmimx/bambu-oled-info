# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for an ESP32-C3 + SSD1306 128x32 OLED + KY-040 knob (+ optional buzzer) that shows the live status of up to 8 Bambu Lab printers. Public repo `Asimmimx/bambu-oled-info`. The maintainer writes in Turkish; code, commits, docs and all OLED text are English (no language switch; the web panel is English by default with a client-side Turkish switch: English text in the markup, Turkish in `data-tr`/`data-tr-ph` attributes and `T(en, tr)` in the JS, so a new web string needs both), and the README exists twice (`README.md`, `README.tr.md`): change both.

## Commands

There is no linter. Host regression tests are in `tests/poll_schedule_test.cpp`; compile with `g++ -std=c++11 -Wall -Wextra -Werror tests/poll_schedule_test.cpp -o "$env:TEMP/bambu-poll-test.exe"`, then execute that file. Both `esp32c3` and `demo` must build with **zero compiler warnings**. Distinguish build/host checks, board startup checks and an actual printer FINISH test; never claim completion latency was hardware-verified from compilation or startup alone.

```
pio run -e esp32c3                               # real firmware (needs include/secrets.h, else falls back to the example)
pio run -e esp32c3 -t upload --upload-port COMx  # flash
pio run -e demo -t upload                        # fake printers, no Wi-Fi: previews every screen and the buzzer
pio run -e esp32s3 | esp32 | esp32c6 | esp32s2   # build-only checks of the other chips
pio device monitor                               # 115200 baud
python tools/find_printers.py                    # SSDP scan of the LAN
python tools/fetch_access_codes.py [--token] [--cloud] [--export-others]
```

Run `pio` from PowerShell/cmd, **not Git Bash**: the pioarduino tool installer refuses MSys. If `pio` is not on PATH use `python -m platformio` (or `$env:USERPROFILE\.platformio\penv\Scripts\pio.exe`). On Windows PowerShell with Turkish locale (`cp1254`), set `$env:PYTHONUTF8 = "1"; $env:PYTHONIOENCODING = "utf-8"; chcp 65001 > $null` before running `pio -t upload` so `esptool` Unicode progress bars do not fail with `UnicodeEncodeError`.

## Architecture

Two tasks share one table. `Poller.cpp` owns the network in its own FreeRTOS task; `Ui.cpp` (screens, carousel), `Input.cpp` (knob ISR + button), `Buzzer.cpp` and `Web.cpp` (HTTP control panel on port 80) run in Arduino `loop()` (`main.cpp`). They meet in `State.cpp`: a mutex-protected `PrinterStatus[]`. Writers hold `state::Guard` and use `state::raw()`; readers call `state::get()` for a copy.

**One MQTT/TLS session at a time** is the central constraint (about 94 KB free heap in the latest startup check; six TLS sessions do not fit). `Poller.cpp` therefore has a single `PubSubClient g_mqtt` + `WiFiClientSecure* g_tls` and moves it:
- A LAN printer (`ip` set in `secrets.h`): `visitLocal()`, user `bblp`, access code as password, self-signed cert accepted (`setInsecure`).
- A cloud printer (`ip == ""`): `visitCloud()`, one session shared by *all* cloud printers (all their topics are subscribed at once; the retained transport lets subsequent cloud visits reuse it). The server certificate is verified with the built-in CA bundle, which needs NTP time first. Messages are routed to printers by topic (`printerForTopic`), never by "the printer currently visited".
- `openSession()` always closes the previous session first, so a local visit drops the cloud session.

**Scheduling.** The poller owns its schedule independently of the UI. `focus(idx)` is an atomic tie breaker among equally overdue printers, never an exclusive connection or a cooldown reset. `pickNext()` chooses the most overdue printer; every visit is bounded. Cloud checks default to 300 s / 60 s near finish, LAN to 30 s / 10 s. `PollSchedule.h` holds the pure, host-tested timing/selection policy. `remainingAtMs` timestamps the remaining-time field separately from `updatedMs`, so temperature-only deltas cannot postpone fast checks. Only Running jobs count down into `FINISH_SOON_MIN` (10 minutes); estimates never synthesize FINISH. Visit/retry/pushall timers use unsigned elapsed subtraction for rollover safety. Each cloud visit checks full-report eligibility for every cloud printer, including resumed sessions.

**Retained sessions.** A LAN session is reused for the same printer and held between visits; after LAN work the scheduler parks on the shared cloud session when its cooldown allows. The task pumps MQTT every 20 ms while idle and records connection loss as Offline. This improves live coverage at the cost of extra TLS handshakes in mixed setups. `warmup()` reads LAN printers and then all cloud printers before the carousel starts. `settle()` owns visit times and backoff; failed credentials wait at least 60 seconds. No UI task writes schedule arrays.

**Reading reports.** Printers (A1 family) send only *changed* fields, so `applyReport()` merges into the cached `PrinterStatus`; a full status is requested with `pushall` on the first visit and then no more often than every `PUSHALL_MIN_INTERVAL_MS` (330 s; fast checks do not bypass this because frequent full reports can lag the printer CPU). Messages are parsed through an ArduinoJson filter (`buildFilter`): to read a new field, add it to the filter, to `PrinterStatus` and to `applyReport`. There is deliberately no printer-model table; the model-agnostic policy is the conservative one. Only a successful publish advances the full-report timer. A missed FINISH delta during a disconnected period can still require the next full report; check intervals are not a latency guarantee. Elapsed time is not reported by printers and is estimated from percent and time left. Arduino Stream `setTimeout` and `setConnectionTimeout` take milliseconds; the latter controls the TCP/socket timeout and must be set separately. TLS `setHandshakeTimeout` takes seconds. The retained session services full-report timers while idle too, so 300-second visits do not round 330-second recovery up to 600 seconds.

**UI rules that span `Ui.cpp`.** The `View` enum order is the click order. Every view must cope with: no data yet (`!hasData()` gets the full-screen message), stale data (`isStale()`: last values stay, the tag shows `OFF`/`AUTH`, bars are dotted), and `justFinished()` (flashes the state tag / overview cell). Views never read `state::get()` directly but `snapshot()`, which turns a `Finished` that is older than `DONE_ALERT_MS` (or was never seen live: `finishedAtMs == 0`) into `Idle`. The OLED font has no `ş ı ğ İ`: file names go through `sanitizeName()` on ingest (`PrinterStatus::file` is composed by `composeFile()` from `subtask_name` + the plate number of `gcode_file`, because every plate of one Studio project reports the same `subtask_name`), printer names come straight from `secrets.h` (or NVS).

**Buzzer.** Alerts are raised in `applyReport()` from live phase changes only (never for a state found at power-up or during `warmup()`); `buzzer::update()` plays patterns non-blockingly from `loop()`. The demo task raises them too. Pending sound and preset are packed into one atomic mailbox; the UI alert mailbox uses atomic exchange so a new alert cannot be erased between reading and clearing it. `Buzzer.cpp` drives `PIN_BUZZER` via 8-bit LEDC PWM (`0..256` duty), supporting master volume (`1..100%`), polarity inversion (`Active-LOW`), 7 finish presets, 5 error presets, and custom melody strings (`freq:ms`, `freq:ms@vol`, `@vol:ms`). Four `buzzerMode` values are supported: `0` (active flat), `1` (passive square-wave pitch), `2` (active volume + pitch melody), and `3` (active chiptune). Because an active buzzer's internal ~2.5 kHz oscillator needs `>= 1.2 ms` of uninterrupted ON time per pulse to ring loudly, active modes modulate at a low sub-harmonic (`150..650 Hz`) with a high duty cycle (`75..256`) rather than multi-kHz PWM.

**Config & Web Control Panel.** `Config.cpp` is the only file that includes `secrets.h` (via `__has_include`, falling back to `secrets.example.h` so a fresh clone builds) as compile-time defaults, and overrides them with NVS (`Preferences`, namespace `bambu-cfg`). Both network/printer credentials (`save`) and live `DeviceSettings` (`saveSettings`: sound/melody, OLED contrast/flip/invert, carousel/sleep timers, encoder direction/edges, timezone) are persisted in NVS. `Web.cpp` serves a 4-tab control panel on HTTP port 80 (on the LAN IP or the fallback `BambuMonitor-Setup` SoftAP at `192.168.4.1`) with live endpoints (`/api/status`, `/api/config`, `/api/settings`, `/api/sound`, `/api/ui`, `/api/reset`). `settings.h` has per-chip default pins keyed on `CONFIG_IDF_TARGET_*`. `DEMO_MODE` replaces the poller task with `demoTask` (fake states) and skips the warm-up.

## Platform gotchas

- `platformio.ini` pins the pioarduino platform (Arduino core 3). The official PlatformIO platform ships a bootloader that boots **silently** into a loop on the maintainer's boards; the C3 env also forces `40 MHz` + `dio` flash (symptom of the 80 MHz default: `partition 0 invalid magic number`) and `huge_app.csv` (no OTA).
- The boards are CH343 USB-UART boards: `ARDUINO_USB_CDC_ON_BOOT=0` keeps `Serial` on UART0.
- Opening the serial port **resets the board**, and the COM number changes when the board is replugged.
- You cannot see the OLED. Check behaviour through the serial log: `[warmup]`, the per-printer `phase=... %` summary lines, `[ui] showing ...`, `[input] ...` events and the 10 s `[sys]` line (heap and knob pin levels).

## Credentials and docs

- Never ask for, read or handle the user's Bambu password or cloud token. `tools/fetch_access_codes.py` is written so the user runs it in their own terminal; ask them to run it and continue from the result.
- `include/secrets.h` and `include/secrets.*.h` hold real serials, names, codes and the cloud token. Examples in READMEs and comments must use made-up values (a real serial once slipped into a README draft); scan staged files against the real values before every push.
- The README "Status" table and the "Which ESP32 works?" table claim only what was actually run. At the time of writing that is the ESP32-C3, the SSD1306 and knob, three A1 mini printers on the LAN and three more through the cloud; the other chips are build-tested only and the buzzer has not been heard yet. Update both tables when that changes.
- Do not rewrite public history or force-push; add commits instead.

## Latest validation (2026-10-21)

Host polling regressions passed. All six PlatformIO environments built without compiler warnings. The real `esp32c3` firmware was flashed over COM6 with hash verification; a 45-second restart capture completed warmup, recorded six MQTT connections with no runtime fault markers, and `/api/status` returned all six configured printers (three LAN, three cloud). About 94 KB heap was free (minimum near 75 KB). This did not include an actual print finishing or an audible buzzer test.
