#pragma once

#include <Arduino.h>

// Default gameplay configuration. Values are applied on first boot and can be
// overridden by persisted Preferences values. These defaults follow agents.md.
constexpr uint32_t API_POST_INTERVAL_MS = 1000;      // 1s
constexpr uint32_t API_TIMEOUT_MS = 10000;           // 10s
constexpr uint32_t BUTTON_HOLD_MS = 3000;            // 3s for arming/reset
constexpr uint8_t DEFUSE_CODE_LENGTH = 4;            // digits
constexpr uint8_t MAX_WIFI_RETRIES = 10;             // WiFi attempts

constexpr uint32_t DEFAULT_BOMB_DURATION_MS = 40000; // example: 40s

// Placeholder default defuse code used until Preferences or web UI override it.
static constexpr const char *DEFAULT_DEFUSE_CODE = "1234";

// Optional default API endpoint. Replace with the real backend URL when known.
static constexpr const char *DEFAULT_API_ENDPOINT = "https://n8n.santiso.xyz/webhook-test/ac64f84c-08b9-47e4-b252-a77f3411f0a9";
