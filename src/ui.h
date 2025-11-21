#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace ui {
// Initialize the TFT and draw the static main screen layout (title, outlines, etc.).
void initMainScreen();

// Boot/connection screen helpers drawn prior to READY state.
void showBootScreen(const char *ssid);
void updateBootStatus(bool connected, const String &ipString, const String &apiEndpoint, bool apiResponseReceived);

// Render the primary game UI reflecting the provided state and progress values.
void renderState(FlameState state, uint32_t bombDurationMs, uint32_t remainingMs, float armingProgress01,
                 uint8_t codeLength, uint8_t enteredDigits);

// Helper to format a millisecond value into MM:SS (zero-padded) for on-screen display.
void formatTimeMMSS(uint32_t ms, char *buffer, size_t len);

// Placeholder UI lifecycle hooks for future expansion.
void initUI();
void updateUI();
}  // namespace ui
