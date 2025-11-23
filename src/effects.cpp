#include "effects.h"

#include <Adafruit_NeoPixel.h>

namespace {
constexpr uint8_t LED_PIN = 19;
constexpr uint16_t LED_COUNT = 128;
constexpr uint8_t LED_BRIGHTNESS = 64;
constexpr uint32_t STARTUP_TEST_DURATION_MS = 1000;
constexpr uint8_t AMP_ENABLE_PIN = 4;     // LOW = enable
constexpr uint8_t AUDIO_PIN = 26;          // DAC output
constexpr uint8_t BEEP_CHANNEL = 0;
constexpr uint32_t BEEP_FREQUENCY_HZ = 2000;
constexpr uint8_t BEEP_DUTY = 180;
constexpr uint32_t STARTUP_BEEP_DURATION_MS = 200;
constexpr uint32_t CONFIRM_BEEP_DURATION_MS = 150;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool startupTestActive = false;
bool startupTestComplete = false;
unsigned long startupTestStartMs = 0;

bool startupBeepActive = false;
bool startupBeepComplete = false;
unsigned long startupBeepStartMs = 0;

bool confirmBeepActive = false;
unsigned long confirmBeepStartMs = 0;
bool ledcConfigured = false;

void beginTone() {
  if (!ledcConfigured) {
    ledcSetup(BEEP_CHANNEL, BEEP_FREQUENCY_HZ, 8);
    ledcAttachPin(AUDIO_PIN, BEEP_CHANNEL);
    ledcConfigured = true;
  }
  ledcWrite(BEEP_CHANNEL, BEEP_DUTY);
}

void endTone() { ledcWrite(BEEP_CHANNEL, 0); }
}

namespace effects {
void initEffects() {
  // Initialize LED strip
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();

  // Configure amplifier control
  pinMode(AMP_ENABLE_PIN, OUTPUT);
  digitalWrite(AMP_ENABLE_PIN, HIGH);  // Disable amp until needed
}

void startStartupTest() {
  // Simple LED self-check: turn all LEDs on, then off after a short duration.
  startupTestActive = true;
  startupTestComplete = false;
  startupTestStartMs = millis();

  for (uint16_t i = 0; i < LED_COUNT; ++i) {
    strip.setPixelColor(i, strip.Color(255, 150, 0));  // Warm orange to mimic flame
  }
  strip.show();
}

void updateStartupTest() {
  if (!startupTestActive || startupTestComplete) {
    return;
  }

  const unsigned long now = millis();
  if (now - startupTestStartMs >= STARTUP_TEST_DURATION_MS) {
    strip.clear();
    strip.show();
    startupTestActive = false;
    startupTestComplete = true;
  }
}

void startStartupBeep() {
  startupBeepActive = true;
  startupBeepComplete = false;
  startupBeepStartMs = millis();

  // Enable the amplifier and start tone playback.
  digitalWrite(AMP_ENABLE_PIN, LOW);
  beginTone();
}

void updateStartupBeep() {
  if (!startupBeepActive || startupBeepComplete) {
    return;
  }

  const unsigned long now = millis();
  if (now - startupBeepStartMs >= STARTUP_BEEP_DURATION_MS) {
    endTone();
    digitalWrite(AMP_ENABLE_PIN, HIGH);  // Disable amp after the beep
    startupBeepActive = false;
    startupBeepComplete = true;
  }
}

void startConfirmationBeep() {
  confirmBeepActive = true;
  confirmBeepStartMs = millis();

  digitalWrite(AMP_ENABLE_PIN, LOW);
  beginTone();
}

void updateConfirmationBeep() {
  if (!confirmBeepActive) {
    return;
  }

  const unsigned long now = millis();
  if (now - confirmBeepStartMs >= CONFIRM_BEEP_DURATION_MS) {
    endTone();
    digitalWrite(AMP_ENABLE_PIN, HIGH);
    confirmBeepActive = false;
  }
}

void updateEffects() {
  // Run startup self-checks; future animations will also update here.
  updateStartupTest();
  updateStartupBeep();
  updateConfirmationBeep();
}
}  // namespace effects
