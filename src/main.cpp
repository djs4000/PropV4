#include <Arduino.h>

#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"
#include "util.h"
#include "wifi_config.h"

// Cooperative scheduler timestamps
static unsigned long lastStateUpdateMs = 0;
static uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;
static bool mainScreenInitialized = false;
static FlameState lastRenderedState = ON;
static uint32_t lastRenderedRemainingMs = 0;
static float lastRenderedArmingProgress = -1.0f;
static bool configScreenRendered = false;

static void renderBootScreenIfNeeded() {
  if (mainScreenInitialized) {
    return;
  }

  const bool wifiFailed = network::isConfigPortalActive() || network::hasWifiFailedPermanently();
  ui::renderBootScreen(network::getConfiguredWifiSsid(), network::isWifiConnected(), wifiFailed,
                       network::getConfigPortalSsid(), network::getConfigPortalAddress(),
                       network::getWifiIpString(), network::getConfiguredApiEndpoint(),
                       network::hasReceivedApiResponse());
}

static void renderMainUiIfNeeded(FlameState state) {
  const unsigned long now = millis();

  float armingProgress = 0.0f;
  if (state == ARMING && isButtonHoldActive() && getButtonHoldStartMs() != 0) {
    const uint32_t elapsed = now - getButtonHoldStartMs();
    armingProgress = static_cast<float>(elapsed) / static_cast<float>(BUTTON_HOLD_MS);
    armingProgress = constrain(armingProgress, 0.0f, 1.0f);
  }
  const uint32_t remainingMs = configuredBombDurationMs;  // Countdown hook will update this later.

  bool shouldRender = false;

  if (state == READY && !mainScreenInitialized) {
    ui::initMainScreen();
    mainScreenInitialized = true;
    shouldRender = true;
  }

  if (!mainScreenInitialized) {
    return;
  }

  if (state != lastRenderedState || remainingMs != lastRenderedRemainingMs ||
      armingProgress != lastRenderedArmingProgress) {
    shouldRender = true;
  }

  if (shouldRender) {
    const uint8_t enteredDigits = (state == ARMED) ? getEnteredDigits() : 0;
    ui::renderState(state, configuredBombDurationMs, remainingMs, armingProgress, DEFUSE_CODE_LENGTH,
                    enteredDigits);
    lastRenderedState = state;
    lastRenderedRemainingMs = remainingMs;
    lastRenderedArmingProgress = armingProgress;
  }
}

#ifdef DEBUG
// Allows manual state overrides for testing API POST behavior without waiting on
// backend-driven transitions.
static void handleDebugSerialStateChange() {
  if (!Serial.available()) {
    return;
  }

  const int incoming = Serial.read();
  FlameState requestedState = getState();
  bool shouldUpdate = true;

  switch (incoming) {
    case '0':
      requestedState = ON;
      break;
    case '1':
      requestedState = READY;
      break;
    case '2':
      requestedState = ACTIVE;
      break;
    case '3':
      requestedState = ARMING;
      break;
    case '4':
      requestedState = ARMED;
      break;
    case '5':
      requestedState = DEFUSED;
      break;
    case '6':
      requestedState = DETONATED;
      break;
    case '7':
      requestedState = ERROR_STATE;
      break;
    default:
      shouldUpdate = false;
      break;
  }

  if (shouldUpdate) {
    setState(requestedState);
    renderMainUiIfNeeded(getState());
  }
}
#endif

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  while (!Serial) {
    // Wait for serial to be ready (non-blocking for ESP32, loop exits immediately).
    break;
  }
#endif

  // Initial state on boot
  setState(ON);

  effects::initEffects();
  effects::startStartupTest();
  effects::startStartupBeep();

  initInputs();
  ui::initUI();
  network::beginWifi();
  configuredBombDurationMs = network::getConfiguredBombDurationMs();
}

void loop() {
  const unsigned long now = millis();

  // Keep the cached bomb duration aligned with any persisted updates.
  configuredBombDurationMs = network::getConfiguredBombDurationMs();

  updateInputs();

  // Startup hardware self-checks (LEDs + audio) remain non-blocking via effects::updateEffects().
  effects::updateEffects();

  renderBootScreenIfNeeded();

  ui::updateUI();

  // Networking cadence is controlled internally based on API_POST_INTERVAL_MS.
  network::updateWifi();

  if (network::isConfigPortalActive()) {
    if (!configScreenRendered) {
      ui::renderConfigPortalScreen(network::getConfigPortalSsid(), network::getConfigPortalPassword());
      configScreenRendered = true;
    }
  } else if (configScreenRendered) {
    // Force boot/main UI to refresh after leaving config mode.
    configScreenRendered = false;
    mainScreenInitialized = false;
  }

  // Always service the configuration web server so LAN clients can load the page
  // even when STA mode is connected.
  network::updateConfigPortal();

#ifdef DEBUG
  handleDebugSerialStateChange();
#endif

  if (network::isWifiConnected()) {
    network::updateApi();
  }

  // Drive WiFi-dependent state changes without blocking the loop.
  if (getState() == ON) {
    if (network::hasReceivedApiResponse()) {
      setState(READY);
      renderMainUiIfNeeded(getState());
    } else if (network::hasWifiFailedPermanently()) {
      setState(ERROR_STATE);
      renderMainUiIfNeeded(getState());
    }
  }

  // State machine should run frequently but cheaply; simple guard in case
  // future logic needs throttling.
  if (now - lastStateUpdateMs >= 10) {
    lastStateUpdateMs = now;
    updateState();
    renderMainUiIfNeeded(getState());
  }
}
