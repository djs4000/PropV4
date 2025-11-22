#pragma once

#include <Arduino.h>

// Default gameplay configuration. Values are applied on first boot and can be
// overridden by persisted Preferences values. These defaults follow agents.md.

// Networking cadence and timeout handling
constexpr uint32_t API_POST_INTERVAL_MS = 500;   // ms between POST attempts
constexpr uint32_t API_TIMEOUT_MS = 10000;       // ms before triggering ERROR_STATE

// Input handling
constexpr uint32_t BUTTON_HOLD_MS = 3000;        // ms both buttons must be held
constexpr uint32_t IR_CONFIRM_WINDOW_MS = 2000;  // ms allowed for IR confirmation
constexpr uint8_t DEFUSE_CODE_LENGTH = 4;        // digits
constexpr uint8_t MAX_WIFI_RETRIES = 10;         // WiFi attempts

constexpr uint32_t DEFAULT_BOMB_DURATION_MS = 40000;  // example: 40s

// Placeholder default defuse code used until Preferences or web UI override it.
static constexpr const char *DEFAULT_DEFUSE_CODE = "1234";

// Optional default API endpoint. Replace with the real backend URL when known.
static constexpr const char *DEFAULT_API_ENDPOINT = "http://192.168.1.234:9055/prop";

// SoftAP configuration used when station mode fails and the device enters the
// configuration portal. The SSID is generated at runtime using the prefix and
// the last bytes of the MAC address for uniqueness.
static constexpr const char *SOFTAP_SSID_PREFIX = "DigitalFlame-";
static constexpr const char *SOFTAP_PASSWORD = "digitalflame";

// Controls how the device interacts with the backend API. Additional configurability
// will be added later; for now the mode is fixed to TestSendOnly.
enum class ApiMode {
  Disabled,      // no HTTP calls, maybe just log JSON
  TestSendOnly,  // send POST, ignore response and never trigger errors
  FullOnline     // send POST, parse response, enforce timeout rules
};

inline ApiMode getApiMode() { return ApiMode::FullOnline; }
