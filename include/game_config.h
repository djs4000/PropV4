#pragma once

#include <Arduino.h>


#define API_DEBUG_LOGGING 1
// =====================================================================================
// Hardware Pinout Configuration
// =====================================================================================
// This section defines the GPIO pins used for various hardware components.

constexpr uint8_t I2C_SDA_PIN = 23 ;           // I2C data line for keypad and buttons (blue wire)
constexpr uint8_t I2C_SCL_PIN = 27;           // I2C clock line for keypad and buttons (white wire)
constexpr uint8_t IR_PIN = 35;                // IR receiver input pin (yellow wire)
constexpr uint8_t LED_PIN = 19;               // WS2812B LED strip data pin (green wire)
constexpr uint8_t AMP_ENABLE_PIN = 4;         // Audio amplifier enable pin (LOW to enable)
constexpr uint8_t AUDIO_PIN = 26;             // DAC output pin for audio
constexpr uint8_t BACKLIGHT_PIN = 21;         // TFT display backlight control pin

// =====================================================================================
// I2C Configuration
// =====================================================================================
// Defines I2C addresses and communication speed.

constexpr uint8_t KEYPAD_ADDR = 0x20;         // PCF8574 address for 4x4 keypad
constexpr uint8_t BUTTON_ADDR = 0x21;         // PCF8574 address for dual buttons
constexpr uint32_t I2C_FREQ = 50000;         // I2C bus frequency (100kHz)

// =====================================================================================
// Gameplay & Core Logic Configuration
// =================================================================1====================
// These values control the core gameplay mechanics and timings.

// Default gameplay configuration. Values are applied on first boot and can be
// overridden by persisted Preferences values. These defaults follow agents.md.
#ifdef APP_DEBUG
constexpr uint32_t API_POST_INTERVAL_MS = 500;       // 5s for testing
#else
constexpr uint32_t API_POST_INTERVAL_MS = 500;       // 500ms for live
#endif
constexpr uint32_t API_TIMEOUT_MS = 10000;            // 10s before API is considered offline
constexpr uint32_t BUTTON_HOLD_MS = 3000;             // 3s hold for arming/reset
constexpr uint32_t IR_CONFIRM_WINDOW_MS = 5000;       // 5s window to receive IR confirmation
constexpr uint8_t DEFUSE_CODE_LENGTH = 4;             // Number of digits in the defuse code
constexpr uint8_t MAX_WIFI_RETRIES = 3;              // WiFi connection attempts before failing
constexpr uint32_t DEFAULT_BOMB_DURATION_MS = 40000;  // Default bomb countdown time (e.g., 40s)

// Placeholder default defuse code used until Preferences or web UI override it.
static constexpr const char *DEFAULT_DEFUSE_CODE = "1234";

// =====================================================================================
// Network Configuration
// =====================================================================================
// Settings for WiFi, SoftAP, and the backend API endpoint.

// Optional default API endpoint. Replace with the real backend URL when known.
static constexpr const char *DEFAULT_API_ENDPOINT = "http://192.168.0.2:9055/prop";

// SoftAP configuration used when station mode fails and the device enters the
// configuration portal. The SSID is generated at runtime using the prefix and
// the last bytes of the MAC address for uniqueness.
static constexpr const char *SOFTAP_SSID_PREFIX = "DigitalFlame-";
static constexpr const char *SOFTAP_PASSWORD = "digitalflame";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;  // Timeout for each WiFi connection attempt

// Controls how the device interacts with the backend API. Additional configurability
// will be added later; for now the mode is fixed to TestSendOnly.
enum class ApiMode {
  Disabled,      // no HTTP calls, maybe just log JSON
  TestSendOnly,  // send POST, ignore response and never trigger errors
  FullOnline     // send POST, parse response, enforce timeout rules
};

inline ApiMode getApiMode() { return ApiMode::FullOnline; }

// =====================================================================================
// LED & Display Configuration
// =====================================================================================
// Configuration for the WS2812B LED matrix and TFT display.

// LED matrix configuration for the WS2812B strip wrapped as a cylinder. The
// current build is 9 rows high and 8 columns around with no leading pixels
// before the matrix.
constexpr uint8_t LED_MATRIX_ROWS = 14;     // Height of the LED matrix
constexpr uint8_t LED_MATRIX_COLS = 8;      // Circumference columns of the LED matrix
constexpr uint16_t LED_COUNT = LED_MATRIX_ROWS * LED_MATRIX_COLS;
constexpr uint8_t LED_BRIGHTNESS = 250;     // LED brightness (0-255)

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Tunable colors per flame state.
constexpr RgbColor COLOR_READY = {255, 255, 0};      // Yellow
constexpr RgbColor COLOR_ACTIVE = {0, 255, 0};         // Green
constexpr RgbColor COLOR_ARMING = {255, 77, 0};        // Orange/Yellow
constexpr RgbColor COLOR_ARMED = {255, 0, 0};          // Red
constexpr RgbColor COLOR_DEFUSED = {30, 120, 255};     // Blue
constexpr RgbColor COLOR_DETONATED = {255, 0, 0};      // Bright Red
constexpr RgbColor COLOR_ERROR = {75, 0, 130};         // Indigo/Purple
constexpr RgbColor COLOR_BOOT = {255, 255, 255};      // Bright white for boot flash

// TFT display layout and text sizes
constexpr uint32_t UI_FRAME_INTERVAL_MS = 1000 / 24;  // Target UI refresh rate (~24 FPS)
constexpr uint8_t TITLE_TEXT_SIZE = 2;
constexpr uint8_t TIMER_TEXT_SIZE = 5;
constexpr int16_t TIMER_CLEAR_HEIGHT = 72;
constexpr uint8_t STATUS_TEXT_SIZE = 2;
constexpr int16_t STATUS_CLEAR_HEIGHT = 36;
constexpr uint8_t BOOT_DETAIL_TEXT_SIZE = 1;
constexpr uint8_t CODE_TEXT_SIZE = 2;
constexpr int16_t TITLE_Y = 20;
constexpr int16_t TIMER_Y = 80;
constexpr int16_t STATUS_Y = 150;
constexpr int16_t BAR_Y = 185;
constexpr int16_t BAR_WIDTH = 200;
constexpr int16_t BAR_HEIGHT = 16;
constexpr int16_t BAR_BORDER = 2;
constexpr int16_t CODE_Y = 260;

// =====================================================================================
// Effects & Audio Configuration
// =====================================================================================
// Timing and parameters for LED animations and audio feedback.

constexpr uint32_t EFFECTS_FRAME_INTERVAL_MS = 50;         // Base cadence for LED effect updates

// Countdown beeps
constexpr uint32_t COUNTDOWN_BEEP_INTERVAL_MS = 1000;        // 1s beeps (slow cadence)
constexpr uint32_t COUNTDOWN_BEEP_START_THRESHOLD_MS = 11000; // begin slow beeps when remaining is under this
constexpr uint32_t COUNTDOWN_BEEP_FAST_INTERVAL_MS = 500;    // faster cadence near zero
constexpr uint32_t COUNTDOWN_BEEP_FAST_THRESHOLD_MS = 5500; // switch to fast cadence when remaining is below this
constexpr uint32_t COUNTDOWN_BEEP_FASTEST_INTERVAL_MS = 250;
constexpr uint32_t COUNTDOWN_BEEP_FASTEST_THRESHOLD_MS = 3250;
constexpr uint16_t COUNTDOWN_BEEP_DURATION_MS = 75;
constexpr uint8_t COUNTDOWN_BEEP_VOLUME = 255;

// Audio settings
constexpr uint8_t AUDIO_CHANNEL = 0;
constexpr uint8_t AUDIO_RES_BITS = 8;
constexpr uint16_t IR_CONFIRM_PROMPT_BEEP_MS = 120;
constexpr uint16_t IR_CONFIRM_PROMPT_BEEP_FREQ = 1500;
constexpr uint16_t WRONG_CODE_TONE_MS = 220;
constexpr uint16_t WRONG_CODE_TONE_FREQ_HZ = 90;
constexpr uint16_t WRONG_CODE_GAP_MS = 140;

// Effect durations
#ifdef APP_DEBUG
constexpr uint32_t DETONATED_EFFECT_DURATION_MS = 5000;
constexpr uint32_t DEFUSED_EFFECT_DURATION_MS = 5000;
#else
constexpr uint32_t DETONATED_EFFECT_DURATION_MS = 10000;
constexpr uint32_t DEFUSED_EFFECT_DURATION_MS = 5000;
#endif

// =====================================================================================
// Input Configuration
// =====================================================================================
// Debounce timings and keypad mapping.

constexpr uint32_t KEY_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

// Key map for the 4x4 matrix keypad, rotated to match physical wiring.
constexpr char KEY_MAP[4][4] = {{'1', '4', '7', '*'},
                                {'2', '5', '8', '0'},
                                {'3', '6', '9', '#'},
                                {'A', 'B', 'C', 'D'}};
