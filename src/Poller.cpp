#include "Poller.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Config.h"
#include "State.h"
#include "settings.h"

namespace {

volatile int g_focus = -1;
volatile uint32_t g_focusSetMs = 0;
volatile int g_visiting = -1;

uint32_t g_retryAt[MAX_PRINTERS] = {};
uint32_t g_lastPushall[MAX_PRINTERS] = {};

PubSubClient g_mqtt;
JsonDocument g_filter;
int g_cur = -1;               // printer whose messages are being parsed
volatile bool g_gotFull = false;  // saw a report that carries gcode_state during this visit
uint32_t g_maxPayload = 0;

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

// Printers send only changed fields (A1/P1) or everything (X1): merge whatever is present.
void applyReport(int idx, JsonObjectConst p) {
  state::Guard guard;
  PrinterStatus& s = state::raw(idx);

  if (p["gcode_state"].is<const char*>()) {
    const Phase old = s.phase;
    s.phase = parsePhase(p["gcode_state"].as<const char*>());
    if (s.phase == Phase::Running && old != Phase::Running && old != Phase::Paused) s.elapsedMin = -1;
    g_gotFull = true;
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

  const char* name = p["subtask_name"].is<const char*>() ? p["subtask_name"].as<const char*>() : nullptr;
  const char* gcode = p["gcode_file"].is<const char*>() ? p["gcode_file"].as<const char*>() : nullptr;
  if (name && *name) strlcpy(s.file, name, sizeof s.file);
  else if (gcode && *gcode) strlcpy(s.file, gcode, sizeof s.file);

  // The printer reports the remaining time but not the elapsed time: estimate the latter from the
  // progress. Kept after the print ends so a finished job still shows how long it took.
  if ((s.phase == Phase::Running || s.phase == Phase::Paused) && s.percent >= 1 && s.percent <= 99 &&
      s.remainingMin >= 0) {
    s.elapsedMin = (int32_t)((int64_t)s.remainingMin * s.percent / (100 - s.percent));
  }

  s.link = Link::Online;
  s.updatedMs = millis();
}

void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  (void)topic;
  if (g_cur < 0) return;
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
  if (!p.isNull()) applyReport(g_cur, p);
}

void buildFilter() {
  JsonObject p = g_filter["print"].to<JsonObject>();
  const char* keys[] = {"gcode_state",     "mc_percent",   "mc_remaining_time", "layer_num",
                        "total_layer_num", "nozzle_temper", "nozzle_target_temper",
                        "bed_temper",      "bed_target_temper", "chamber_temper", "print_error",
                        "hms",             "wifi_signal",  "subtask_name",      "gcode_file"};
  for (const char* k : keys) p[k] = true;
}

// ---- Connection handling ---------------------------------------------------------------

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    state::setWifiUp(true);
    return;
  }
  state::setWifiUp(false);
  Serial.printf("[wifi] connecting to %s\n", config::wifiSsid());
  WiFi.disconnect();
  WiFi.begin(config::wifiSsid(), config::wifiPassword());
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) vTaskDelay(pdMS_TO_TICKS(200));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] up, ip %s, rssi %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    configTzTime(TIMEZONE, "pool.ntp.org", "time.cloudflare.com");
    state::setWifiUp(true);
  } else {
    Serial.println("[wifi] failed, will retry");
  }
}

enum class Visit : uint8_t { Ok, Offline, AuthFailed };

// Connects to one printer. In manual focus mode it stays until the focus moves elsewhere.
Visit visit(int idx) {
  const PrinterConfig& pc = config::printer(idx);
  g_visiting = idx;
  g_cur = idx;
  g_gotFull = false;
  {
    state::Guard guard;
    state::raw(idx).link = Link::Connecting;
  }

  WiFiClientSecure* tls = new WiFiClientSecure();
  tls->setInsecure();  // LAN-only device with a self-signed certificate
  tls->setTimeout(CONNECT_TIMEOUT_S);
  tls->setHandshakeTimeout(HANDSHAKE_TIMEOUT_S);
  g_mqtt.setClient(*tls);
  g_mqtt.setServer(pc.ip, 8883);

  char clientId[32];
  snprintf(clientId, sizeof clientId, "c3-monitor-%08x", (unsigned)ESP.getEfuseMac());

  const uint32_t t0 = millis();
  const bool ok = g_mqtt.connect(clientId, "bblp", pc.accessCode);
  if (!ok) {
    const int st = g_mqtt.state();
    Serial.printf("[%s] connect failed (mqtt state %d) after %lu ms\n", pc.name, st, millis() - t0);
    tls->stop();
    delete tls;
    // 4 = bad credentials, 5 = not authorized
    return (st == 4 || st == 5) ? Visit::AuthFailed : Visit::Offline;
  }
  Serial.printf("[%s] connected in %lu ms, free heap %u (largest block %u)\n", pc.name,
                millis() - t0, ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  char reportTopic[48], requestTopic[48];
  snprintf(reportTopic, sizeof reportTopic, "device/%s/report", pc.serial);
  snprintf(requestTopic, sizeof requestTopic, "device/%s/request", pc.serial);
  g_mqtt.subscribe(reportTopic);

  // A1/P1 printers only send changes; one "pushall" gives the full picture. It is rate limited.
  const uint32_t now = millis();
  bool expectFull = !state::get(idx).hasData();
  if (g_lastPushall[idx] == 0 || now - g_lastPushall[idx] > PUSHALL_MIN_INTERVAL_MS) {
    g_lastPushall[idx] = now ? now : 1;
    g_mqtt.publish(requestTopic,
                   "{\"pushing\":{\"sequence_id\":\"1\",\"command\":\"pushall\",\"version\":1,\"push_target\":1}}");
    Serial.printf("[%s] pushall sent\n", pc.name);
    expectFull = true;
  }

  // Without a pushall an idle A1/P1 may legitimately stay silent: then only listen briefly for
  // changes instead of waiting for a full report that is not coming.
  const uint32_t started = millis();
  uint32_t fullAt = expectFull ? 0 : started;
  while (g_mqtt.connected()) {
    g_mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(5));

    const uint32_t t = millis();
    if (g_gotFull && !fullAt) fullAt = t;
    const int focus = g_focus;
    if (focus >= 0) {
      if (focus != idx) break;  // the knob moved on
    } else {
      if (fullAt && t - fullAt > AUTO_LISTEN_MS) break;
      if (!fullAt && t - started > FIRST_REPORT_WAIT_MS) break;
    }
  }

  const bool gotReport = g_gotFull;
  {
    const PrinterStatus s = state::get(idx);
    Serial.printf("[%s] phase=%d %d%% left=%ldmin elapsed=%ldmin layer=%d/%d nozzle=%.0f/%.0f bed=%.0f/%.0f "
                  "err=%lu wifi=%ddBm file=\"%s\"\n",
                  pc.name, (int)s.phase, s.percent, (long)s.remainingMin, (long)s.elapsedMin, s.layer,
                  s.totalLayers, s.nozzle, s.nozzleTarget, s.bed, s.bedTarget, (unsigned long)s.printError,
                  s.wifiDbm, s.file);
  }
  g_mqtt.disconnect();
  tls->stop();
  delete tls;
  // Reachable counts as online even when silent; the UI shows "waiting for data" until a report lands.
  if (!gotReport && expectFull) Serial.printf("[%s] connected but no full report received\n", pc.name);
  return Visit::Ok;
}

// The next printer to talk to, or -1 if nobody is due yet.
int pickNext(int last) {
  const uint32_t now = millis();
  const int n = (int)config::printerCount();
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
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    const int idx = pickNext(last);
    if (idx < 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    last = idx;
    const Visit r = visit(idx);
    {
      state::Guard guard;
      PrinterStatus& s = state::raw(idx);
      if (r == Visit::Ok) s.link = Link::Online;
      else s.link = (r == Visit::AuthFailed) ? Link::AuthFailed : Link::Offline;
    }
    if (r == Visit::Ok) g_retryAt[idx] = 0;
    else if (r == Visit::AuthFailed) g_retryAt[idx] = millis() + OFFLINE_RETRY_MS;  // wrong code: do not hammer
    else g_retryAt[idx] = millis() + (g_focus == idx ? FOCUS_RETRY_MS : OFFLINE_RETRY_MS);
  }
}

#if DEMO_MODE
// Fake printers that cycle through every state, to preview the UI without Wi-Fi or printers.
void demoTask(void*) {
  const int n = (int)config::printerCount();
  uint32_t tick = 0;
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

}  // namespace poller
