# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for an ESP32-C3 + SSD1306 128x32 OLED + KY-040 knob (+ optional buzzer) that shows the live status of up to 8 Bambu Lab printers. Public repo `Asimmimx/bambu-oled-info`. The maintainer writes in Turkish; code, commits and docs are English, and the README exists twice (`README.md`, `README.tr.md`): change both.

## Commands

There are no tests and no linter. "Verified" means: both `esp32c3` and `demo` build with **zero warnings**, and the behaviour was checked on the board through the serial log.

```
pio run -e esp32c3                               # real firmware (needs include/secrets.h, else falls back to the example)
pio run -e esp32c3 -t upload --upload-port COMx  # flash
pio run -e demo -t upload                        # fake printers, no Wi-Fi: previews every screen and the buzzer
pio run -e esp32s3 | esp32 | esp32c6 | esp32s2   # build-only checks of the other chips
pio device monitor                               # 115200 baud
python tools/find_printers.py                    # SSDP scan of the LAN
python tools/fetch_access_codes.py [--token] [--cloud] [--export-others]
```

Run `pio` from PowerShell/cmd, **not Git Bash**: the pioarduino tool installer refuses MSys. If `pio` is not on PATH use `python -m platformio`.

## Architecture

Two tasks share one table. `Poller.cpp` owns the network in its own FreeRTOS task; `Ui.cpp` (screens, carousel), `Input.cpp` (knob ISR + button) and `Buzzer.cpp` run in Arduino `loop()` (`main.cpp`). They meet in `State.cpp`: a mutex-protected `PrinterStatus[]`. Writers hold `state::Guard` and use `state::raw()`; readers call `state::get()` for a copy.

**One MQTT/TLS session at a time** is the central constraint (about 100 KB of heap stays free; six TLS sessions do not fit). `Poller.cpp` therefore has a single `PubSubClient g_mqtt` + `WiFiClientSecure* g_tls` and moves it:
- A LAN printer (`ip` set in `secrets.h`): `visitLocal()`, user `bblp`, access code as password, self-signed cert accepted (`setInsecure`).
- A cloud printer (`ip == ""`): `visitCloud()`, one session shared by *all* cloud printers (all their topics are subscribed at once; `keep` lets the next cloud printer reuse it). The server certificate is verified with the built-in CA bundle, which needs NTP time first. Messages are routed to printers by topic (`printerForTopic`), never by "the printer currently visited".
- `openSession()` always closes the previous session first, so a local visit drops the cloud session.

**Who drives the poller.** The UI does: `poller::focus(idx)` = stay connected to that printer for live data; `-1` = round-robin (used by the Overview, which has no single printer). The carousel timer lives in `Ui.cpp`, not in the poller. `warmup()` runs once at power-up and reads every printer before the carousel starts (the UI shows a progress screen meanwhile). `settle()` turns a visit result into `Link` state plus back-off; a wrong access code (`AuthFailed`) is retried at most once a minute and `focus()` does not bring it forward.

**Reading reports.** Printers (A1 family) send only *changed* fields, so `applyReport()` merges into the cached `PrinterStatus`; a full status is requested with `pushall` on the first visit and then at most every `PUSHALL_MIN_INTERVAL_MS` (Bambu warns the printer CPU lags otherwise). Messages are parsed through an ArduinoJson filter (`buildFilter`): to read a new field, add it to the filter, to `PrinterStatus` and to `applyReport`. There is deliberately no printer-model table; the model-agnostic policy is the conservative one. Elapsed time is not reported by printers and is estimated from percent and time left.

**UI rules that span `Ui.cpp`.** The `View` enum order is the click order. Every view must cope with: no data yet (`!hasData()` gets the full-screen message), stale data (`isStale()`: last values stay, the tag shows `OFF`/`AUTH`, bars are dotted), and `justFinished()` (flashes the state tag / overview cell). The OLED font has no `ş ı ğ İ`: file names go through `sanitizeName()` on ingest, printer names come straight from `secrets.h`.

**Buzzer.** Alerts are raised in `applyReport()` from live phase changes only (never for a state found at power-up or during `warmup()`); `buzzer::update()` plays patterns non-blockingly from `loop()`. The demo task raises them too.

**Config.** `Config.cpp` is the only file that includes `secrets.h` (via `__has_include`, falling back to `secrets.example.h` so a fresh clone builds). `settings.h` has per-chip default pins keyed on `CONFIG_IDF_TARGET_*`. `DEMO_MODE` replaces the poller task with `demoTask` (fake states) and skips the warm-up.

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
