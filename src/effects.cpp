#include "effects.h"

#include <Adafruit_NeoPixel.h>
#include <cmath>

#include "game_config.h"
#include "state_machine.h"

namespace {
constexpr uint8_t LED_PIN = 19;
constexpr uint8_t LED_BRIGHTNESS = 80;
constexpr uint8_t AMP_ENABLE_PIN = 4;  // LOW = enable
constexpr uint8_t AUDIO_PIN = 26;       // DAC output
constexpr uint8_t AUDIO_CHANNEL = 0;
constexpr uint8_t AUDIO_RES_BITS = 8;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

float armingProgress01 = 0.0f;
FlameState lastRenderedState = ON;
uint32_t lastFrameMs = 0;
uint32_t bootFlashStartMs = 0;
bool bootFlashActive = false;
uint32_t defusedStartMs = 0;
bool defusedActive = false;
uint32_t detonatedStartMs = 0;
bool detonatedActive = false;

uint32_t lastCountdownBeepMs = 0;
bool countdownFinalBeepDone = false;
uint32_t lastArmedBeepMs = 0;

struct ToneState {
  bool active = false;
  uint16_t frequency = 0;
  uint32_t endMs = 0;
  uint8_t volume = 0;
};

ToneState toneState;

uint32_t colorToPixel(const RgbColor &c, float scale = 1.0f) {
  scale = constrain(scale, 0.0f, 1.0f);
  const uint8_t r = static_cast<uint8_t>(static_cast<float>(c.r) * scale);
  const uint8_t g = static_cast<uint8_t>(static_cast<float>(c.g) * scale);
  const uint8_t b = static_cast<uint8_t>(static_cast<float>(c.b) * scale);
  return strip.Color(r, g, b);
}

void fillAll(const RgbColor &color, float scale = 1.0f) {
  const uint32_t pixelColor = colorToPixel(color, scale);
  for (uint16_t i = 0; i < LED_COUNT; ++i) {
    strip.setPixelColor(i, pixelColor);
  }
}

uint16_t mapRowColToIndex(uint8_t row, uint8_t col) {
  if (row >= LED_MATRIX_ROWS || col >= LED_MATRIX_COLS) {
    return 0;
  }
  const bool reverse = (row % 2) == 1;  // serpentine wiring around the cylinder
  const uint8_t mappedCol = reverse ? (LED_MATRIX_COLS - 1 - col) : col;
  return static_cast<uint16_t>(row) * LED_MATRIX_COLS + mappedCol;
}

void updateTone() {
  const uint32_t nowMs = millis();

  if (!toneState.active) {
    return;
  }

  if (nowMs >= toneState.endMs) {
    ledcWriteTone(AUDIO_CHANNEL, 0);
    ledcWrite(AUDIO_CHANNEL, 0);
    digitalWrite(AMP_ENABLE_PIN, HIGH);
    toneState.active = false;
    return;
  }
}

void playBootFlash(uint32_t now) {
  const uint32_t elapsed = now - bootFlashStartMs;
  if (elapsed < 250) {
    fillAll(COLOR_BOOT, 1.0f);
  } else if (elapsed < 600) {
    const float t = 1.0f - (static_cast<float>(elapsed - 250) / 350.0f);
    fillAll(COLOR_BOOT, constrain(t, 0.0f, 1.0f));
  } else {
    bootFlashActive = false;
  }
}

void renderReady(uint32_t now) {
  static uint16_t phase = 0;
  static uint32_t lastStepMs = 0;
  if (now - lastStepMs >= 120) {
    phase = (phase + 1) % LED_MATRIX_COLS;
    lastStepMs = now;
  }
  for (uint8_t row = 0; row < LED_MATRIX_ROWS; ++row) {
    for (uint8_t col = 0; col < LED_MATRIX_COLS; ++col) {
      const bool highlight = (col == phase);
      const float scale = highlight ? 0.4f : 0.05f;
      strip.setPixelColor(mapRowColToIndex(row, col), colorToPixel(COLOR_READY, scale));
    }
  }
}

void renderActive(uint32_t now) {
  const float phase = (static_cast<float>(now % 2000) / 2000.0f);
  const float wave = 0.3f + 0.7f * (1.0f - fabsf(2.0f * phase - 1.0f));
  fillAll(COLOR_ACTIVE, wave);
}

void renderArming() {
  const uint8_t litRows = static_cast<uint8_t>(ceilf(constrain(armingProgress01, 0.0f, 1.0f) * LED_MATRIX_ROWS));
  for (uint8_t row = 0; row < LED_MATRIX_ROWS; ++row) {
    const bool rowLit = row < litRows;
    const float scale = rowLit ? 0.8f : 0.05f;
    for (uint8_t col = 0; col < LED_MATRIX_COLS; ++col) {
      strip.setPixelColor(mapRowColToIndex(row, col), colorToPixel(COLOR_ARMING, scale));
    }
  }
}

void renderArmed(uint32_t now) {
  const bool on = ((now / 300) % 2) == 0;
  fillAll(COLOR_ARMED, on ? 0.8f : 0.05f);
}

void renderDefused(uint32_t now) {
  if (!defusedActive) {
    fillAll(COLOR_DEFUSED, 0.0f);
    return;
  }
  const uint32_t elapsed = now - defusedStartMs;
  if (elapsed >= DEFUSED_EFFECT_DURATION_MS) {
    defusedActive = false;
    fillAll(COLOR_DEFUSED, 0.0f);
    return;
  }
  const float t = 1.0f - (static_cast<float>(elapsed) / static_cast<float>(DEFUSED_EFFECT_DURATION_MS));
  fillAll(COLOR_DEFUSED, t);
}

void renderDetonated(uint32_t now) {
  if (!detonatedActive) {
    fillAll(COLOR_DETONATED, 0.0f);
    return;
  }
  const uint32_t elapsed = now - detonatedStartMs;
  if (elapsed >= DETONATED_EFFECT_DURATION_MS) {
    detonatedActive = false;
    fillAll(COLOR_DETONATED, 0.0f);
    return;
  }
  const bool on = ((now / 120) % 2) == 0;
  fillAll(COLOR_DETONATED, on ? 1.0f : 0.0f);
}

void renderError(uint32_t now) {
  const float phase = (static_cast<float>(now % 3000) / 3000.0f);
  const float wave = 0.1f + 0.6f * (1.0f - fabsf(2.0f * phase - 1.0f));
  fillAll(COLOR_ERROR, wave);
}

void handleCountdownBeeps(uint32_t now) {
  if (getMatchStatus() != Countdown || !isGameTimerValid()) {
    countdownFinalBeepDone = false;
    lastCountdownBeepMs = 0;
    return;
  }

  const uint32_t remaining = getGameTimerRemainingMs();

  // Only beep during the final 5 seconds of the countdown.
  if (remaining > 5000) {
    lastCountdownBeepMs = 0;
    countdownFinalBeepDone = false;
    return;
  }

  if (remaining <= 120) {
    if (!countdownFinalBeepDone) {
      effects::playBeep(1900, 220, 200);
      countdownFinalBeepDone = true;
    }
    return;
  }

  countdownFinalBeepDone = false;
  if (now - lastCountdownBeepMs >= COUNTDOWN_BEEP_INTERVAL_MS) {
    effects::playBeep(1900, 140, 200);
    lastCountdownBeepMs = now;
  }
}

void handleArmedBeeps(uint32_t now, FlameState state) {
  if (state != ARMED || !isBombTimerActive()) {
    return;
  }
  const uint32_t remaining = getBombTimerRemainingMs();
  uint32_t interval = 0;
  if (remaining <= 3000) {
    interval = COUNTDOWN_BEEP_FAST_INTERVAL_MS;
  } else if (remaining <= 10000) {
    interval = COUNTDOWN_BEEP_INTERVAL_MS;
  }
  if (interval > 0 && now - lastArmedBeepMs >= interval) {
    effects::playBeep(2000, 150);
    lastArmedBeepMs = now;
  }
}
}  // namespace

namespace effects {
void init() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();

  pinMode(AMP_ENABLE_PIN, OUTPUT);
  digitalWrite(AMP_ENABLE_PIN, HIGH);
  ledcSetup(AUDIO_CHANNEL, 1000, AUDIO_RES_BITS);
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  ledcWriteTone(AUDIO_CHANNEL, 0);
  ledcWrite(AUDIO_CHANNEL, 0);
}

void update(uint32_t now) {
  updateTone();
  handleCountdownBeeps(now);
  handleArmedBeeps(now, getState());

  if (now - lastFrameMs < EFFECTS_FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  const FlameState state = getState();
  switch (state) {
    case ON:
      if (bootFlashActive) {
        playBootFlash(now);
      } else {
        fillAll(COLOR_BOOT, 0.02f);
      }
      break;
    case READY:
      renderReady(now);
      break;
    case ACTIVE:
      renderActive(now);
      break;
    case ARMING:
      renderArming();
      break;
    case ARMED:
      renderArmed(now);
      break;
    case DEFUSED:
      renderDefused(now);
      break;
    case DETONATED:
      renderDetonated(now);
      break;
    case ERROR_STATE:
      renderError(now);
      break;
  }

  lastRenderedState = state;
  strip.show();
}

void onBoot() {
  bootFlashStartMs = millis();
  bootFlashActive = true;
  playBeep(1500, 120, 160);
}

void onStateChanged(FlameState oldState, FlameState newState) {
  lastRenderedState = newState;
  if (newState != ARMING) {
    armingProgress01 = 0.0f;
  }

  if (newState == DEFUSED) {
    defusedActive = true;
    defusedStartMs = millis();
    playBeep(1600, 220);
  } else if (newState == DETONATED) {
    detonatedActive = true;
    detonatedStartMs = millis();
    playBeep(900, DETONATED_EFFECT_DURATION_MS / 2, 255);
  } else if (newState == ERROR_STATE) {
    playBeep(500, 400);
  } else if (newState == READY && oldState == ON) {
    bootFlashStartMs = millis();
    bootFlashActive = true;
  }
}

void onKeypadKey() { playBeep(2300, 50, 160); }

void onArmingConfirmed() { playBeep(2200, 200); }

void setArmingProgress(float progress01) { armingProgress01 = constrain(progress01, 0.0f, 1.0f); }

void playBeep(uint16_t frequencyHz, uint16_t durationMs, uint8_t volume) {
  if (frequencyHz == 0 || durationMs == 0) {
    return;
  }
  toneState.active = true;
  toneState.frequency = frequencyHz;
  toneState.endMs = millis() + durationMs;
  toneState.volume = constrain(volume, static_cast<uint8_t>(0), static_cast<uint8_t>(255));
  ledcWriteTone(AUDIO_CHANNEL, toneState.frequency);
  ledcWrite(AUDIO_CHANNEL, toneState.volume);
  digitalWrite(AMP_ENABLE_PIN, LOW);
}
}  // namespace effects
