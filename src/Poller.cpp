#include "Poller.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "Buzzer.h"
#include "Config.h"
#include "State.h"
#include "Ui.h"
#include "Web.h"
#include "settings.h"

namespace {

volatile int g_focus = -1;
volatile uint32_t g_focusSetMs = 0;
volatile int g_visiting = -1;

uint32_t g_retryAt[MAX_PRINTERS] = {};
uint32_t g_lastPushall[MAX_PRINTERS] = {};

PubSubClient g_mqtt;
JsonDocument g_filter;
WiFiClientSecure* g_tls = nullptr;  // transport of the current MQTT session, if any
bool g_cloudUp = false;             // the session is the Bambu Cloud one, shared by all cloud printers
volatile bool g_gotFull[MAX_PRINTERS] = {};  // a report with gcode_state arrived during this visit
uint32_t g_maxPayload = 0;
volatile bool g_warm = true;  // the initial sweep over all printers is still running
volatile int g_warmDone = 0;

const char kPushall[] =
    "{\"pushing\":{\"sequence_id\":\"1\",\"command\":\"pushall\",\"version\":1,\"push_target\":1}}";

// ---- Parsing ---------------------------------------------------------------------------

Phase parsePhase(const char* s) {
  if (!strcmp(s, "RUNNING")) return Phase::Running;
  if (!strcmp(s, "PAUSE")) return Phase::Paused;
  if (!strcmp(s, "IDLE")) return Phase::Idle;
  if (!strcmp(s, "FINISH")) return Phase::Finished;
  if (!strcmp(s, "FAILED")) return Phase::Failed;
  if (!strcmp(s, "PREPARE") || !strcmp(s, "SLICING") || !strcmp(s, "INIT")) return Phase::Prepare;
  return Phase::Unknown;
}

// The display font has no ş ğ ı İ Ş Ğ and cannot show other scripts. Fold those Turkish letters to
// ASCII and replace anything else outside Latin-1 with '?', so file names never show garbage.
void sanitizeName(const char* in, char* out, size_t n) {
  const uint8_t* s = (const uint8_t*)in;
  size_t o = 0;
  for (size_t i = 0; s[i] && o + 3 < n;) {
    if (s[i] < 0x80) {
      out[o++] = (char)s[i++];
    } else if ((s[i] & 0xE0) == 0xC0 && (s[i + 1] & 0xC0) == 0x80) {  // two-byte sequence
      const uint16_t cp = ((s[i] & 0x1F) << 6) | (s[i + 1] & 0x3F);
      i += 2;
      switch (cp) {
        case 0x11E: out[o++] = 'G'; break;
        case 0x11F: out[o++] = 'g'; break;
        case 0x130: out[o++] = 'I'; break;
        case 0x131: out[o++] = 'i'; break;
        case 0x15E: out[o++] = 'S'; break;
        case 0x15F: out[o++] = 's'; break;
        default:
          if (cp >= 0xA0 && cp <= 0xFF) {  // Latin-1 (ç ö ü ...): the font has these
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          } else {
            out[o++] = '?';
          }
      }
    } else {  // longer sequences or stray bytes
      i++;
      while ((s[i] & 0xC0) == 0x80) i++;
      out[o++] = '?';
    }
  }
  out[o] = '\0';
}

// Printers send only changed fields (A1/P1) or everything (X1): merge whatever is present.
void applyReport(int idx, JsonObjectConst p) {
  state::Guard guard;
  PrinterStatus& s = state::raw(idx);
  const bool hadData = s.updatedMs != 0;
  const Phase oldPhase = s.phase;
  const uint32_t oldError = s.printError;

  if (p["gcode_state"].is<const char*>()) {
    const Phase old = s.phase;
    s.phase = parsePhase(p["gcode_state"].as<const char*>());
    if (s.phase == Phase::Running && old != Phase::Running && old != Phase::Paused) s.elapsedMin = -1;
    g_gotFull[idx] = true;
  }
  if (p["mc_percent"].is<int>()) s.percent = (int8_t)constrain(p["mc_percent"].as<int>(), 0, 100);
  if (p["mc_remaining_time"].is<int>()) s.remainingMin = p["mc_remaining_time"].as<int>();
  if (p["layer_num"].is<int>()) s.layer = p["layer_num"].as<int>();
  if (p["total_layer_num"].is<int>()) s.totalLayers = p["total_layer_num"].as<int>();
  if (p["nozzle_temper"].is<float>()) s.nozzle = p["nozzle_temper"].as<float>();
  if (p["nozzle_target_temper"].is<float>()) s.nozzleTarget = p["nozzle_target_temper"].as<float>();
  if (p["bed_temper"].is<float>()) s.bed = p["bed_temper"].as<float>();
  if (p["bed_target_temper"].is<float>()) s.bedTarget = p["bed_target_temper"].as<float>();
  if (p["chamber_temper"].is<float>()) s.chamber = p["chamber_temper"].as<float>();
  if (p["print_error"].is<unsigned long>()) s.printError = p["print_error"].as<unsigned long>();
  if (p["hms"].is<JsonArrayConst>()) s.hmsCount = p["hms"].as<JsonArrayConst>().size();
  if (p["wifi_signal"].is<const char*>()) s.wifiDbm = atoi(p["wifi_signal"].as<const char*>());
  if (p["spd_lvl"].is<int>()) s.speedLevel = (int8_t)p["spd_lvl"].as<int>();
  if (p["spd_mag"].is<int>()) s.speedPct = (int16_t)p["spd_mag"].as<int>();
  if (p["stg_cur"].is<int>()) s.stage = (int8_t)p["stg_cur"].as<int>();
  // Fans are reported as a string "0".."15"; show them as a percentage
  if (p["cooling_fan_speed"].is<const char*>())
    s.partFanPct = (int8_t)constrain(atoi(p["cooling_fan_speed"].as<const char*>()) * 100 / 15, 0, 100);
  if (p["big_fan1_speed"].is<const char*>())
    s.auxFanPct = (int8_t)constrain(atoi(p["big_fan1_speed"].as<const char*>()) * 100 / 15, 0, 100);
  if (p["nozzle_diameter"].is<const char*>())
    strlcpy(s.nozzleDia, p["nozzle_diameter"].as<const char*>(), sizeof s.nozzleDia);

  // Filament: which slot is active, and the type loaded in each slot (AMS units of 4 trays each)
  JsonObjectConst ams = p["ams"];
  if (!ams.isNull()) {
    if (ams["tray_now"].is<const char*>()) s.trayNow = (int16_t)atoi(ams["tray_now"].as<const char*>());
    for (JsonObjectConst unit : ams["ams"].as<JsonArrayConst>()) {
      const int u = atoi(unit["id"] | "0");
      for (JsonObjectConst tray : unit["tray"].as<JsonArrayConst>()) {
        const int slot = u * 4 + atoi(tray["id"] | "0");
        if (slot >= 0 && slot < 16) strlcpy(s.trayType[slot], tray["tray_type"] | "", sizeof s.trayType[slot]);
      }
    }
  }
  if (p["vt_tray"]["tray_type"].is<const char*>())
    strlcpy(s.extType, p["vt_tray"]["tray_type"].as<const char*>(), sizeof s.extType);

  const char* name = p["subtask_name"].is<const char*>() ? p["subtask_name"].as<const char*>() : nullptr;
  const char* gcode = p["gcode_file"].is<const char*>() ? p["gcode_file"].as<const char*>() : nullptr;
  if (name && *name) sanitizeName(name, s.file, sizeof s.file);
  else if (gcode && *gcode) sanitizeName(gcode, s.file, sizeof s.file);

  // The printer reports the remaining time but not the elapsed time: estimate the latter from the
  // progress. Kept after the print ends so a finished job still shows how long it took.
  if ((s.phase == Phase::Running || s.phase == Phase::Paused) && s.percent >= 1 && s.percent <= 99 &&
      s.remainingMin >= 0) {
    s.elapsedMin = (int32_t)((int64_t)s.remainingMin * s.percent / (100 - s.percent));
  }

  // Alert (and remember when) for changes seen live, never for the state a printer was already in at power-up
  if (s.phase != Phase::Finished) s.finishedAtMs = 0;
  if (hadData && !g_warm) {
    const bool wasBusy = oldPhase == Phase::Running || oldPhase == Phase::Paused || oldPhase == Phase::Prepare;
    if (s.phase == Phase::Finished && wasBusy) {
      s.finishedAtMs = millis();  // the UI flashes DONE for DONE_ALERT_MS, then shows IDLE
      buzzer::play(buzzer::Sound::Finished);
      ui::notifyAlert(idx);
    } else if ((s.phase == Phase::Failed && oldPhase != Phase::Failed) || (s.printError != 0 && oldError == 0)) {
      buzzer::play(buzzer::Sound::Failed);
      ui::notifyAlert(idx);
    }
  }

  s.link = Link::Online;
  s.updatedMs = millis();
}

// "device/<serial>/report" -> index of the configured printer with that serial, or -1.
int printerForTopic(const char* topic) {
  if (strncmp(topic, "device/", 7) != 0) return -1;
  const char* sn = topic + 7;
  const char* end = strchr(sn, '/');
  const size_t len = end ? (size_t)(end - sn) : strlen(sn);
  for (size_t i = 0; i < config::printerCount(); i++) {
    const char* serial = config::printer(i).serial;
    if (strlen(serial) == len && !strncmp(sn, serial, len)) return (int)i;
  }
  return -1;
}

void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  const int idx = printerForTopic(topic);
  if (idx < 0) return;
  if (len > g_maxPayload) {
    g_maxPayload = len;
    Serial.printf("[mqtt] largest message so far: %u bytes (buffer %u)\n", len, MQTT_BUFFER_BYTES);
  }
  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, payload, len, DeserializationOption::Filter(g_filter));
  if (err) {
    Serial.printf("[mqtt] JSON error: %s\n", err.c_str());
    return;
  }
  JsonObjectConst p = doc["print"];
  if (!p.isNull()) applyReport(idx, p);
}

void buildFilter() {
  JsonObject p = g_filter["print"].to<JsonObject>();
  const char* keys[] = {"gcode_state",     "mc_percent",   "mc_remaining_time", "layer_num",
                        "total_layer_num", "nozzle_temper", "nozzle_target_temper",
                        "bed_temper",      "bed_target_temper", "chamber_temper", "print_error",
                        "hms",             "wifi_signal",  "subtask_name",      "gcode_file",
                        "spd_lvl",         "spd_mag",      "stg_cur",           "cooling_fan_speed",
                        "big_fan1_speed",  "nozzle_diameter"};
  for (const char* k : keys) p[k] = true;
  // filament: only the few AMS fields we read, not the whole (large) tray objects
  p["ams"]["tray_now"] = true;
  p["ams"]["ams"][0]["id"] = true;
  p["ams"]["ams"][0]["tray"][0]["id"] = true;
  p["ams"]["ams"][0]["tray"][0]["tray_type"] = true;
  p["vt_tray"]["tray_type"] = true;
}

// ---- Connection handling ---------------------------------------------------------------

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (web::apActive() && WiFi.softAPgetStationNum() == 0) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      web::setApActive(false);
      Serial.println("[wifi] STA connected, setup AP closed");
    }
    state::setWifiUp(true);
    return;
  }
  state::setWifiUp(false);

  // Do not hop channels while a user is connected to the setup AP
  if (web::apActive() && WiFi.softAPgetStationNum() > 0) return;

  const char* ssid = config::wifiSsid();
  if (*ssid && strcmp(ssid, "your-wifi-name") != 0) {
    Serial.printf("[wifi] connecting to %s\n", ssid);
    WiFi.disconnect();
    WiFi.begin(ssid, config::wifiPassword());
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) vTaskDelay(pdMS_TO_TICKS(200));
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[wifi] up, ip %s (web: http://%s), rssi %d\n", WiFi.localIP().toString().c_str(),
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      configTzTime(config::settings().timezone, "pool.ntp.org", "time.cloudflare.com");
      state::setWifiUp(true);
      return;
    }
    Serial.println("[wifi] failed, will retry");
  }

  if (!web::apActive()) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(web::kApSsid);
    web::setApActive(true);
    Serial.printf("[wifi] setup AP started: SSID=\"%s\" url=http://%s\n", web::kApSsid,
                  WiFi.softAPIP().toString().c_str());
  }
}

enum class Visit : uint8_t { Ok, Offline, AuthFailed };

void closeSession() {
  if (g_tls) {
    g_mqtt.disconnect();
    g_tls->stop();
    delete g_tls;
    g_tls = nullptr;
  }
  g_cloudUp = false;
}

// Opens an MQTT-over-TLS session. A printer on the LAN has a self-signed certificate, which is
// accepted as is. Bambu Cloud has a normal certificate: it is verified against the built-in CA
// bundle before the account token is sent.
Visit openSession(const char* host, const char* user, const char* pass, bool verify, const char* label) {
  closeSession();
  g_tls = new WiFiClientSecure();
  if (verify) g_tls->useBuiltinCACertBundle();
  else g_tls->setInsecure();
  g_tls->setTimeout(CONNECT_TIMEOUT_S);
  g_tls->setHandshakeTimeout(HANDSHAKE_TIMEOUT_S);
  g_mqtt.setClient(*g_tls);
  g_mqtt.setServer(host, 8883);

  char clientId[32];
  snprintf(clientId, sizeof clientId, "c3-monitor-%08x", (unsigned)ESP.getEfuseMac());

  const uint32_t t0 = millis();
  if (!g_mqtt.connect(clientId, user, pass)) {
    const int st = g_mqtt.state();
    char tlsErr[100] = "";
    g_tls->lastError(tlsErr, sizeof tlsErr);
    Serial.printf("[%s] connect failed (mqtt state %d%s%s) after %lu ms\n", label, st, *tlsErr ? ", tls: " : "",
                  tlsErr, millis() - t0);
    closeSession();
    // 4 = bad credentials, 5 = not authorized
    return (st == 4 || st == 5) ? Visit::AuthFailed : Visit::Offline;
  }
  Serial.printf("[%s] connected in %lu ms, free heap %u (largest block %u)\n", label, millis() - t0,
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  return Visit::Ok;
}

// A1/P1 printers only send changes; one "pushall" gives the full picture. It is rate limited.
// Returns true if a full report was requested.
bool requestFullIfDue(int idx) {
  const PrinterConfig& pc = config::printer(idx);
  const uint32_t now = millis();
  if (g_lastPushall[idx] != 0 && now - g_lastPushall[idx] <= PUSHALL_MIN_INTERVAL_MS) return false;
  char requestTopic[64];
  snprintf(requestTopic, sizeof requestTopic, "device/%s/request", pc.serial);
  g_lastPushall[idx] = now ? now : 1;
  g_mqtt.publish(requestTopic, kPushall);
  Serial.printf("[%s] pushall sent\n", pc.name);
  return true;
}

void subscribePrinter(int idx) {
  char reportTopic[64];
  snprintf(reportTopic, sizeof reportTopic, "device/%s/report", config::printer(idx).serial);
  g_mqtt.subscribe(reportTopic);
}

void logSummary(int idx) {
  const PrinterStatus s = state::get(idx);
  const char* name = config::printer(idx).name;
  Serial.printf("[%s] phase=%d %d%% left=%ldmin elapsed=%ldmin layer=%d/%d nozzle=%.0f/%.0f bed=%.0f/%.0f "
                "err=%lu wifi=%ddBm file=\"%s\"\n",
                name, (int)s.phase, s.percent, (long)s.remainingMin, (long)s.elapsedMin, s.layer, s.totalLayers,
                s.nozzle, s.nozzleTarget, s.bed, s.bedTarget, (unsigned long)s.printError, s.wifiDbm, s.file);
  Serial.printf("[%s]   speed=%d(%d%%) fan=%d%% stage=%d nozzle_dia=%s tray_now=%d filament=\"%s\" hms=%u\n", name,
                s.speedLevel, s.speedPct, s.partFanPct, s.stage, s.nozzleDia, s.trayNow, s.filament(), s.hmsCount);
}

// A printer on the LAN. In manual focus mode it stays connected until the focus moves elsewhere.
Visit visitLocal(int idx) {
  const PrinterConfig& pc = config::printer(idx);
  g_visiting = idx;
  g_gotFull[idx] = false;
  {
    state::Guard guard;
    state::raw(idx).link = Link::Connecting;
  }

  const Visit r = openSession(pc.ip, "bblp", pc.accessCode, false, pc.name);
  if (r != Visit::Ok) return r;

  subscribePrinter(idx);
  const bool expectFull = requestFullIfDue(idx) || !state::get(idx).hasData();

  // Without a pushall an idle A1/P1 may legitimately stay silent: then only listen briefly for
  // changes instead of waiting for a full report that is not coming.
  const uint32_t started = millis();
  uint32_t fullAt = expectFull ? 0 : started;
  while (g_mqtt.connected()) {
    g_mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(5));

    const uint32_t t = millis();
    if (g_gotFull[idx] && !fullAt) fullAt = t;
    if (g_warm) {  // initial sweep: leave as soon as the state is known
      if (fullAt || t - started > FIRST_REPORT_WAIT_MS) break;
      continue;
    }
    const int focus = g_focus;
    if (focus >= 0) {
      if (focus != idx) break;  // the knob moved on
    } else {
      if (fullAt && t - fullAt > AUTO_LISTEN_MS) break;
      if (!fullAt && t - started > FIRST_REPORT_WAIT_MS) break;
    }
  }

  logSummary(idx);
  if (!g_gotFull[idx] && expectFull) Serial.printf("[%s] connected but no full report received\n", pc.name);
  closeSession();
  // Reachable counts as online even when silent; the UI shows "waiting for data" until a report lands.
  return Visit::Ok;
}

// A printer reached through Bambu Cloud. One session serves every cloud printer: all their report
// topics are subscribed at once, so moving between cloud printers costs no reconnect.
Visit visitCloud(int idx) {
  const PrinterConfig& pc = config::printer(idx);
  const int n = (int)config::printerCount();
  g_visiting = idx;
  g_gotFull[idx] = false;

  if (!config::cloudConfigured()) {
    Serial.printf("[%s] cloud printer, but BAMBU_CLOUD_USER / BAMBU_CLOUD_TOKEN are missing in secrets.h\n", pc.name);
    return Visit::AuthFailed;
  }

  const bool resumed = g_cloudUp && g_mqtt.connected();
  bool expectFull = !state::get(idx).hasData();
  if (!resumed) {
    // Checking the server certificate needs a valid clock (NTP)
    for (int i = 0; i < 100 && time(nullptr) < 1700000000; i++) vTaskDelay(pdMS_TO_TICKS(100));
    if (time(nullptr) < 1700000000) {
      Serial.println("[cloud] clock not set yet (NTP), trying again later");
      return Visit::Offline;
    }
    {
      state::Guard guard;
      for (int i = 0; i < n; i++)
        if (config::isCloud(i)) state::raw(i).link = Link::Connecting;
    }
    const Visit r = openSession(config::cloudHost(), config::cloudUser(), config::cloudToken(), true, "cloud");
    if (r != Visit::Ok) return r;
    g_cloudUp = true;
    for (int i = 0; i < n; i++) {
      if (!config::isCloud(i)) continue;
      g_gotFull[i] = false;
      subscribePrinter(i);
      const bool asked = requestFullIfDue(i);
      if (i == idx && asked) expectFull = true;
    }
  } else if (requestFullIfDue(idx)) {
    expectFull = true;
  }

  const uint32_t started = millis();
  uint32_t fullAt = expectFull ? 0 : started;
  // In the carousel-free overview mode there is nothing to wait for once the session is up
  const bool quick = resumed && g_focus < 0 && !expectFull;
  bool keep = true;  // keep the session for the next cloud printer
  bool lost = false;
  for (;;) {
    if (!g_mqtt.connected()) {
      lost = true;
      break;
    }
    g_mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(5));

    const uint32_t t = millis();
    if (g_gotFull[idx] && !fullAt) fullAt = t;
    if (g_warm) {  // initial sweep: done once every cloud printer has reported, or the wait runs out
      bool all = true;
      for (int i = 0; i < n; i++)
        if (config::isCloud(i) && !g_gotFull[i]) all = false;
      if (all || t - started > WARMUP_CLOUD_WAIT_MS) break;
      continue;
    }
    if (quick) {
      if (t - started > 200) break;
      continue;
    }
    const int focus = g_focus;
    if (focus >= 0) {
      if (focus != idx) {
        keep = config::isCloud(focus);  // another cloud printer is served by the same session
        break;
      }
    } else {
      if (fullAt && t - fullAt > AUTO_LISTEN_MS) break;
      if (!fullAt && t - started > FIRST_REPORT_WAIT_MS) break;
    }
  }

  if (lost) {
    Serial.println("[cloud] connection lost");
    closeSession();
    return Visit::Offline;
  }
  if (g_warm) {
    for (int i = 0; i < n; i++)
      if (config::isCloud(i)) logSummary(i);
  } else if (!quick) {
    logSummary(idx);
  }
  if (!keep) closeSession();
  return Visit::Ok;
}

Visit visit(int idx) { return config::isCloud(idx) ? visitCloud(idx) : visitLocal(idx); }

// The next printer to talk to, or -1 if nobody is due yet.
int pickNext(int last) {
  const uint32_t now = millis();
  const int n = (int)config::printerCount();
  if (n <= 0) return -1;
  const int focus = g_focus;
  if (focus >= 0 && focus < n) {
    if (now - g_focusSetMs < FOCUS_SETTLE_MS) return -1;  // let the knob settle
    return now >= g_retryAt[focus] ? focus : -1;
  }
  for (int k = 1; k <= n; k++) {
    const int i = (last + k) % n;
    if (now >= g_retryAt[i]) return i;
  }
  return -1;
}

// Records the outcome of a visit. A cloud result applies to every cloud printer, a local one to that
// printer only.
void settle(int idx, Visit r) {
  const int n = (int)config::printerCount();
  const bool cloud = config::isCloud(idx);
  const uint32_t now = millis();
  for (int i = 0; i < n; i++) {
    if (cloud ? !config::isCloud(i) : i != idx) continue;
    {
      state::Guard guard;
      PrinterStatus& s = state::raw(i);
      if (r == Visit::Ok) s.link = Link::Online;
      else s.link = (r == Visit::AuthFailed) ? Link::AuthFailed : Link::Offline;
    }
    if (r == Visit::Ok) g_retryAt[i] = 0;
    else if (r == Visit::AuthFailed) g_retryAt[i] = now + OFFLINE_RETRY_MS;  // wrong code: do not hammer
    else g_retryAt[i] = now + (g_focus == idx ? FOCUS_RETRY_MS : OFFLINE_RETRY_MS);
  }
}

// Power-up: read every printer once, so the first thing on screen is real data. Printers on the LAN
// first, then one visit that fills in all cloud printers.
void warmup() {
  const int n = (int)config::printerCount();
  Serial.println("[warmup] reading every printer once");
  int firstCloud = -1, cloudCount = 0;
  for (int i = 0; i < n; i++) {
    if (config::isCloud(i)) {
      if (firstCloud < 0) firstCloud = i;
      cloudCount++;
      continue;
    }
    settle(i, visit(i));
    g_warmDone = g_warmDone + 1;
  }
  if (firstCloud >= 0) {
    settle(firstCloud, visit(firstCloud));
    g_warmDone = g_warmDone + cloudCount;
  }
  closeSession();
  g_warm = false;
  Serial.println("[warmup] done");
}

void task(void*) {
  buildFilter();
  g_mqtt.setBufferSize(MQTT_BUFFER_BYTES);
  g_mqtt.setCallback(onMessage);
  g_mqtt.setKeepAlive(30);
  g_mqtt.setSocketTimeout(5);

  int last = -1;
  for (;;) {
    ensureWifi();
    if (!state::wifiUp()) {
      closeSession();
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (g_warm) {
      warmup();
      continue;
    }
    const int idx = pickNext(last);
    if (idx < 0) {
      // keep a held-open cloud session alive (pings, incoming reports) while there is nothing to visit
      if (g_tls && g_mqtt.connected()) g_mqtt.loop();
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    last = idx;
    settle(idx, visit(idx));
  }
}

#if DEMO_MODE
// Fake printers that cycle through every state, to preview the UI without Wi-Fi or printers.
void demoTask(void*) {
  const int n = (int)config::printerCount();
  uint32_t tick = 0;
  Phase prev[MAX_PRINTERS] = {};
  state::setWifiUp(true);
  for (;;) {
    for (int i = 0; i < n; i++) {
      state::Guard guard;
      PrinterStatus& s = state::raw(i);
      s.link = Link::Online;
      s.updatedMs = millis();
      s.nozzle = 215; s.nozzleTarget = 220; s.bed = 59; s.bedTarget = 60; s.chamber = 31;
      s.wifiDbm = -52 - i;
      s.totalLayers = 320;
      s.speedLevel = 2; s.speedPct = 100; s.partFanPct = 66; s.stage = 0;
      strlcpy(s.nozzleDia, "0.4", sizeof s.nozzleDia);
      s.trayNow = 254;
      strlcpy(s.extType, "PLA", sizeof s.extType);
      switch ((i + tick / 40) % 6) {
        case 0: case 1: case 2:
          s.phase = Phase::Running;
          s.percent = (int8_t)((tick * (i + 1) / 2) % 99 + 1);
          s.remainingMin = 240 * (100 - s.percent) / 100;
          s.elapsedMin = s.remainingMin * s.percent / (100 - s.percent);
          s.layer = s.percent * 320 / 100;
          strlcpy(s.file, "Benchy_0.2mm_PLA.3mf", sizeof s.file);
          break;
        case 3:
          s.phase = Phase::Idle; s.percent = 0; s.nozzle = 25; s.nozzleTarget = 0; s.bed = 24; s.bedTarget = 0;
          break;
        case 4:
          s.phase = Phase::Finished; s.percent = 100; s.remainingMin = 0; s.elapsedMin = 187;
          strlcpy(s.file, "Phone_stand.3mf", sizeof s.file);
          break;
        default:
          s.phase = Phase::Failed; s.printError = 0x0500C011; s.percent = 37; s.remainingMin = 91; s.elapsedMin = 54;
          break;
      }
      if (i == n - 1 && (tick / 25) % 2) s.link = Link::Offline;
      // let the demo exercise the buzzer too
      if (s.phase != prev[i]) {
        if (s.phase == Phase::Finished) {
          s.finishedAtMs = millis();
          buzzer::play(buzzer::Sound::Finished);
        } else if (s.phase == Phase::Failed) {
          buzzer::play(buzzer::Sound::Failed);
        }
        prev[i] = s.phase;
      }
    }
    tick++;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
#endif

}  // namespace

namespace poller {

void begin() {
#if DEMO_MODE
  g_warm = false;  // fake data is there from the start
  xTaskCreate(demoTask, "demo", 4096, nullptr, 1, nullptr);
#else
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  xTaskCreate(task, "poller", 16384, nullptr, 1, nullptr);
#endif
}

void focus(int idx) {
  g_focus = idx;
  g_focusSetMs = millis();
  // Looking at an unreachable printer earns it a fresh attempt, except after a wrong access code:
  // retrying that only locks the printer's patience, it never starts working by itself.
  if (idx >= 0 && state::get(idx).link != Link::AuthFailed) g_retryAt[idx] = 0;
}

int visiting() { return g_visiting; }

bool warmingUp() { return g_warm; }

void warmupProgress(int& done, int& total) {
  done = g_warmDone;
  total = (int)config::printerCount();
}

}  // namespace poller
