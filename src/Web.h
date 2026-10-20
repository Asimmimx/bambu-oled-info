#pragma once

namespace web {

void begin();
void handle();

void setApActive(bool active);
bool apActive();

constexpr const char* kApSsid = "BambuMonitor-Setup";

}  // namespace web

