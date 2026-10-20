#pragma once
#include <stdint.h>
#include "Input.h"

namespace ui {

void begin();
void handle(input::Event e);
void render();

void applyDisplaySettings();
int selectedPrinter();
void selectPrinter(int idx);
uint8_t currentView();
void setView(uint8_t v);
bool autoMode();
void setAutoMode(bool on);
bool sleeping();
void wake();
void sleepNow();
void toast(const char* text);
void notifyAlert(int idx);

}  // namespace ui
