#include "State.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
PrinterStatus g_status[MAX_PRINTERS];
SemaphoreHandle_t g_mutex = nullptr;
volatile bool g_wifiUp = false;
}  // namespace

namespace state {

void begin() { g_mutex = xSemaphoreCreateMutex(); }

PrinterStatus get(size_t idx) {
  Guard g;
  return g_status[idx % MAX_PRINTERS];
}

Guard::Guard() { xSemaphoreTake(g_mutex, portMAX_DELAY); }
Guard::~Guard() { xSemaphoreGive(g_mutex); }

PrinterStatus& raw(size_t idx) { return g_status[idx % MAX_PRINTERS]; }

void setWifiUp(bool up) { g_wifiUp = up; }
bool wifiUp() { return g_wifiUp; }

}  // namespace state
