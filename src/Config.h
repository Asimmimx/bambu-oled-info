#pragma once
#include <stddef.h>

constexpr size_t MAX_PRINTERS = 8;

// ip: the printer's LAN address, or "" for a printer reached through Bambu Cloud
// (e.g. one in another house). Cloud printers need BAMBU_CLOUD_USER / BAMBU_CLOUD_TOKEN.
struct PrinterConfig {
  const char* name;
  const char* shortName;
  const char* ip;
  const char* serial;
  const char* accessCode;
};

namespace config {
size_t printerCount();
const PrinterConfig& printer(size_t i);
bool isCloud(size_t i);
const char* wifiSsid();
const char* wifiPassword();

bool cloudConfigured();  // user and token both present
const char* cloudHost();
const char* cloudUser();
const char* cloudToken();
}  // namespace config
