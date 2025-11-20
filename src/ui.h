#pragma once

#include "state_machine.h"

namespace ui {
// Initializes the TFT display and shows the initial splash screen.
void initDisplay();

// Draws the current state text in a dedicated status area.
void renderStatus(FlameState state);

// Placeholder UI lifecycle hooks for future expansion.
void initUI();
void updateUI();
}  // namespace ui
