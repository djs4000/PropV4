#include "effects.h"

#include <Adafruit_NeoPixel.h>
#include <cmath>

#include "core/game_state.h"
#include "game_config.h"
#include "state_machine.h"

namespace {
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
int lastCountdownBeepSecond = -1;
uint32_t lastCountdownPulseMs = 0;
uint32_t nextCountdownCueMs = 0;
uint16_t activeCountdownPulseDurationMs = 0;
int countdownCuesRemaining = 0;

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

struct ChimeState {
  bool active = false;
  uint8_t step = 0;
  uint32_t nextBeepMs = 0;
};
ChimeState defusedChime;

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
  // Updated for 14 rows (Indices 0-111)
  // Zig-Zag pattern: Col 0 goes 0->13, Col 1 goes 14->27, etc.
  static const uint16_t bottomRowIndices[LED_MATRIX_COLS] = {13, 14, 41, 42, 69, 70, 97, 98};
  static const uint16_t topRowIndices[LED_MATRIX_COLS]    = {0, 27, 28, 55, 56, 83, 84, 111};
  
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
    //digitalWrite(AMP_ENABLE_PIN, HIGH);
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

void renderCountdown(uint32_t now) {
  const uint32_t remainingMs = getGameTimerRemainingMs();
  if (remainingMs > 3000) {
    fillAll(COLOR_BOOT, 0.1f);
    lastCountdownBeepSecond = -1;  // Reset beep tracking
    lastCountdownPulseMs = 0;
    nextCountdownCueMs = 0;
    countdownCuesRemaining = 0;
    activeCountdownPulseDurationMs = 0;
  } else {
    // Final 3 seconds: low-brightness base with a short bright pulse + beep
    const uint16_t basePulseDurationMs = 150;
    const int currentSecond = static_cast<int>((remainingMs + 999) / 1000);

    // Re-sync the cue schedule if we just entered the window or the timer jumped.
    const int targetCuesRemaining = constrain(currentSecond + 1, 1, 4);
    if (countdownCuesRemaining == 0 || countdownCuesRemaining > targetCuesRemaining) {
      countdownCuesRemaining = targetCuesRemaining;
      nextCountdownCueMs = now;
    }

    if (countdownCuesRemaining > 0 && now >= nextCountdownCueMs) {
      const int cueSecond = countdownCuesRemaining - 1;
      const uint16_t beepDurationMs = cueSecond == 0 ? 300 : 150;
      activeCountdownPulseDurationMs = cueSecond == 0 ? basePulseDurationMs * 2 : basePulseDurationMs;
      effects::playBeep(1800, beepDurationMs, 255);
      lastCountdownBeepSecond = cueSecond;
      lastCountdownPulseMs = now;
      --countdownCuesRemaining;
      nextCountdownCueMs = now + 1000;
    }

    const bool shouldPulse = (lastCountdownPulseMs > 0) &&
                             (now - lastCountdownPulseMs < activeCountdownPulseDurationMs);
    fillAll(COLOR_BOOT, shouldPulse ? 1.0f : 0.1f);
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
  if (remaining <= COUNTDOWN_BEEP_FASTEST_THRESHOLD_MS) {
    interval = COUNTDOWN_BEEP_FASTEST_INTERVAL_MS;
  } else if (remaining <= COUNTDOWN_BEEP_FAST_THRESHOLD_MS) {
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
  // Short, boot-style chirp to match the arming confirmation volume for better audibility.
  effects::playBeep(1500, COUNTDOWN_BEEP_DURATION_MS, COUNTDOWN_BEEP_VOLUME);
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

  if (wrongCodeBeep.step == 2) {
    // Second beep: repeat the low growl.
    const uint16_t sawFrequencyHz = WRONG_CODE_TONE_FREQ_HZ;
    const uint16_t sawDurationMs = WRONG_CODE_TONE_MS;
    const uint8_t sawVolume = 255;
    effects::playBeep(sawFrequencyHz, sawDurationMs, sawVolume, /*sawtooth=*/true);
  }

  wrongCodeBeep.active = false;
  wrongCodeBeep.step = 0;
}

void handleDefusedChime(uint32_t now) {
  if (!defusedChime.active) {
    return;
  }

  if (toneState.active && now < toneState.endMs) {
    return;
  }

  if (now < defusedChime.nextBeepMs) {
    return;
  }

  switch (defusedChime.step) {
    case 2:
      // Play the second note of the chime
      effects::playBeep(2000, 100, 255);
      defusedChime.step = 3;
      defusedChime.nextBeepMs = toneState.endMs + 50;
      break;
    case 3:
      // Play the final, higher note
      effects::playBeep(2500, 250, 255);
      defusedChime.step = 4;
      defusedChime.nextBeepMs = toneState.endMs;
      break;
    case 4:
      // Chime is finished
      defusedChime.active = false;
      defusedChime.step = 0;
      break;
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
  digitalWrite(AMP_ENABLE_PIN, LOW);
  ledcSetup(AUDIO_CHANNEL, 1000, AUDIO_RES_BITS);
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  ledcWriteTone(AUDIO_CHANNEL, 0);
  ledcWrite(AUDIO_CHANNEL, 0);
}

void update(uint32_t now) {
  updateTone();
  handleWrongCodeBeep(now);
  handleDefusedChime(now);
  handleArmedBeeps(now, getState());

  if (now - lastFrameMs < EFFECTS_FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  const bool countdownActive = getMatchStatus() == Countdown;
  const bool countdownCuesActive = countdownActive || countdownCuesRemaining > 0;
  if (countdownCuesActive) {
    renderCountdown(now);
    strip.show();
    return;
  }

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
    // Start the triumphant defuse chime
    defusedChime.active = true;
    defusedChime.step = 1;
    playBeep(1500, 100, 255); // First note
    defusedChime.nextBeepMs = toneState.endMs + 50;
    defusedChime.step = 2;
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
  playBeep(WRONG_CODE_TONE_FREQ_HZ, WRONG_CODE_TONE_MS, 255, /*sawtooth=*/true);
  wrongCodeBeep.step = 2;
  wrongCodeBeep.nextBeepMs = toneState.endMs + WRONG_CODE_GAP_MS;
}

void onArmingConfirmNeeded() { playBeep(IR_CONFIRM_PROMPT_BEEP_FREQ, IR_CONFIRM_PROMPT_BEEP_MS, 200); }

void onArmingConfirmed() { playBeep(2200, 200); }

void setArmingProgress(float progress01) { armingProgress01 = constrain(progress01, 0.0f, 1.0f); }

uint16_t getWrongCodeBeepDurationMs() {
  return (WRONG_CODE_TONE_MS * 2) + WRONG_CODE_GAP_MS;
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
    //digitalWrite(AMP_ENABLE_PIN, LOW);
    return;
  }

  ledcWriteTone(AUDIO_CHANNEL, toneState.frequency);
  ledcWrite(AUDIO_CHANNEL, toneState.volume);
  //digitalWrite(AMP_ENABLE_PIN, LOW);
}
}  // namespace effects
