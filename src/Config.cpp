#include "Config.h"
#include <string.h>

// A fresh clone has no secrets.h yet: fall back to the example so the demo build still compiles.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Bambu Cloud access is optional (only for printers whose ip is ""). Written by
// tools/fetch_access_codes.py --cloud.
#ifndef BAMBU_CLOUD_HOST
#define BAMBU_CLOUD_HOST "us.mqtt.bambulab.com"
#endif
#ifndef BAMBU_CLOUD_USER
#define BAMBU_CLOUD_USER ""
#endif
#ifndef BAMBU_CLOUD_TOKEN
#define BAMBU_CLOUD_TOKEN ""
#endif

static_assert(sizeof(PRINTERS) / sizeof(PRINTERS[0]) <= MAX_PRINTERS,
              "Too many printers in secrets.h; raise MAX_PRINTERS in Config.h");

namespace config {
size_t printerCount() { return sizeof(PRINTERS) / sizeof(PRINTERS[0]); }
const PrinterConfig& printer(size_t i) { return PRINTERS[i]; }
bool isCloud(size_t i) { return PRINTERS[i].ip[0] == '\0'; }
const char* wifiSsid() { return WIFI_SSID; }
const char* wifiPassword() { return WIFI_PASSWORD; }

bool cloudConfigured() { return strlen(BAMBU_CLOUD_USER) > 0 && strlen(BAMBU_CLOUD_TOKEN) > 0; }
const char* cloudHost() { return BAMBU_CLOUD_HOST; }
const char* cloudUser() { return BAMBU_CLOUD_USER; }
const char* cloudToken() { return BAMBU_CLOUD_TOKEN; }
}  // namespace config
