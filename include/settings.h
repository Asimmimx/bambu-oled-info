#pragma once
// Hardware pins and behaviour. Wi-Fi and printer credentials live in secrets.h instead.
#if __has_include(<sdkconfig.h>)
#include <sdkconfig.h>
#endif

// ---- Pins ------------------------------------------------------------------------------
// Defaults per chip, picked so they avoid the boot-strap, flash, USB and UART pins of that chip.
// Override any of them with a -D flag in platformio.ini, or just edit the PIN_ lines below.
#if defined(CONFIG_IDF_TARGET_ESP32)        // classic ESP32: GPIO 6-11 are the flash, 34-39 are input only
#define DEF_OLED_SDA 21
#define DEF_OLED_SCL 22
#define DEF_ENC_CLK  25
#define DEF_ENC_DT   26
#define DEF_ENC_SW   27
#define DEF_BUZZER   23
#elif defined(CONFIG_IDF_TARGET_ESP32S3)    // avoid 0 (boot), 19-20 (USB), 26-37 (flash / PSRAM), 43-44 (UART)
#define DEF_OLED_SDA 8
#define DEF_OLED_SCL 9
#define DEF_ENC_CLK  4
#define DEF_ENC_DT   5
#define DEF_ENC_SW   6
#define DEF_BUZZER   7
#elif defined(CONFIG_IDF_TARGET_ESP32S2)    // avoid 0 (boot), 19-20 (USB), 26-32 (flash), 45-46 (straps)
#define DEF_OLED_SDA 8
#define DEF_OLED_SCL 9
#define DEF_ENC_CLK  4
#define DEF_ENC_DT   5
#define DEF_ENC_SW   6
#define DEF_BUZZER   7
#elif defined(CONFIG_IDF_TARGET_ESP32C6)    // avoid 4, 5, 8, 9, 15 (straps), 12-13 (USB), 16-17 (UART)
#define DEF_OLED_SDA 6
#define DEF_OLED_SCL 7
#define DEF_ENC_CLK  0
#define DEF_ENC_DT   1
#define DEF_ENC_SW   2
#define DEF_BUZZER   3
#else                                       // ESP32-C3 (the tested chip): safe GPIOs are 0 1 3 4 5 6 7 10
#define DEF_OLED_SDA 4                      // avoid 2 8 9 (straps), 11-17 (flash), 18-19 (USB), 20-21 (UART)
#define DEF_OLED_SCL 5
#define DEF_ENC_CLK  6
#define DEF_ENC_DT   7
#define DEF_ENC_SW   10
#define DEF_BUZZER   3
#endif

#ifndef PIN_OLED_SDA
#define PIN_OLED_SDA DEF_OLED_SDA
#endif
#ifndef PIN_OLED_SCL
#define PIN_OLED_SCL DEF_OLED_SCL
#endif
#ifndef PIN_ENC_CLK
#define PIN_ENC_CLK DEF_ENC_CLK   // KY-040 "CLK" (A)
#endif
#ifndef PIN_ENC_DT
#define PIN_ENC_DT DEF_ENC_DT     // KY-040 "DT"  (B)
#endif
#ifndef PIN_ENC_SW
#define PIN_ENC_SW DEF_ENC_SW     // KY-040 "SW"  (push button)
#endif

// Flip if turning the knob clockwise moves backwards.
#define ENC_REVERSE 0
// Quadrature edges per detent. 4 for a standard KY-040; try 2 if one click skips a printer.
#define ENC_EDGES_PER_DETENT 4

// ---- Buzzer (optional) -----------------------------------------------------------------
// Sounds when a print finishes or fails, plus a short chirp at power-up. Hold the knob button for
// 2.5 s to mute / unmute. Set PIN_BUZZER to -1 if you have no buzzer.
#ifndef PIN_BUZZER
#define PIN_BUZZER DEF_BUZZER
#endif
// 0 = ACTIVE buzzer: it has its own oscillator, makes one fixed tone and can only be switched on / off,
//     so the sounds differ by rhythm. (If your buzzer plays a single note, this is the one.)
// 1 = PASSIVE buzzer: the ESP32 supplies the pitch, so it can play real notes.
#define BUZZER_PASSIVE 0
// 1 if your buzzer module sounds when the pin is LOW (many 3-pin modules with a transistor do).
#define BUZZER_ACTIVE_LOW 0

// ---- Time ------------------------------------------------------------------------------
// POSIX TZ string used for the "finishes at" clock. Turkey: "<+03>-3".
#define TIMEZONE "<+03>-3"

// ---- Carousel --------------------------------------------------------------------------
// Auto mode: each printer stays on screen this long, then the next one is shown.
#define CAROUSEL_MS           10000

// ---- Polling behaviour -----------------------------------------------------------------
// While a printer is on screen the poller stays connected to it (live updates). The overview
// screen has no single printer, so the poller visits them all in turn instead:
#define AUTO_LISTEN_MS        3500   // keep listening this long after the first full report
#define FIRST_REPORT_WAIT_MS  7000   // give up on a printer that connects but stays silent
#define CONNECT_TIMEOUT_S     3      // TCP connect timeout (powered-off printers)
#define HANDSHAKE_TIMEOUT_S   8      // TLS handshake timeout
#define OFFLINE_RETRY_MS      60000  // how often an unreachable printer is retried
#define FOCUS_RETRY_MS        8000   // ... while you are looking at it
#define FOCUS_SETTLE_MS       450    // wait for the knob to stop before connecting
// Bambu's own advice: do not send "pushall" more often than every 5 minutes (P1/A1 CPUs lag).
#define PUSHALL_MIN_INTERVAL_MS 330000

// At power-up every printer is read once before the carousel starts, so no screen says "waiting".
// A cloud session shares one wait for all cloud printers:
#define WARMUP_CLOUD_WAIT_MS  8000

// A finished job flashes "DONE" for this long, then the printer shows a steady "IDLE". A printer that
// was already finished when the device started shows "IDLE" right away.
#define DONE_ALERT_MS         300000

// After this long without turning the knob the display returns to the auto carousel.
#define MANUAL_TIMEOUT_MS     45000
// Turn the OLED off after this long when nothing is printing (OLED burn-in). 0 = never.
#define SCREEN_SLEEP_MS       300000

// Serial log: how often to print free heap and the knob pin levels.
#define HEARTBEAT_MS          10000

// Largest MQTT message accepted (bytes). Bigger messages are silently dropped by PubSubClient.
#define MQTT_BUFFER_BYTES     24576
