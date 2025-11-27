#include <Arduino.h>
#include <IRremote.hpp>

#include "core/scheduler.h"
#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"
#include "util.h"
#include "wifi_config.h"

static uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;
static bool mainScreenInitialized = false;
static FlameState lastRenderedState = ON;
static uint32_t lastRenderedRemainingMs = 0;
static uint32_t lastRenderedRemainingCs = 0;
static float lastRenderedArmingProgress = -1.0f;
static String lastRenderedDefuseBuffer;
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
  const bool awaitingIrConfirm = state == ARMING && isIrConfirmationWindowActive();
  const String defuseBuffer = (state == ARMED) ? String(getDefuseBuffer()) : String("");
  uint32_t remainingMs = configuredBombDurationMs;
  if (state == ARMED && isBombTimerActive()) {
    remainingMs = getBombTimerRemainingMs();
  } else if (isGameTimerValid()) {
    remainingMs = getGameTimerRemainingMs();
  }
  const uint32_t remainingSeconds = remainingMs / 1000;
  const uint32_t lastRenderedSeconds = lastRenderedRemainingMs / 1000;
  const uint32_t remainingCentiseconds = remainingMs / 10;
  const uint32_t lastRenderedCentiseconds = lastRenderedRemainingCs;

  bool shouldRender = false;
  bool digitsChanged = false;

  if (state == ARMED) {
    digitsChanged = defuseBuffer != lastRenderedDefuseBuffer;
  } else if (!lastRenderedDefuseBuffer.isEmpty()) {
    digitsChanged = true;
  }

  if (state == READY && !mainScreenInitialized) {
    ui::initMainScreen();
    mainScreenInitialized = true;
    shouldRender = true;
  }

  if (!mainScreenInitialized) {
    return;
  }

  effects::setArmingProgress(armingProgress);

  if (awaitingIrConfirm) {
    ui::showArmingConfirmPrompt();
    lastRenderedState = state;
    lastRenderedRemainingMs = remainingMs;
    lastRenderedRemainingCs = remainingCentiseconds;
    lastRenderedArmingProgress = armingProgress;
    lastRenderedDefuseBuffer = (state == ARMED) ? defuseBuffer : String("");
    return;
  }

  const bool centisecondChangedEnough = state == ARMED && isBombTimerActive() &&
                                        (remainingCentiseconds > lastRenderedCentiseconds ?
                                             (remainingCentiseconds - lastRenderedCentiseconds) >= 5 :
                                             (lastRenderedCentiseconds - remainingCentiseconds) >= 5);

  if (state != lastRenderedState || remainingSeconds != lastRenderedSeconds || centisecondChangedEnough ||
      armingProgress != lastRenderedArmingProgress || digitsChanged) {
    shouldRender = true;
  }

  if (shouldRender) {
    const uint8_t enteredDigits = (state == ARMED) ? getEnteredDigits() : 0;
    ui::renderState(state, configuredBombDurationMs, remainingMs, armingProgress, DEFUSE_CODE_LENGTH,
                    enteredDigits, getDefuseBuffer());
    lastRenderedState = state;
    lastRenderedRemainingMs = remainingMs;
    lastRenderedRemainingCs = remainingCentiseconds;
    lastRenderedArmingProgress = armingProgress;
    lastRenderedDefuseBuffer = (state == ARMED) ? defuseBuffer : String("");
  }
}

#ifdef APP_DEBUG
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
      // Ensure manual activation doesn't immediately revert to READY due to a
      // non-running match status pulled from the network layer.
      setMatchStatus(Running);
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

static void handleStateTask(uint32_t) {
  updateState();

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
}

static void handleEffectsTask(uint32_t now) { effects::update(now); }

static void handleUiTask(uint32_t) {
  // Keep the cached bomb duration aligned with any persisted updates.
  configuredBombDurationMs = network::getConfiguredBombDurationMs();

  renderBootScreenIfNeeded();

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

  renderMainUiIfNeeded(getState());

#ifdef APP_DEBUG
  handleDebugSerialStateChange();
#endif
}

static void handleConfigPortalTask(uint32_t now) { network::updateConfigPortal(now, getState()); }

static void handleApiTask(uint32_t) {
  if (network::isWifiConnected()) {
    network::updateApi();
  }
}

void setup() {
#ifdef APP_DEBUG
  Serial.begin(115200);
  while (!Serial) {
    // Wait for serial to be ready (non-blocking for ESP32, loop exits immediately).
    break;
  }
#endif

  // Initial state on boot
  setState(ON);

  effects::init();
  effects::onBoot();

  initInputs();
  ui::initUI();
  network::beginWifi();
  configuredBombDurationMs = network::getConfiguredBombDurationMs();

  scheduler::addTask([](uint32_t) { updateInputs(); }, 5);
  scheduler::addTask([](uint32_t) { network::updateWifi(); }, 200);
  scheduler::addTask(handleStateTask, 10);
  scheduler::addTask(handleEffectsTask, 42);
  scheduler::addTask(handleUiTask, 42);
  scheduler::addTask(handleConfigPortalTask, 200);
  scheduler::addTask(handleApiTask, API_POST_INTERVAL_MS);
}

void loop() {
  scheduler::run();
}
