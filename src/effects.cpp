#include "effects.h"

#include <Adafruit_NeoPixel.h>

#include "game_config.h"

namespace {
constexpr uint8_t LED_PIN = 19;
constexpr uint16_t LED_COUNT = 128;
constexpr uint8_t LED_BRIGHTNESS = 64;
constexpr uint8_t AMP_ENABLE_PIN = 4;  // LOW = enable
constexpr uint8_t AUDIO_PIN = 26;       // DAC output
constexpr uint8_t BEEP_CHANNEL = 0;

constexpr uint32_t CLICK_DURATION_MS = 50;
constexpr uint32_t STATE_TONE_DURATION_MS = 150;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

FlameState lastState = ON;
bool ledcConfigured = false;
uint32_t toneEndMs = 0;

uint32_t clickEndMs = 0;

uint32_t colorForState(FlameState state) {
  switch (state) {
    case READY:
      return strip.Color(0, 32, 0);
    case ACTIVE:
      return strip.Color(255, 64, 0);
    case ARMING:
      return strip.Color(255, 180, 0);
    case ARMED:
      return strip.Color(255, 0, 0);
    case DEFUSED:
      return strip.Color(0, 0, 255);
    case DETONATED:
      return strip.Color(255, 0, 255);
    case ERROR_STATE:
      return strip.Color(255, 0, 0);
    case ON:
    default:
      return strip.Color(8, 8, 8);
  }
}

void fillStrip(uint32_t color) {
  for (uint16_t i = 0; i < LED_COUNT; ++i) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void ensureToneSetup() {
  if (ledcConfigured) {
    return;
  }
  ledcSetup(BEEP_CHANNEL, 2000, 8);
  ledcAttachPin(AUDIO_PIN, BEEP_CHANNEL);
  ledcConfigured = true;
}

void startTone(uint16_t frequency, uint32_t durationMs) {
  ensureToneSetup();
  digitalWrite(AMP_ENABLE_PIN, LOW);
  ledcWriteTone(BEEP_CHANNEL, frequency);
  toneEndMs = millis() + durationMs;
}

void stopTone() {
  ledcWriteTone(BEEP_CHANNEL, 0);
  digitalWrite(AMP_ENABLE_PIN, HIGH);
  toneEndMs = 0;
}
}

namespace effects {
void init() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();

  pinMode(AMP_ENABLE_PIN, OUTPUT);
  digitalWrite(AMP_ENABLE_PIN, HIGH);
}

void update() {
  const uint32_t now = millis();

  if (toneEndMs != 0 && now >= toneEndMs) {
    stopTone();
  }

  if (clickEndMs != 0 && now >= clickEndMs) {
    stopTone();
    clickEndMs = 0;
  }
}

void onStateChanged(FlameState state) {
  if (state == lastState) {
    return;
  }
  lastState = state;
  fillStrip(colorForState(state));
  startTone(1200, STATE_TONE_DURATION_MS);
}

void playKeypadClick() {
  startTone(1800, CLICK_DURATION_MS);
  clickEndMs = millis() + CLICK_DURATION_MS;
}
}  // namespace effects

