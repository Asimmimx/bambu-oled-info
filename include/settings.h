#pragma once
// Hardware pins and behaviour. Wi-Fi and printer credentials live in secrets.h instead.

// ---- Pins (ESP32-C3) -------------------------------------------------------------------
// Safe GPIOs: 0 1 3 4 5 6 7 10.
// Avoid: 2 8 9 (strapping), 11-17 (flash), 18 19 (USB), 20 21 (UART0 / USB-serial bridge).
#define PIN_OLED_SDA 4
#define PIN_OLED_SCL 5
#define PIN_ENC_CLK  6   // KY-040 "CLK" (A)
#define PIN_ENC_DT   7   // KY-040 "DT"  (B)
#define PIN_ENC_SW   10  // KY-040 "SW"  (push button)

// Flip if turning the knob clockwise moves backwards.
#define ENC_REVERSE 0
// Quadrature edges per detent. 4 for a standard KY-040; try 2 if one click skips a printer.
#define ENC_EDGES_PER_DETENT 4

// ---- Language of the on-screen labels: 0 = English, 1 = Turkish ------------------------
#define UI_LANG_TR 1

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

// After this long without turning the knob the display returns to the auto carousel.
#define MANUAL_TIMEOUT_MS     45000
// Turn the OLED off after this long when nothing is printing (OLED burn-in). 0 = never.
#define SCREEN_SLEEP_MS       300000

// Serial log: how often to print free heap.
#define HEARTBEAT_MS          10000

// Largest MQTT message accepted (bytes). Bigger messages are silently dropped by PubSubClient.
#define MQTT_BUFFER_BYTES     24576
