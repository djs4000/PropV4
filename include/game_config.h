#pragma once

#include <Arduino.h>

// Default gameplay configuration. Values are applied on first boot and can be
// overridden by persisted Preferences values. These defaults follow agents.md.
#ifdef APP_DEBUG
constexpr uint32_t API_POST_INTERVAL_MS = 500;      // 5s for testing
#else
constexpr uint32_t API_POST_INTERVAL_MS = 500;      // 500ms for live
#endif
constexpr uint32_t API_TIMEOUT_MS = 10000;           // 10s
constexpr uint32_t BUTTON_HOLD_MS = 3000;            // 3s for arming/reset
constexpr uint32_t IR_CONFIRM_WINDOW_MS = 5000;      // 5s IR confirmation window
constexpr uint8_t DEFUSE_CODE_LENGTH = 4;            // digits
constexpr uint8_t MAX_WIFI_RETRIES = 10;             // WiFi attempts

constexpr uint32_t DEFAULT_BOMB_DURATION_MS = 40000; // example: 40s

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

// LED matrix configuration for the WS2812B strip wrapped as a cylinder. The
// current build is 9 rows high and 8 columns around with no leading pixels
// before the matrix.
constexpr uint8_t LED_MATRIX_ROWS = 9;    // height
constexpr uint8_t LED_MATRIX_COLS = 8;    // circumference columns
constexpr uint16_t LED_COUNT = LED_MATRIX_ROWS * LED_MATRIX_COLS;

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Tunable colors per flame state.
constexpr RgbColor COLOR_READY = {200, 150, 40};       // soft yellow
constexpr RgbColor COLOR_ACTIVE = {30, 200, 60};       // green
constexpr RgbColor COLOR_ARMING = {255, 180, 40};      // orange/yellow
constexpr RgbColor COLOR_ARMED = {220, 40, 20};        // red
constexpr RgbColor COLOR_DEFUSED = {30, 120, 255};     // blue
constexpr RgbColor COLOR_DETONATED = {255, 0, 0};      // bright red
constexpr RgbColor COLOR_ERROR = {200, 0, 0};          // solid red
constexpr RgbColor COLOR_BOOT = {255, 200, 40};        // bright yellow for boot flash

// Timing controls for LED/audio effects.
constexpr uint32_t EFFECTS_FRAME_INTERVAL_MS = 30;       // base frame cadence
constexpr uint32_t COUNTDOWN_BEEP_INTERVAL_MS = 1000;      // 1s beeps (slow cadence)
constexpr uint32_t COUNTDOWN_BEEP_START_THRESHOLD_MS = 11000; // begin slow beeps when remaining is under this
constexpr uint32_t COUNTDOWN_BEEP_FAST_INTERVAL_MS = 500;  // faster cadence near zero
constexpr uint32_t COUNTDOWN_BEEP_FAST_THRESHOLD_MS = 6000; // switch to fast cadence when remaining is below this
constexpr uint32_t DETONATED_EFFECT_DURATION_MS = 10000;
constexpr uint32_t DEFUSED_EFFECT_DURATION_MS = 5000;
