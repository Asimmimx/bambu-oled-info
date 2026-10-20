#include <Arduino.h>
#include "Buzzer.h"
#include "Config.h"
#include "Input.h"
#include "Poller.h"
#include "State.h"
#include "Ui.h"
#include "Web.h"
#include "settings.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\nBambu print farm monitor");
  config::begin();
  state::begin();
  input::begin();
  buzzer::begin();
  ui::begin();
  poller::begin();
  web::begin();
}

void loop() {
  web::handle();
  ui::handle(input::poll());
  ui::render();
  buzzer::update();

  // Periodic heap report: a steady "min free" over hours means no leak in the TLS/JSON path.
  static uint32_t lastLog = 0;
  if (millis() - lastLog >= HEARTBEAT_MS) {
    lastLog = millis();
    Serial.printf("[sys] up %lus, heap free %u, min %u, largest block %u | knob pins clk=%d dt=%d sw=%d\n",
                  millis() / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                  digitalRead(PIN_ENC_CLK), digitalRead(PIN_ENC_DT), digitalRead(PIN_ENC_SW));
  }
  delay(4);
}
