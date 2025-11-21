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
static uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;  // TODO: load from NVS
static bool mainScreenInitialized = false;
static FlameState lastRenderedState = ON;
static uint32_t lastRenderedRemainingMs = 0;
static float lastRenderedArmingProgress = -1.0f;
static bool bootScreenShown = false;

static void renderMainUiIfNeeded(FlameState state) {
  // Placeholder progress wiring; will be replaced with real ARMING hold tracking.
  const float armingProgress = (state == ARMING) ? 0.0f : 0.0f;
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
    ui::renderState(state, configuredBombDurationMs, remainingMs, armingProgress, DEFUSE_CODE_LENGTH, 0);
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
  // TODO: Validate that no buttons are held at boot to detect potential hardware faults.
  ui::initUI();
  ui::showBootScreen(DEFAULT_WIFI_SSID);
  bootScreenShown = true;
  network::beginWifi();
}

void loop() {
  const unsigned long now = millis();

  updateInputs();

  // Startup hardware self-checks (LEDs + audio) remain non-blocking via effects::updateEffects().
  effects::updateEffects();

  ui::updateUI();

  // Networking cadence is controlled internally based on API_POST_INTERVAL_MS.
  network::updateWifi();

  if (getState() == ON) {
    const bool wifiConnected = network::isWifiConnected();
    if (bootScreenShown) {
      ui::updateBootStatus(wifiConnected, wifiConnected ? network::getWifiIpString() : String(""));
    }
  }

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
