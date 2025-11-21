#pragma once

#include <Arduino.h>

// Default gameplay configuration. Values are applied on first boot and can be
// overridden by persisted Preferences values. These defaults follow agents.md.
#ifdef DEBUG
constexpr uint32_t API_POST_INTERVAL_MS = 5000;      // 5s for testing
#else
constexpr uint32_t API_POST_INTERVAL_MS = 500;      // 500ms for live
#endif
constexpr uint32_t API_TIMEOUT_MS = 10000;           // 10s
constexpr uint32_t BUTTON_HOLD_MS = 3000;            // 3s for arming/reset
constexpr uint8_t DEFUSE_CODE_LENGTH = 4;            // digits
constexpr uint8_t MAX_WIFI_RETRIES = 10;             // WiFi attempts

constexpr uint32_t DEFAULT_BOMB_DURATION_MS = 40000; // example: 40s

// Placeholder default defuse code used until Preferences or web UI override it.
static constexpr const char *DEFAULT_DEFUSE_CODE = "1234";

// Optional default API endpoint. Replace with the real backend URL when known.
static constexpr const char *DEFAULT_API_ENDPOINT = "192.168.1.234:9055/prop/";

// Controls how the device interacts with the backend API. Additional configurability
// will be added later; for now the mode is fixed to TestSendOnly.
enum class ApiMode {
  Disabled,      // no HTTP calls, maybe just log JSON
  TestSendOnly,  // send POST, ignore response and never trigger errors
  FullOnline     // send POST, parse response, enforce timeout rules
};

inline ApiMode getApiMode() { return ApiMode::FullOnline; }
