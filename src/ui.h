#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace ui {
// Draw the boot screen showing WiFi/API progress before entering READY.
void renderBootScreen(const String &wifiSsid, bool wifiConnected, const String &ipAddress,
                      const String &apiEndpoint, bool hasApiResponse);

// Initialize the TFT and draw the static main screen layout (title, outlines, etc.).
void initMainScreen();

// Render the primary game UI reflecting the provided state and progress values.
void renderState(FlameState state, uint32_t bombDurationMs, uint32_t remainingMs, float armingProgress01,
                 uint8_t codeLength, uint8_t enteredDigits);

// Helper to format a millisecond value into MM:SS (zero-padded) for on-screen display.
void formatTimeMMSS(uint32_t ms, char *buffer, size_t len);

// Placeholder UI lifecycle hooks for future expansion.
void initUI();
void updateUI();

// Display instructions when the device enters SoftAP configuration mode.
void renderConfigPortalScreen(const String &ssid, const String &password);
}  // namespace ui
