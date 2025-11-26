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

uint32_t lastArmedBeepMs = 0;

struct ToneState {
  bool active = false;
  uint16_t frequency = 0;
  uint32_t endMs = 0;
  uint8_t volume = 0;
  uint32_t startMs = 0;
  bool sawtooth = false;
  uint16_t periodMs = 0;
};

ToneState toneState;

struct DoubleBeepState {
  bool active = false;
  uint8_t step = 0;
  uint32_t nextBeepMs = 0;
};

DoubleBeepState wrongCodeBeep;

// Wrong-code beep cadence (high tone → short pause → mid tone → pause → 90 Hz
// sawtooth). Expose the aggregate so keypad handling can lock out entries
// while the alert plays.
constexpr uint16_t WRONG_CODE_FIRST_TONE_MS = 140;
constexpr uint16_t WRONG_CODE_GAP1_MS = 140;
constexpr uint16_t WRONG_CODE_SECOND_TONE_MS = 140;
constexpr uint16_t WRONG_CODE_GAP2_MS = 180;
constexpr uint16_t WRONG_CODE_FINAL_TONE_MS = 220;

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

  // Measured physical mapping (bottom row: 8,9,26,27,44,45,62,63 / top row:
  // 0,17,18,35,36,56,54,71) requires a column-specific interpolation rather
  // than a simple serpentine assumption. Each column uses the measured bottom
  // and top indices and interpolates linearly through the remaining rows to
  // approximate the zigzag wiring while keeping a bottom->top fill order.
  static const uint16_t bottomRowIndices[LED_MATRIX_COLS] = {8, 9, 26, 27, 44, 45, 62, 63};
  static const uint16_t topRowIndices[LED_MATRIX_COLS] = {0, 17, 18, 35, 36, 56, 54, 71};

  const float step = static_cast<float>(topRowIndices[col] - bottomRowIndices[col]) /
                     static_cast<float>(LED_MATRIX_ROWS - 1);
  const uint16_t mapped = static_cast<uint16_t>(lroundf(bottomRowIndices[col] + step * row));
  return mapped;
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

  if (toneState.sawtooth) {
    // Sawtooth amplitude ramp at the requested frequency (periodMs).
    const uint32_t elapsed = nowMs - toneState.startMs;
    const uint16_t phaseMs = toneState.periodMs == 0 ? 0 : (elapsed % toneState.periodMs);
    const float phase = toneState.periodMs == 0 ? 0.0f
                                                : static_cast<float>(phaseMs) / static_cast<float>(toneState.periodMs);
    const uint8_t duty = static_cast<uint8_t>(constrain(static_cast<float>(toneState.volume) * phase, 0.0f, 255.0f));
    ledcWriteTone(AUDIO_CHANNEL, toneState.frequency);
    ledcWrite(AUDIO_CHANNEL, duty);
    digitalWrite(AMP_ENABLE_PIN, LOW);
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

void handleArmedBeeps(uint32_t now, FlameState state) {
  if (state != ARMED || !isBombTimerActive()) {
    lastArmedBeepMs = now;
    return;
  }

  const uint32_t remaining = getBombTimerRemainingMs();
  if (remaining == 0) {
    return;
  }

  uint32_t interval = 0;
  if (remaining <= COUNTDOWN_BEEP_FAST_THRESHOLD_MS) {
    interval = COUNTDOWN_BEEP_FAST_INTERVAL_MS;
  } else if (remaining <= COUNTDOWN_BEEP_START_THRESHOLD_MS) {
    interval = COUNTDOWN_BEEP_INTERVAL_MS;
  } else {
    // Not within the audible window yet.
    lastArmedBeepMs = now;
    return;
  }

  if (toneState.active && now < toneState.endMs) {
    return;
  }

  if (now - lastArmedBeepMs < interval) {
    return;
  }

  lastArmedBeepMs = now;
  playBeep(1500, 120, 255);
}

void handleWrongCodeBeep(uint32_t now) {
  if (!wrongCodeBeep.active) {
    return;
  }

  if (toneState.active && now < toneState.endMs) {
    return;
  }

  if (now < wrongCodeBeep.nextBeepMs) {
    return;
  }

  if (wrongCodeBeep.step == 1) {
    effects::playBeep(2000, WRONG_CODE_SECOND_TONE_MS, 255);
    wrongCodeBeep.step = 2;
    wrongCodeBeep.nextBeepMs = now + WRONG_CODE_GAP2_MS;
    return;
  }

  // Second beep: low sawtooth growl.
  const uint16_t sawFrequencyHz = 90;
  const uint16_t sawDurationMs = WRONG_CODE_FINAL_TONE_MS;
  const uint8_t sawVolume = 255;
  effects::playBeep(sawFrequencyHz, sawDurationMs, sawVolume, /*sawtooth=*/true);
  wrongCodeBeep.active = false;
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
  handleWrongCodeBeep(now);
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

void onKeypadKey() { playBeep(1200, 140, 255); }

void onWrongCode() {
  wrongCodeBeep.active = true;
  wrongCodeBeep.step = 1;
  playBeep(1800, WRONG_CODE_FIRST_TONE_MS, 255);
  wrongCodeBeep.nextBeepMs = toneState.endMs + WRONG_CODE_GAP1_MS;
}

void onArmingConfirmed() { playBeep(2200, 200); }

void setArmingProgress(float progress01) { armingProgress01 = constrain(progress01, 0.0f, 1.0f); }

uint16_t getWrongCodeBeepDurationMs() {
  return WRONG_CODE_FIRST_TONE_MS + WRONG_CODE_GAP1_MS + WRONG_CODE_SECOND_TONE_MS + WRONG_CODE_GAP2_MS +
         WRONG_CODE_FINAL_TONE_MS;
}

void playBeep(uint16_t frequencyHz, uint16_t durationMs, uint8_t volume, bool sawtooth) {
  if (frequencyHz == 0 || durationMs == 0) {
    return;
  }
  toneState.active = true;
  toneState.frequency = frequencyHz;
  toneState.endMs = millis() + durationMs;
  toneState.volume = constrain(volume, static_cast<uint8_t>(0), static_cast<uint8_t>(255));
  toneState.startMs = millis();
  toneState.sawtooth = sawtooth;
  toneState.periodMs = (frequencyHz == 0) ? 0 : static_cast<uint16_t>(max<uint32_t>(1, 1000UL / frequencyHz));

  if (sawtooth) {
    ledcWriteTone(AUDIO_CHANNEL, toneState.frequency);
    ledcWrite(AUDIO_CHANNEL, 0);
    digitalWrite(AMP_ENABLE_PIN, LOW);
    return;
  }

  ledcWriteTone(AUDIO_CHANNEL, toneState.frequency);
  ledcWrite(AUDIO_CHANNEL, toneState.volume);
  digitalWrite(AMP_ENABLE_PIN, LOW);
}
}  // namespace effects
