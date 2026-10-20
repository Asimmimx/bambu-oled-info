#pragma once
#include <stddef.h>

constexpr size_t MAX_PRINTERS = 8;

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
const char* wifiSsid();
const char* wifiPassword();
}  // namespace config
