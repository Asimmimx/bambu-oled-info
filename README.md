# Bambu OLED Info

A tiny desk gadget that shows what your Bambu Lab printers are doing: **name, progress bar, time left, time elapsed, finish time, temperatures, layer, file name, speed mode, filament and what the printer is doing right now**. It is an ESP32-C3, a 0.91" OLED and a rotary knob. Printers on your Wi-Fi are read directly over your local network; printers somewhere else (a second house, a workshop) can be read through your Bambu account.

*[Türkçe](README.tr.md)*

![Wiring diagram](docs/wiring.svg)

## Status

| | |
|---|---|
| Verified on hardware | ESP32-C3 (CH343 USB-UART board), SSD1306 128x32 OLED, KY-040 knob, 3x **A1 mini** (firmware 01.08.01.00) on the LAN in normal cloud mode, no LAN Only Mode needed; 3 more printers in another house (an A1, an X2D and an A2L, by their owner's account) read through Bambu Cloud |
| Not verified | P1 / X1 / H2 series and everything else |

Other models should work in principle (same MQTT protocol), but I have not been able to test them. Reports are welcome.

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

     It writes the codes into `secrets.h` and keeps nothing else.

4. **Build and flash:**

   ```bash
   pio run -e esp32c3 -t upload
   ```

   On Windows use PowerShell or cmd, not Git Bash (the platform's tool installer refuses MSys).

No printers yet, or want to see the screens first? `pio run -e demo -t upload` shows fake printers cycling through every state.

### Printers that are not on your Wi-Fi

A printer whose `ip` is `""` in `secrets.h` is read through Bambu Cloud instead. One command sets this up: it stores your cloud login in `secrets.h` and adds every printer of your account that is not in the file yet:

```bash
python tools/fetch_access_codes.py --token --cloud
```

- The device keeps **one** cloud connection for all of them, so switching between cloud printers is instant. The server certificate is verified against the built-in CA bundle before the token is sent; the clock is set over NTP first.
- The token is a password to your Bambu account and is compiled into the firmware. It is valid for about 3 months; run the command again and re-flash then. A cloud printer showing `AUTH` usually means the token expired.
- Global (non-China) accounts only; the cloud host is `BAMBU_CLOUD_HOST` (default `us.mqtt.bambulab.com`).

The alternative is a second unit next to those printers with its own `secrets.h`, which never needs the cloud.

## Using it

| Action | Result |
|---|---|
| Nothing | Each printer is shown for 10 s, then the next one |
| Turn the knob | Pick a printer; auto-cycling pauses for 45 s |
| Click | Next screen: **Progress** → **Details** → **Info** → **Overview** |
| Long press | Auto-cycling on / off (a message stays on screen for 3 s) |

- **Progress**: name, state, bar with percentage, time left and elapsed. While a printer is preparing it shows what it is doing instead (heating bed, leveling...).
- **Details**: layer, nozzle / bed / chamber temperatures, finish time, Wi-Fi signal and the (scrolling) file name.
- **Info**: speed mode and percentage, part-cooling fan, nozzle size, filament in use, current stage and HMS notices.
- **Overview**: every printer at once as a small grid.
- A printer with an error flashes its header. If nothing is printing for 5 minutes the screen switches off (OLED burn-in); touching the knob or a new print wakes it.
- State tags: `RUN` `PAUSE` `PREP` `DONE` `IDLE` `ERR`, `OFF` (not reachable), `AUTH` (wrong access code or expired token).

## How it works

An ESP32-C3 cannot hold six TLS sessions at once. The firmware keeps **one** MQTT-over-TLS session (port 8883) and stays connected to the printer on screen, so it updates live: user `bblp` with the access code for a printer on the LAN, your account for Bambu Cloud. It remembers the last state of every printer, which is what the overview shows.

- Printers like the A1 only send *changes*, so the firmware merges them into the remembered state. It asks for a full status (`pushall`) on the first visit and then at most every 5.5 minutes, since Bambu advises against asking more often.
- The printers report time *left* but not time *elapsed*; elapsed is estimated from the progress, so it is approximate (and unknown for a print that finished before the device started).
- The finish time needs NTP; set `TIMEZONE` in `settings.h`.
- Wrong access codes are retried at most once a minute.
- File names are shown in ASCII: Turkish letters are folded (ğ → g) and unsupported characters become `?`.
- Memory stays flat: about 100 KB heap free while connected.

All tunables (carousel time, language `UI_LANG_TR`, timeouts, pins) are in [`include/settings.h`](include/settings.h). Up to 8 printers are supported (`MAX_PRINTERS`).

## Troubleshooting

- **Board is completely silent / no serial output, even after flashing.** Many ESP32-C3 boards cannot read their flash at 80 MHz; the bootloader then loops on `partition 0 invalid magic number`. This repo already forces 40 MHz DIO and the current Arduino core in `platformio.ini`. If you change the platform, keep those settings.
- **Blank OLED.** Open the serial monitor (`pio device monitor`). The firmware prints `[oled] found at 0x3C ...`, or scans every other pin pair and tells you where the display answers.
- **`AUTH`** on a printer: wrong access code (or the printer does not accept local connections; try LAN Only Mode). On cloud printers: the token expired.
- **`OFF`**: the printer is off or not reachable from this network.
- **Knob or button does nothing.** The serial log prints `[input] left / right / click / long press` for every event, and the 10-second status line shows the pin levels (`clk= dt= sw=`; `sw` drops to 0 while the knob is pressed). A level that never changes means a wiring problem.
- Names you type in `secrets.h` are shown in a font without ş ı ğ İ; use plain letters.

## Disclaimer

Unofficial and not affiliated with Bambu Lab. The protocol is documented by the community ([OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)) and can change with any firmware update. The cloud script and cloud mode use an undocumented API and may stop working. The firmware only *reads* printer status; it never sends commands other than the status request. TLS certificates of printers on the LAN are not verified (they are self-signed devices on your network); the cloud server's certificate is.

## License

[MIT](LICENSE)
