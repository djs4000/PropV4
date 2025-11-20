#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace ui {
// Initializes the TFT display and shows the initial splash screen.
void initDisplay();

// Draws the current state text and WiFi status in dedicated areas.
void renderStatus(FlameState state, bool wifiConnected, bool wifiError, const String &wifiIp);

// Placeholder UI lifecycle hooks for future expansion.
void initUI();
void updateUI();
}  // namespace ui
