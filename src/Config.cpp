#include "Config.h"

// A fresh clone has no secrets.h yet: fall back to the example so the demo build still compiles.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static_assert(sizeof(PRINTERS) / sizeof(PRINTERS[0]) <= MAX_PRINTERS,
              "Too many printers in secrets.h; raise MAX_PRINTERS in Config.h");

namespace config {
size_t printerCount() { return sizeof(PRINTERS) / sizeof(PRINTERS[0]); }
const PrinterConfig& printer(size_t i) { return PRINTERS[i]; }
const char* wifiSsid() { return WIFI_SSID; }
const char* wifiPassword() { return WIFI_PASSWORD; }
}  // namespace config
