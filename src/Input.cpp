#include "Input.h"
#include <Arduino.h>
#include "settings.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t LONG_PRESS_MS = 800;

// Quadrature decoder, indexed by (previous AB << 2 | current AB). Invalid (bouncy) jumps are 0.
const int8_t kStep[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

volatile uint8_t g_ab = 0;
volatile int8_t g_acc = 0;
volatile int16_t g_detents = 0;

void IRAM_ATTR onEdge() {
  const uint8_t a = digitalRead(PIN_ENC_CLK);
  const uint8_t b = digitalRead(PIN_ENC_DT);
  const uint8_t ab = ((g_ab << 2) | (a << 1) | b) & 0x0F;
  g_ab = ab;
  int8_t acc = g_acc + kStep[ab];
  if (acc >= ENC_EDGES_PER_DETENT) {
    acc = 0;
    g_detents = g_detents + 1;
  } else if (acc <= -ENC_EDGES_PER_DETENT) {
    acc = 0;
    g_detents = g_detents - 1;
  }
  g_acc = acc;
}

bool g_down = false;
bool g_longFired = false;
uint32_t g_downAt = 0;
uint32_t g_lastChange = 0;

}  // namespace

namespace input {

void begin() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  g_ab = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), onEdge, CHANGE);
}

Event poll() {
  // Rotation
  noInterrupts();
  int16_t d = g_detents;
  if (d > 0) g_detents = d - 1;
  else if (d < 0) g_detents = d + 1;
  interrupts();
  if (d != 0) {
    const bool right = (d > 0) != (ENC_REVERSE != 0);
    return right ? Event::Right : Event::Left;
  }

  // Push button (active low): short click on release, long press while held
  const uint32_t now = millis();
  const bool pressed = digitalRead(PIN_ENC_SW) == LOW;
  if (pressed != g_down && now - g_lastChange > DEBOUNCE_MS) {
    g_lastChange = now;
    g_down = pressed;
    if (pressed) {
      g_downAt = now;
      g_longFired = false;
    } else if (!g_longFired) {
      return Event::Click;
    }
  }
  if (g_down && !g_longFired && now - g_downAt >= LONG_PRESS_MS) {
    g_longFired = true;
    return Event::LongPress;
  }
  return Event::None;
}

}  // namespace input
