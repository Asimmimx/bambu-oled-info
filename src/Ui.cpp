#include "Ui.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>
#include "Config.h"
#include "Poller.h"
#include "State.h"
#include "settings.h"

#if UI_LANG_TR
#define S_LEFT    "Kalan"
#define S_ELAPSED "Geçen"
#define S_ETA     "Bitis"
#define S_READY   "Hazir"
#define S_ERROR   "Hata"
#define S_WIFI    "WiFi baglaniyor"
#define S_NODATA  "Veri bekleniyor"
#define S_BADCODE "Erisim kodu hatali"
#define S_NOREPLY "Yanit yok"
#define S_AUTO_ON  "Otomatik: ACIK"
#define S_AUTO_OFF "Otomatik: KAPALI"
#define U_HOUR "s"
#define U_MIN  "d"
#else
#define S_LEFT    "Left"
#define S_ELAPSED "Elapsed"
#define S_ETA     "ETA"
#define S_READY   "Ready"
#define S_ERROR   "Error"
#define S_WIFI    "Connecting WiFi"
#define S_NODATA  "Waiting for data"
#define S_BADCODE "Wrong access code"
#define S_NOREPLY "No reply"
#define S_AUTO_ON  "Auto: ON"
#define S_AUTO_OFF "Auto: OFF"
#define U_HOUR "h"
#define U_MIN  "m"
#endif

namespace {

enum class View : uint8_t { Progress, Details, Overview };

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

View g_view = View::Progress;
int g_sel = 0;
bool g_autoMode = true;   // carousel enabled (long press toggles)
bool g_manual = false;    // knob touched recently: stay on g_sel
uint32_t g_lastInput = 0;
uint32_t g_carouselAt = 0;  // when the current printer came on screen (auto mode)
uint32_t g_lastActive = 0;
bool g_asleep = false;
uint32_t g_lastFrame = 0;
uint32_t g_frame = 0;
char g_toast[24] = "";
uint32_t g_toastUntil = 0;

int printerCount() { return (int)config::printerCount(); }

bool following() { return g_autoMode && !g_manual; }

void showToast(const char* text) {
  strlcpy(g_toast, text, sizeof g_toast);
  g_toastUntil = millis() + 1200;
}

// Tells the poller which printer to stay connected to. The overview shows every printer at once,
// so there the poller is left free to visit them all in turn.
void applyFocus() {
  if (following() && g_view == View::Overview) poller::focus(-1);
  else poller::focus(g_sel);
}

// ---- Formatting ------------------------------------------------------------------------

void fmtDuration(char* out, size_t n, int32_t min) {
  if (min < 0) snprintf(out, n, "--");
  else if (min < 60) snprintf(out, n, "%d" U_MIN, (int)min);
  else snprintf(out, n, "%d" U_HOUR "%02d" U_MIN, (int)(min / 60), (int)(min % 60));
}

void fmtTemp(char* out, size_t n, char prefix, float now, float target) {
  if (isnan(now)) snprintf(out, n, "%c--", prefix);
  else if (isnan(target) || target < 1) snprintf(out, n, "%c%.0f", prefix, now);
  else snprintf(out, n, "%c%.0f/%.0f", prefix, now, target);
}

const char* tagFor(const PrinterStatus& s) {
  if (s.link == Link::Offline) return "OFF";
  if (s.link == Link::AuthFailed) return "AUTH";
  if (!s.hasData()) return "...";
  if (s.printError != 0 && s.phase != Phase::Failed) return "ERR";
  switch (s.phase) {
    case Phase::Running: return "RUN";
    case Phase::Paused: return "PAUSE";
    case Phase::Prepare: return "PREP";
    case Phase::Finished: return "DONE";
    case Phase::Failed: return "ERR";
    case Phase::Idle: return "IDLE";
    default: return "...";
  }
}

// "finishes at" clock, empty until NTP has synced
bool fmtEta(char* out, size_t n, int32_t remainingMin) {
  const time_t now = time(nullptr);
  if (now < 1700000000 || remainingMin < 0) return false;
  const time_t eta = now + (time_t)remainingMin * 60;
  struct tm t, t0;
  localtime_r(&eta, &t);
  localtime_r(&now, &t0);
  const int days = (t.tm_yday - t0.tm_yday + 366) % 366;
  if (days > 0) snprintf(out, n, "%02d:%02d+%d", t.tm_hour, t.tm_min, days);
  else snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
  return true;
}

// ---- Drawing helpers -------------------------------------------------------------------

void drawRight(int y, const char* text) {
  u8g2.drawUTF8(128 - u8g2.getUTF8Width(text), y, text);
}

void drawBar(int x, int y, int w, int h, int percent) {
  u8g2.drawFrame(x, y, w, h);
  const int fill = ((w - 4) * constrain(percent, 0, 100)) / 100;
  if (fill > 0) u8g2.drawBox(x + 2, y + 2, fill, h - 4);
}

void drawCentered(int y, const char* text) {
  u8g2.drawUTF8((128 - u8g2.getUTF8Width(text)) / 2, y, text);
}

bool blinkOn() { return (millis() / 500) % 2 == 0; }

// ---- Views -----------------------------------------------------------------------------

void drawHeader(const PrinterConfig& pc, const PrinterStatus& s) {
  u8g2.setFont(u8g2_font_6x10_tf);
  char name[16];
  snprintf(name, sizeof name, "%.14s", pc.name);
  u8g2.drawUTF8(0, 0, name);
  drawRight(0, tagFor(s));
  if (s.needsAttention() && blinkOn()) {  // flash the header on errors
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(1);
  }
}

void drawProgressView(int idx, const PrinterStatus& s) {
  const PrinterConfig& pc = config::printer(idx);
  drawHeader(pc, s);

  if (s.link == Link::Offline || s.link == Link::AuthFailed || !s.hasData()) {
    u8g2.setFont(u8g2_font_6x10_tf);
    if (s.link == Link::AuthFailed) drawCentered(13, S_BADCODE);
    else if (s.link == Link::Offline) drawCentered(13, S_NOREPLY);
    else drawCentered(13, S_NODATA);
    u8g2.setFont(u8g2_font_5x8_tf);
    drawCentered(24, pc.ip);
    return;
  }

  char buf[40], a[12], b[12];
  const bool showBar = s.active() || s.phase == Phase::Finished || (s.phase == Phase::Failed && s.percent > 0);
  u8g2.setFont(u8g2_font_6x10_tf);
  if (showBar && s.percent >= 0) {
    drawBar(0, 12, 100, 9, s.percent);
    snprintf(buf, sizeof buf, "%d%%", s.percent);
    u8g2.drawUTF8(104, 11, buf);
  } else {
    u8g2.drawUTF8(0, 12, s.needsAttention() ? S_ERROR : S_READY);
  }

  u8g2.setFont(u8g2_font_5x8_tf);
  if (s.active()) {
    fmtDuration(a, sizeof a, s.remainingMin);
    fmtDuration(b, sizeof b, s.elapsedMin);
    snprintf(buf, sizeof buf, S_LEFT " %s " S_ELAPSED " %s", a, b);
  } else if (s.phase == Phase::Finished) {
    fmtDuration(a, sizeof a, s.elapsedMin);
    snprintf(buf, sizeof buf, S_ELAPSED " %s", a);
  } else if (s.needsAttention()) {
    snprintf(buf, sizeof buf, "%08lX", (unsigned long)s.printError);
  } else {
    char n[16], t[16];
    fmtTemp(n, sizeof n, 'N', s.nozzle, s.nozzleTarget);
    fmtTemp(t, sizeof t, 'B', s.bed, s.bedTarget);
    snprintf(buf, sizeof buf, "%s %s", n, t);
  }
  u8g2.drawUTF8(0, 24, buf);
}

void drawDetailsView(int idx, const PrinterStatus& s) {
  const PrinterConfig& pc = config::printer(idx);
  u8g2.setFont(u8g2_font_5x8_tf);

  char buf[40], n[16], t[16], c[10];
  snprintf(buf, sizeof buf, "%.13s", pc.name);
  u8g2.drawUTF8(0, 0, buf);
  if (s.totalLayers > 0 && s.layer >= 0) {
    snprintf(buf, sizeof buf, "L%d/%d", s.layer, s.totalLayers);
    drawRight(0, buf);
  } else {
    drawRight(0, tagFor(s));
  }

  if (s.link == Link::Offline || s.link == Link::AuthFailed || !s.hasData()) {
    drawCentered(12, s.link == Link::AuthFailed ? S_BADCODE : s.link == Link::Offline ? S_NOREPLY : S_NODATA);
    drawCentered(24, pc.ip);
    return;
  }

  fmtTemp(n, sizeof n, 'N', s.nozzle, s.nozzleTarget);
  fmtTemp(t, sizeof t, 'B', s.bed, s.bedTarget);
  c[0] = 0;
  if (!isnan(s.chamber) && s.chamber > 1) snprintf(c, sizeof c, " C%.0f", s.chamber);
  snprintf(buf, sizeof buf, "%s %s%s", n, t, c);
  u8g2.drawUTF8(0, 8, buf);

  char eta[12];
  if (s.active() && fmtEta(eta, sizeof eta, s.remainingMin)) snprintf(buf, sizeof buf, S_ETA " %s", eta);
  else buf[0] = 0;
  u8g2.drawUTF8(0, 16, buf);
  if (s.wifiDbm != 0) {
    snprintf(buf, sizeof buf, "%ddBm", s.wifiDbm);
    drawRight(16, buf);
  }

  // File name, scrolling when it does not fit
  if (s.file[0]) {
    const int w = u8g2.getUTF8Width(s.file);
    int x = 0;
    if (w > 128) {
      const int span = w + 24;
      x = -(int)((g_frame / 2) % span);
      u8g2.drawUTF8(x + span, 24, s.file);
    }
    u8g2.drawUTF8(x, 24, s.file);
  }
}

void drawOverviewView(int selected) {
  u8g2.setFont(u8g2_font_4x6_tf);
  const int n = printerCount();
  for (int i = 0; i < n && i < 6; i++) {
    const PrinterConfig& pc = config::printer(i);
    const PrinterStatus s = state::get(i);
    const int x = (i % 3) * 43;
    const int y = (i / 3) * 16;

    char value[8];
    if (s.link == Link::Offline || s.link == Link::AuthFailed || !s.hasData()) snprintf(value, sizeof value, "%s", s.link == Link::AuthFailed ? "AUTH" : "off");
    else if (s.active() && s.percent >= 0) snprintf(value, sizeof value, "%d%%", s.percent);
    else if (s.phase == Phase::Finished) snprintf(value, sizeof value, "done");
    else if (s.needsAttention()) snprintf(value, sizeof value, "ERR");
    else snprintf(value, sizeof value, "idle");

    char name[6];
    snprintf(name, sizeof name, "%.5s", pc.shortName);

    const bool sel = (i == selected);
    if (sel) u8g2.drawBox(x, y, 41, 7);
    u8g2.setDrawColor(sel ? 0 : 1);
    u8g2.drawStr(x + 1, y + 1, name);
    u8g2.drawStr(x + 41 - u8g2.getStrWidth(value), y + 1, value);
    u8g2.setDrawColor(1);

    if (s.needsAttention() && blinkOn()) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(x, y, 41, 7);
      u8g2.setDrawColor(1);
    }
    if ((s.active() || s.phase == Phase::Finished) && s.percent >= 0 && s.hasData() && s.link != Link::Offline) {
      drawBar(x, y + 9, 41, 5, s.percent);
    } else {
      u8g2.drawHLine(x, y + 11, 41);  // dotted rule for quiet printers
      u8g2.setDrawColor(0);
      for (int dx = 1; dx < 41; dx += 2) u8g2.drawPixel(x + dx, y + 11);
      u8g2.setDrawColor(1);
    }
  }
}

void drawToast() {
  u8g2.setFont(u8g2_font_6x10_tf);
  const int w = u8g2.getUTF8Width(g_toast) + 10;
  const int x = (128 - w) / 2;
  u8g2.setDrawColor(0);
  u8g2.drawBox(x - 1, 8, w + 2, 16);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(x, 9, w, 14);
  u8g2.drawUTF8(x + 5, 11, g_toast);
}

bool anyActive() {
  for (int i = 0; i < printerCount(); i++) {
    const PrinterStatus s = state::get(i);
    if (s.link != Link::Offline && (s.active() || s.needsAttention())) return true;
  }
  return false;
}

void setSleeping(bool sleep) {
  if (sleep == g_asleep) return;
  g_asleep = sleep;
  u8g2.setPowerSave(sleep ? 1 : 0);
}

// ---- Display wiring check ---------------------------------------------------------------

// True if something acknowledges 0x3C or 0x3D (SSD1306) on this pin pair.
bool probeOled(int sda, int scl, uint8_t& addr) {
  Wire.end();
  if (!Wire.begin(sda, scl, 100000)) return false;
  Wire.setTimeOut(5);
  for (uint8_t a : {0x3C, 0x3D}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr = a;
      return true;
    }
  }
  return false;
}

// Logs where the display answers; if it is silent on the configured pins, tries every other pair of
// free GPIOs so a wiring mistake shows up in the log instead of as a blank screen.
void checkDisplayWiring() {
  uint8_t addr = 0;
  if (probeOled(PIN_OLED_SDA, PIN_OLED_SCL, addr)) {
    Serial.printf("[oled] found at 0x%02X (SDA=GPIO%d SCL=GPIO%d)\n", addr, PIN_OLED_SDA, PIN_OLED_SCL);
    if (addr != 0x3C) u8g2.setI2CAddress(addr << 1);
  } else {
    Serial.printf("[oled] no answer on SDA=GPIO%d SCL=GPIO%d, scanning other pin pairs...\n", PIN_OLED_SDA,
                  PIN_OLED_SCL);
    const int pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    bool found = false;
    for (int sda : pins) {
      for (int scl : pins) {
        if (sda == scl || (sda == PIN_OLED_SDA && scl == PIN_OLED_SCL)) continue;
        if (probeOled(sda, scl, addr)) {
          Serial.printf("[oled] FOUND at 0x%02X with SDA=GPIO%d SCL=GPIO%d -> rewire, or set PIN_OLED_SDA/SCL in settings.h\n",
                        addr, sda, scl);
          found = true;
        }
      }
    }
    if (!found) Serial.println("[oled] not found on any pin pair: check VCC (3V3), GND and the solder joints");
  }
  Wire.end();
}

}  // namespace

namespace ui {

void begin() {
  checkDisplayWiring();
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setFontPosTop();
  u8g2.enableUTF8Print();
  g_lastActive = g_carouselAt = millis();
  applyFocus();
}

void handle(input::Event e) {
  if (e == input::Event::None) return;
  const uint32_t now = millis();
  g_lastInput = now;
  g_lastActive = now;
  if (g_asleep) {  // the first touch only wakes the screen
    setSleeping(false);
    return;
  }

  const int n = printerCount();
  switch (e) {
    case input::Event::Left:
    case input::Event::Right:
      if (n == 0) break;
      g_manual = true;
      g_sel = (g_sel + (e == input::Event::Right ? 1 : n - 1)) % n;
      poller::focus(g_sel);
      break;
    case input::Event::Click:
      g_view = (View)(((int)g_view + 1) % 3);
      g_carouselAt = now;
      applyFocus();
      break;
    case input::Event::LongPress:
      g_autoMode = !g_autoMode;
      g_manual = !g_autoMode;
      g_carouselAt = now;
      applyFocus();
      showToast(g_autoMode ? S_AUTO_ON : S_AUTO_OFF);
      break;
    default:
      break;
  }
}

void render() {
  const uint32_t now = millis();

  // Auto-resume the carousel after the knob has been idle for a while
  if (g_manual && g_autoMode && now - g_lastInput > MANUAL_TIMEOUT_MS) {
    g_manual = false;
    g_carouselAt = now;
    applyFocus();
  }
  if (following()) {
    const int n = printerCount();
    if (g_view == View::Overview) {
      // Highlight whichever printer the poller is visiting right now
      const int v = poller::visiting();
      if (v >= 0 && v < n) g_sel = v;
    } else if (n > 1 && now - g_carouselAt >= CAROUSEL_MS) {
      g_carouselAt = now;
      g_sel = (g_sel + 1) % n;
      poller::focus(g_sel);
      Serial.printf("[ui] showing %s\n", config::printer(g_sel).name);
    }
  }

  if (anyActive()) g_lastActive = now;
  if (SCREEN_SLEEP_MS > 0) {
    if (now - g_lastActive > SCREEN_SLEEP_MS) setSleeping(true);
    else setSleeping(false);
  }
  if (g_asleep) return;

  if (now - g_lastFrame < 100) return;  // 10 fps
  g_lastFrame = now;
  g_frame++;

  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  if (printerCount() == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered(11, "No printers");
  } else if (!state::wifiUp()) {
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered(4, S_WIFI);
    u8g2.setFont(u8g2_font_5x8_tf);
    drawCentered(18, config::wifiSsid());
  } else {
    const int idx = constrain(g_sel, 0, printerCount() - 1);
    switch (g_view) {
      case View::Progress: drawProgressView(idx, state::get(idx)); break;
      case View::Details: drawDetailsView(idx, state::get(idx)); break;
      case View::Overview: drawOverviewView(idx); break;
    }
  }
  if (now < g_toastUntil) drawToast();
  u8g2.sendBuffer();
}

}  // namespace ui
