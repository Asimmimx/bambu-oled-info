# Bambu OLED Info

A tiny desk gadget that shows what your Bambu Lab printers are doing: **name, progress bar, time left, time elapsed, finish time, temperatures, layer and file name**. It is an ESP32-C3, a 0.91" OLED and a rotary knob. It talks to the printers directly over your local network, so there is no server, no app and no cloud in the loop.

*[Türkçe](README.tr.md)*

![Wiring diagram](docs/wiring.svg)

## Status

| | |
|---|---|
| Verified on hardware | ESP32-C3 (CH343 USB-UART board), SSD1306 128x32 OLED, 3x **A1 mini** (firmware 01.08.01.00) in normal cloud mode, no LAN Only Mode needed |
| Written, not yet verified on hardware | Rotary-encoder input, A1, P1/X1, H2 series, X2D, A2L |

Other models should work in principle (same local MQTT protocol), but I have not been able to test them. Reports are welcome.

## What you need

- An **ESP32-C3** dev board
- A **0.91" 128x32 SSD1306 I2C OLED** (pins GND, VCC, SCK/SCL, SDA)
- A **KY-040 rotary encoder** (optional but nice: it lets you pick a printer)
- A few jumper wires and a USB cable
- [PlatformIO](https://platformio.org/) (CLI or the VS Code extension)

## Wiring

| OLED | ESP32-C3 | | KY-040 | ESP32-C3 |
|---|---|---|---|---|
| GND | GND | | CLK | GPIO6 |
| VCC | 3V3 | | DT | GPIO7 |
| SCK (SCL) | GPIO5 | | SW | GPIO10 |
| SDA | GPIO4 | | + | 3V3 (not 5V) |
| | | | GND | GND |

Go by the pin *labels* on your modules, not their order. The pins can be changed in [`include/settings.h`](include/settings.h); avoid GPIO 2, 8, 9 (boot), 11-17 (flash), 18-21 (USB / UART).

## Quick start

```bash
git clone https://github.com/Asimmimx/bambu-oled-info
cd bambu-oled-info
cp include/secrets.example.h include/secrets.h      # Windows: copy ... ...
```

1. **Find your printers** (name, serial number, IP) with the helper, run from a computer on the same network:

   ```bash
   python tools/find_printers.py
   ```

2. **Edit `include/secrets.h`**: your Wi-Fi and one line per printer. This file is git-ignored.

3. **Get the printers' access codes.** Two ways:
   - On the printer: turn on *LAN Only Mode*; the code appears under the switch. Note that LAN Only Mode disconnects the printer from Bambu Cloud.
   - Or read them once from your Bambu account, without touching the printers (the script uses Bambu's unofficial cloud API):

     ```bash
     python tools/fetch_access_codes.py            # e-mail + password (+ e-mail code)
     python tools/fetch_access_codes.py --token    # or paste the "token" cookie of makerworld.com
     ```

     It writes the codes into `secrets.h` and keeps nothing else. Afterwards everything is local.

4. **Build and flash:**

   ```bash
   pio run -e esp32c3 -t upload
   ```

   On Windows use PowerShell or cmd, not Git Bash (the platform's tool installer refuses MSys).

No printers yet, or want to see the screens first? `pio run -e demo -t upload` shows fake printers cycling through every state.

## Using it

| Action | Result |
|---|---|
| Nothing | Each printer is shown for 10 s, then the next one |
| Turn the knob | Pick a printer; auto-cycling pauses for 45 s |
| Click | Next screen: **Progress** → **Details** → **Overview** |
| Long press | Auto-cycling on / off |

- **Progress**: name, state, bar with percentage, time left and elapsed.
- **Details**: layer, nozzle / bed / chamber temperatures, finish time, Wi-Fi signal and the (scrolling) file name.
- **Overview**: every printer at once as a small grid.
- A printer with an error flashes its header. If nothing is printing for 5 minutes the screen switches off (OLED burn-in); touching the knob or a new print wakes it.
- State tags: `RUN` `PAUSE` `PREP` `DONE` `IDLE` `ERR`, `OFF` (not reachable), `AUTH` (wrong access code).

## How it works

An ESP32-C3 cannot hold six TLS sessions at once. The firmware keeps **one** MQTT-over-TLS connection (port 8883, user `bblp`, the access code as password) and stays connected to the printer on screen, so it updates live. It remembers the last state of every printer, which is what the overview shows.

- Printers like the A1 only send *changes*, so the firmware merges them into the remembered state. It asks for a full status (`pushall`) on the first visit and then at most every 5.5 minutes, since Bambu advises against asking more often.
- The printers report time *left* but not time *elapsed*; elapsed is estimated from the progress, so it is approximate.
- The finish time needs NTP; set `TIMEZONE` in `settings.h`.
- Wrong access codes are retried at most once a minute.
- Memory stays flat: about 94 KB heap free at the lowest point while connected.

All tunables (carousel time, language `UI_LANG_TR`, timeouts, pins) are in [`include/settings.h`](include/settings.h). Up to 8 printers are supported (`MAX_PRINTERS`).

## Printers in more than one place

The device only sees the Wi-Fi it is on. For printers somewhere else, build a second unit with its own `secrets.h`.

## Troubleshooting

- **Board is completely silent / no serial output, even after flashing.** Many ESP32-C3 boards cannot read their flash at 80 MHz; the bootloader then loops on `partition 0 invalid magic number`. This repo already forces 40 MHz DIO and the current Arduino core in `platformio.ini`. If you change the platform, keep those settings.
- **Blank OLED.** Open the serial monitor (`pio device monitor`). The firmware prints `[oled] found at 0x3C ...`, or scans every other pin pair and tells you where the display answers.
- **`AUTH`** on a printer: wrong access code (or the printer does not accept local connections; try LAN Only Mode).
- **`OFF`**: the printer is off or not reachable from this network.
- Names are shown in a font without ş ı ğ İ; use plain letters.

## Disclaimer

Unofficial and not affiliated with Bambu Lab. The protocol is documented by the community ([OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)) and can change with any firmware update. The cloud script uses an undocumented API and may stop working. The firmware only *reads* printer status; it never sends commands other than the status request. TLS certificates of the printers are not verified (they are self-signed devices on your LAN).

## License

[MIT](LICENSE)
