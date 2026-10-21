# Detailed reference

[← English README](../README.md) · [Türkçe başvuru](reference.tr.md)

Pin settings, timing, architecture, helper tools and development notes. Follow the README for first-time setup.

## Configuration reference

### `include/settings.h`

| Setting | Default | Meaning |
|---|---|---|
| `PIN_OLED_SDA`, `PIN_OLED_SCL` | 4, 5 | I2C pins of the display |
| `PIN_ENC_CLK`, `PIN_ENC_DT`, `PIN_ENC_SW` | 6, 7, 10 | Knob pins (the defaults here are for the C3; other chips have their own) |
| `PIN_BUZZER` | 3 | Buzzer pin; `-1` = no buzzer |
| `BUZZER_PASSIVE` | 0 | 0 = active defaults, 1 = passive defaults; the web panel selects the sound mode |
| `BUZZER_ACTIVE_LOW` | 0 | 1 if the buzzer module sounds when the pin is LOW |
| `ENC_REVERSE` | 0 | Set to 1 if turning clockwise moves backwards |
| `ENC_EDGES_PER_DETENT` | 4 | Try 2 if one click of the knob skips a printer |
| `TIMEZONE` | `<+03>-3` | POSIX time zone for the finish clock, e.g. `CET-1CEST,M3.5.0,M10.5.0/3` |
| `CAROUSEL_MS` | 10000 | Time each printer stays on screen in auto mode |
| `MANUAL_TIMEOUT_MS` | 45000 | Auto mode resumes after this long without touching the knob |
| `DONE_ALERT_MS` | 300000 | A finished job flashes `DONE` this long, then the printer shows a steady `IDLE` |
| `SCREEN_SLEEP_MS` | 300000 | Screen off after this long with nothing printing; 0 = never |
| `CLOUD_POLL_MS`, `CLOUD_FINISH_POLL_MS` | 300000, 60000 | Normal / near-finish cloud check intervals |
| `LAN_POLL_MS`, `LAN_FINISH_POLL_MS` | 30000, 10000 | Normal / near-finish LAN check intervals |
| `FINISH_SOON_MIN` | 10 | Estimated minutes remaining to enter fast checks |
| `AUTO_LISTEN_MS` | 3500 | Bounded visit: listening time after a status report |
| `FIRST_REPORT_WAIT_MS` | 7000 | Give up on a printer that connects but stays silent |
| `CONNECT_TIMEOUT_S`, `HANDSHAKE_TIMEOUT_S` | 3, 8 | TCP and TLS timeouts |
| `OFFLINE_RETRY_MS` | 60000 | Retry interval for an unreachable printer or rejected credentials |
| `FOCUS_RETRY_MS` | 8000 | Retry interval while that printer is on screen |
| `PUSHALL_MIN_INTERVAL_MS` | 330000 | Normal full-status request interval per printer (5.5 min) |
| `FINISH_CONFIRM_MS` | 60000 | Full-status confirmation while a Running job’s last ETA has reached zero |
| `WARMUP_CLOUD_WAIT_MS` | 8000 | Power-up sweep: how long to wait for the cloud printers to report |
| `HEARTBEAT_MS` | 10000 | Interval of the status line in the serial log |
| `MQTT_BUFFER_BYTES` | 24576 | Largest accepted MQTT message; bigger ones are silently dropped |

### `include/secrets.h`

| Entry | Meaning |
|---|---|
| `WIFI_SSID`, `WIFI_PASSWORD` | Your Wi-Fi (2.4 GHz) |
| `PRINTERS[]` | One entry per printer: `{name, shortName, ip, serial, accessCode}`. `ip` = `""` means "through Bambu Cloud". At most 8 (`MAX_PRINTERS` in `src/Config.h`); the overview shows the first 6. |
| `BAMBU_CLOUD_USER`, `BAMBU_CLOUD_TOKEN`, `BAMBU_CLOUD_HOST` | Only for cloud printers; written by `fetch_access_codes.py --cloud` |

### `platformio.ini`

| Environment | Use |
|---|---|
| `esp32c3` | The real firmware |
| `demo` | Same UI with fake printers and no Wi-Fi |
| `esp32s3`, `esp32`, `esp32c6`, `esp32s2` | Other chips, build-tested only, see [Which ESP32 works?](../README.md#compatibility) |

For a board with **native USB** (no separate USB-UART chip), delete the two `-DARDUINO_USB_...` lines from `build_flags`. Other boards: change `board`. The platform is [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino core 3), and the flash is forced to 40 MHz DIO (see [Troubleshooting](../README.md#troubleshooting)).

## How it works

### The big picture

```
 Printers on the LAN ──┐                      ┌── Knob ─────► input ─┐
   (MQTT/TLS :8883)    ├─► poller task ─► state ◄── ui (loop) ◄─────┘
 Bambu Cloud ──────────┘   (one session)   (shared, locked)   │
   (MQTT/TLS :8883)                                           └──► OLED
```

Two tasks share a small, mutex-protected table with the last known status of every printer:

- The **poller** (`src/Poller.cpp`) owns the network. It talks to one MQTT broker at a time and writes what it learns into the table.
- The **UI** (`src/Ui.cpp`, in the Arduino `loop`) reads the table, draws the screens and handles the knob. The knob itself is decoded in an interrupt (`src/Input.cpp`).

### Why one connection at a time

The ESP32-C3 keeps **one MQTT/TLS session** at a time. The poller visits due printers independently of display selection, choosing the most overdue first. Visits are bounded, so manually focusing one printer cannot starve the others. Connections remain open between checks; after LAN visits the poller returns to the shared cloud session when available, receiving reports for all remote printers. This requires extra TLS connections in mixed LAN/cloud setups. With LAN-only configurations the last visited printer stays connected.

### The status protocol

A printer runs an MQTT broker on port 8883 (TLS). The device logs in with user `bblp` and the printer's **access code**, subscribes to `device/<serial>/report` and asks for a full status once by publishing `{"pushing":{"command":"pushall"}}` to `device/<serial>/request`. For cloud printers the broker is Bambu's, the user is `u_<your account id>` and the password is your token; the topics are the same. The protocol is documented by the community in [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI).

The fields used (all under `print`): `gcode_state`, `mc_percent`, `mc_remaining_time`, `layer_num`, `total_layer_num`, `nozzle_temper`, `nozzle_target_temper`, `bed_temper`, `bed_target_temper`, `chamber_temper`, `print_error`, `hms`, `wifi_signal`, `subtask_name` / `gcode_file`, `spd_lvl`, `spd_mag`, `stg_cur`, `cooling_fan_speed`, `nozzle_diameter`, `ams.tray_now`, `ams.ams[].tray[].tray_type` and `vt_tray.tray_type`. Each message is filtered while it is parsed, so a 9 KB report costs only a few hundred bytes of RAM.

### Details worth knowing

- **Partial reports.** Printers like the A1 only send fields that *changed*. The firmware merges them into the remembered state, and normally asks for a full picture (`pushall`) on the first visit and no more often than every **5.5 minutes** (with the zero-ETA confirmation exception below): Bambu advises against asking more often because the printer's CPU can lag.
- **Adaptive checks and limits.** Remaining time has its own timestamp, unaffected by temperature messages. Only running jobs count down into the final 10 estimated minutes; this estimate never declares a print finished. Cloud checks change from 5 min to 1 min, LAN from 30 s to 10 s. These target intervals start at visit completion; multiple printers, connection/NTP timeouts and Wi-Fi outages can add delay. **Full-report recovery:** normally `pushall` is limited to 330 s. Once the last ETA of a Running job reaches zero, the interval becomes 60 s until the printer reports its authoritative state. Paused, idle, finished and unknown-ETA jobs retain the normal interval. A one-off FINISH delta missed while disconnected may remain unknown until the next full report. FINISH on the open session triggers sound/display alerts immediately; an estimated zero remaining time cannot trigger an alert.
- **Power-up sweep.** Before the carousel starts, every printer is read once (LAN printers one by one, then one cloud session for all cloud printers), which takes about ten seconds for six printers.
- **Time elapsed is an estimate.** Printers report the time *left* and the percentage, not the time already spent. Elapsed time is `left × percent / (100 − percent)` for 1-99 %, so it is approximate, and unknown for a print that finished before the device started.
- **Finish clock.** `now + time left`, using NTP and your `TIMEZONE`.
- **Back-off.** Unreachable printers wait 60 s (8 s if selected when the attempt ends). Rejected credentials wait at least 60 s; display selection cannot reset the cooldown. A lost connection is recorded as offline.
- **TLS.** Printers use self-signed certificates on your LAN, which are accepted as they are. The cloud server's certificate is checked against the built-in Mozilla CA bundle before the token is sent.
- **Job names.** The name is the job name from `subtask_name` (or the file name from `gcode_file`). Jobs sent from Bambu Studio report the *project* name, which is the same for every plate of one project, so the plate number is added: `Organizer (plate 2)`.
- **File names** are converted to ASCII for the display font: Turkish letters are folded (ğ → g) and anything unsupported becomes `?`.
- **Memory.** The 45-second C3 startup check on 2026-10-21 reported about 94 KB free heap, with a minimum near 75 KB. This is a startup observation, not a long-run leak test. The firmware is about 1.3 MB in a 3 MB partition (no OTA).
- **Screen sleep.** If no printer is printing, paused, preparing or in error for 5 minutes, the screen turns off; a knob touch or new activity wakes it.
- **Boot diagnostics.** The log says where the display answered (`[oled] found at 0x3C ...`) and, if it is silent on the configured pins, scans all other pin pairs to tell you where it is wired.

## Project layout

```
platformio.ini            build environments (esp32c3, demo), platform and flash settings
include/settings.h        pins, timings, language, time zone
include/secrets.example.h template for your git-ignored secrets.h
src/main.cpp              setup + loop, periodic status line
src/Config.*              printer list and credentials (NVS + secrets.h defaults)
src/Web.*                 built-in HTTP configuration page and captive-portal AP
src/State.*               shared status table (mutex-protected)
src/Poller.*              MQTT sessions, parsing, back-off, cloud, demo data
src/Input.*               knob decoding (interrupts) and button (click / long / very long press)
src/Buzzer.*              optional buzzer: patterns for active and passive buzzers
src/Ui.*                  display, screens, carousel, toast, screen sleep
tools/find_printers.py    list printers on the LAN (SSDP)
tools/fetch_access_codes.py  read access codes / cloud login from your Bambu account
docs/wiring.svg           wiring diagram
```

## The helper tools

Both are plain Python 3 scripts with no dependencies.

**`tools/find_printers.py [--seconds 40]`** listens for the SSDP announcements of Bambu printers and prints name, model, serial number, IP, connection mode (e.g. `cloud`) and firmware. It sends nothing.

**`tools/fetch_access_codes.py`**

| Option | Effect |
|---|---|
| *(none)* | Log in with e-mail and password (asks for the e-mail code if needed) |
| `--token` | Paste the `token` cookie of makerworld.com instead of logging in |
| `--cloud` | Also store the cloud login in `secrets.h` and add the account's other printers as cloud printers |
| `--export-others` | Save printers that are not in `secrets.h` to `include/secrets.others.h` (git-ignored) |
| `--cn` | China-region account (the firmware's cloud mode is not tested for it) |
| `--secrets PATH` | Another `secrets.h` |

It makes a backup (`secrets.h.bak`), prints a table of your printers, and **never prints or stores your password or token** (with `--cloud` the token is written to `secrets.h` on purpose). It identifies itself honestly as `bambu-oled-info`; it does not pretend to be a Bambu app.

## Security and privacy

- **What lives where.** Wi-Fi password, access codes, serial numbers and the cloud token are only in `include/secrets.h` (git-ignored, with `.bak` copies and `secrets.*.h` also ignored) in compiled defaults and in the device’s flash / NVS when saved through the panel. Check `git status` before you push.
- **The cloud token is a password** to your Bambu account for about three months. With `--cloud` it sits in the flash of the device, so treat the device like a key: if you lose it or hand it on, change your Bambu password. LAN-only use does not need a token at all.
- **Read-only.** The only message the firmware publishes is the status request (`pushall`). It cannot control your printers.
- **LAN TLS is not verified.** Printers use self-signed certificates, so on your network a person with access to it could impersonate a printer. The device only sends the access code to the address in `secrets.h`.
- **Unofficial API.** The account tools and cloud mode use Bambu's undocumented cloud API. Bambu can change or restrict it at any time, and automated use of an account is something they may not appreciate. Use it for your own printers, at your own risk.

## Polling regression tests

Run with a host C++ compiler (keep the executable outside the repository):

```powershell
g++ -std=c++11 -Wall -Wextra -Werror tests/poll_schedule_test.cpp -o "$env:TEMP/bambu-poll-test.exe"
& "$env:TEMP/bambu-poll-test.exe"
```

Tests cover normal/fast intervals, approaching completion without new reports, unknown/paused time, focus starvation, authentication cooldown and `millis()` rollover. Actual FINISH delivery still needs a printer completion test on hardware.

## Contributing

Issues and pull requests are welcome. The most useful report is a **model that works or does not**: please include the printer model, firmware version, whether it was a LAN or cloud printer, and the serial log of one carousel cycle (remove your serial number and codes first). Test a change with `pio run -e esp32c3` and `pio run -e demo`; both must build without warnings.

## Credits and license

Unofficial and not affiliated with Bambu Lab. The protocol knowledge comes from the community project [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI). Libraries: [U8g2](https://github.com/olikraus/u8g2), [PubSubClient](https://github.com/knolleary/pubsubclient) and [ArduinoJson](https://arduinojson.org/); platform: [pioarduino](https://github.com/pioarduino/platform-espressif32).

Released under the [MIT License](../LICENSE).
