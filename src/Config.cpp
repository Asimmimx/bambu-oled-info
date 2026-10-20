#include "Config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

// A fresh clone has no secrets.h yet: fall back to the example so the demo build still compiles.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Bambu Cloud access is optional (only for printers whose ip is ""). Written by
// tools/fetch_access_codes.py --cloud, or set through the web configuration portal.
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

namespace {

constexpr const char* kNvsNamespace = "bambu-cfg";
constexpr const char* kSetNamespace = "bambu-set";

char g_wifiSsid[64] = "";
char g_wifiPassword[64] = "";
char g_cloudHost[64] = "";
char g_cloudUser[48] = "";
char g_cloudToken[1280] = "";
PrinterConfig g_printers[MAX_PRINTERS] = {};
size_t g_printerCount = 0;
bool g_hasCustom = false;
DeviceSettings g_settings;

void loadDefaults() {
  strlcpy(g_wifiSsid, WIFI_SSID, sizeof g_wifiSsid);
  strlcpy(g_wifiPassword, WIFI_PASSWORD, sizeof g_wifiPassword);
  strlcpy(g_cloudHost, BAMBU_CLOUD_HOST, sizeof g_cloudHost);
  strlcpy(g_cloudUser, BAMBU_CLOUD_USER, sizeof g_cloudUser);
  strlcpy(g_cloudToken, BAMBU_CLOUD_TOKEN, sizeof g_cloudToken);

  constexpr size_t kDefCount = sizeof(PRINTERS) / sizeof(PRINTERS[0]);
  g_printerCount = kDefCount <= MAX_PRINTERS ? kDefCount : MAX_PRINTERS;
  memset(g_printers, 0, sizeof g_printers);
  for (size_t i = 0; i < g_printerCount; i++) {
    g_printers[i] = PRINTERS[i];
  }
  g_settings = DeviceSettings{};
  g_hasCustom = false;
}

void readStr(Preferences& prefs, const char* key, char* out, size_t n, const char* def = "") {
  if (prefs.isKey(key)) {
    String val = prefs.getString(key, def);
    strlcpy(out, val.c_str(), n);
  } else {
    strlcpy(out, def, n);
  }
}

void loadDeviceSettings() {
  Preferences sp;
  if (!sp.begin(kSetNamespace, true)) return;
  if (!sp.getBool("saved", false)) {
    sp.end();
    return;
  }
  g_hasCustom = true;
  g_settings.muted = sp.getBool("muted", g_settings.muted);
  g_settings.startupChirp = sp.getBool("chirp", g_settings.startupChirp);
  g_settings.buzzerPassive = sp.getBool("passive", g_settings.buzzerPassive);
  g_settings.buzzerActiveLow = sp.getBool("actlow", g_settings.buzzerActiveLow);
  g_settings.finishMelody = sp.getUChar("fin_mel", g_settings.finishMelody);
  g_settings.failedMelody = sp.getUChar("err_mel", g_settings.failedMelody);
  readStr(sp, "cus_fin", g_settings.customFinish, sizeof g_settings.customFinish, g_settings.customFinish);
  readStr(sp, "cus_err", g_settings.customFailed, sizeof g_settings.customFailed, g_settings.customFailed);

  g_settings.oledContrast = sp.getUChar("contrast", g_settings.oledContrast);
  g_settings.oledFlip = sp.getBool("flip", g_settings.oledFlip);
  g_settings.oledInvert = sp.getBool("invert", g_settings.oledInvert);
  g_settings.defaultView = sp.getUChar("def_view", g_settings.defaultView);
  g_settings.autoCycle = sp.getBool("autocyc", g_settings.autoCycle);
  g_settings.autoJumpAlert = sp.getBool("autojmp", g_settings.autoJumpAlert);
  g_settings.carouselSec = sp.getUShort("car_sec", g_settings.carouselSec);
  g_settings.manualTimeoutSec = sp.getUShort("man_sec", g_settings.manualTimeoutSec);
  g_settings.doneAlertSec = sp.getUShort("done_sec", g_settings.doneAlertSec);
  g_settings.screenSleepSec = sp.getUShort("slp_sec", g_settings.screenSleepSec);

  g_settings.encReverse = sp.getBool("enc_rev", g_settings.encReverse);
  g_settings.encEdges = sp.getUChar("enc_edg", g_settings.encEdges);
  readStr(sp, "tz", g_settings.timezone, sizeof g_settings.timezone, TIMEZONE);
  sp.end();
}

}  // namespace

namespace config {

void begin() {
  loadDefaults();
  loadDeviceSettings();
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) return;
  if (!prefs.getBool("custom", false)) {
    prefs.end();
    Serial.printf("[cfg] using compile-time printer defaults (%u printers)\n", (unsigned)g_printerCount);
    return;
  }

  g_hasCustom = true;
  readStr(prefs, "w_ssid", g_wifiSsid, sizeof g_wifiSsid, WIFI_SSID);
  readStr(prefs, "w_pass", g_wifiPassword, sizeof g_wifiPassword, WIFI_PASSWORD);
  readStr(prefs, "c_host", g_cloudHost, sizeof g_cloudHost, BAMBU_CLOUD_HOST);
  readStr(prefs, "c_user", g_cloudUser, sizeof g_cloudUser, BAMBU_CLOUD_USER);
  readStr(prefs, "c_token", g_cloudToken, sizeof g_cloudToken, BAMBU_CLOUD_TOKEN);
  if (g_cloudHost[0] == '\0') strlcpy(g_cloudHost, "us.mqtt.bambulab.com", sizeof g_cloudHost);

  const uint8_t count = prefs.getUChar("p_cnt", 0);
  g_printerCount = count <= MAX_PRINTERS ? count : MAX_PRINTERS;
  memset(g_printers, 0, sizeof g_printers);
  for (size_t i = 0; i < g_printerCount; i++) {
    char key[16];
    snprintf(key, sizeof key, "p%u_name", (unsigned)i);
    readStr(prefs, key, g_printers[i].name, sizeof g_printers[i].name);
    snprintf(key, sizeof key, "p%u_shrt", (unsigned)i);
    readStr(prefs, key, g_printers[i].shortName, sizeof g_printers[i].shortName);
    snprintf(key, sizeof key, "p%u_ip", (unsigned)i);
    readStr(prefs, key, g_printers[i].ip, sizeof g_printers[i].ip);
    snprintf(key, sizeof key, "p%u_sn", (unsigned)i);
    readStr(prefs, key, g_printers[i].serial, sizeof g_printers[i].serial);
    snprintf(key, sizeof key, "p%u_code", (unsigned)i);
    readStr(prefs, key, g_printers[i].accessCode, sizeof g_printers[i].accessCode);
  }
  prefs.end();
  Serial.printf("[cfg] loaded custom config from NVS (%u printers)\n", (unsigned)g_printerCount);
}

size_t printerCount() { return g_printerCount; }
const PrinterConfig& printer(size_t i) { return g_printers[i % MAX_PRINTERS]; }
bool isCloud(size_t i) { return g_printers[i % MAX_PRINTERS].ip[0] == '\0'; }
const char* wifiSsid() { return g_wifiSsid; }
const char* wifiPassword() { return g_wifiPassword; }

bool cloudConfigured() { return strlen(g_cloudUser) > 0 && strlen(g_cloudToken) > 0; }
const char* cloudHost() { return g_cloudHost; }
const char* cloudUser() { return g_cloudUser; }
const char* cloudToken() { return g_cloudToken; }

DeviceSettings& settings() { return g_settings; }

bool saveSettings() {
  Preferences sp;
  if (!sp.begin(kSetNamespace, false)) return false;
  sp.putBool("saved", true);
  sp.putBool("muted", g_settings.muted);
  sp.putBool("chirp", g_settings.startupChirp);
  sp.putBool("passive", g_settings.buzzerPassive);
  sp.putBool("actlow", g_settings.buzzerActiveLow);
  sp.putUChar("fin_mel", g_settings.finishMelody);
  sp.putUChar("err_mel", g_settings.failedMelody);
  sp.putString("cus_fin", g_settings.customFinish);
  sp.putString("cus_err", g_settings.customFailed);

  sp.putUChar("contrast", g_settings.oledContrast);
  sp.putBool("flip", g_settings.oledFlip);
  sp.putBool("invert", g_settings.oledInvert);
  sp.putUChar("def_view", g_settings.defaultView);
  sp.putBool("autocyc", g_settings.autoCycle);
  sp.putBool("autojmp", g_settings.autoJumpAlert);
  sp.putUShort("car_sec", g_settings.carouselSec);
  sp.putUShort("man_sec", g_settings.manualTimeoutSec);
  sp.putUShort("done_sec", g_settings.doneAlertSec);
  sp.putUShort("slp_sec", g_settings.screenSleepSec);

  sp.putBool("enc_rev", g_settings.encReverse);
  sp.putUChar("enc_edg", g_settings.encEdges);
  sp.putString("tz", g_settings.timezone);
  sp.end();
  g_hasCustom = true;
  return true;
}

bool hasCustomConfig() { return g_hasCustom; }

bool save(const char* ssid, const char* pass, const char* cHost, const char* cUser, const char* cToken,
          const PrinterConfig* printers, size_t count) {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return false;
  prefs.clear();

  prefs.putBool("custom", true);
  prefs.putString("w_ssid", ssid ? ssid : "");
  prefs.putString("w_pass", pass ? pass : "");
  prefs.putString("c_host", (cHost && *cHost) ? cHost : "us.mqtt.bambulab.com");
  prefs.putString("c_user", cUser ? cUser : "");
  prefs.putString("c_token", cToken ? cToken : "");

  const size_t n = count <= MAX_PRINTERS ? count : MAX_PRINTERS;
  prefs.putUChar("p_cnt", (uint8_t)n);
  for (size_t i = 0; i < n; i++) {
    char key[16];
    snprintf(key, sizeof key, "p%u_name", (unsigned)i);
    prefs.putString(key, printers[i].name);
    snprintf(key, sizeof key, "p%u_shrt", (unsigned)i);
    prefs.putString(key, printers[i].shortName);
    snprintf(key, sizeof key, "p%u_ip", (unsigned)i);
    prefs.putString(key, printers[i].ip);
    snprintf(key, sizeof key, "p%u_sn", (unsigned)i);
    prefs.putString(key, printers[i].serial);
    snprintf(key, sizeof key, "p%u_code", (unsigned)i);
    prefs.putString(key, printers[i].accessCode);
  }
  prefs.end();
  return true;
}

void resetToDefaults() {
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, false)) {
    prefs.clear();
    prefs.end();
  }
  Preferences sp;
  if (sp.begin(kSetNamespace, false)) {
    sp.clear();
    sp.end();
  }
  loadDefaults();
}

}  // namespace config
