#include <Arduino.h>
#include <IRremote.hpp>

#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"
#include "util.h"
#include "wifi_config.h"

// Cooperative scheduler timestamps
static uint32_t lastInputsUpdateMs = 0;
static uint32_t lastStateUpdateMs = 0;
static uint32_t lastUiUpdateMs = 0;
static uint32_t lastEffectsUpdateMs = 0;
static uint32_t lastApiUpdateMs = 0;
static uint32_t lastWifiUpdateMs = 0;
static uint32_t lastWebUiUpdateMs = 0;

static uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;
static UiThemeConfig themeConfig = ui::defaultUiTheme();

static GameInputs mapSnapshotToInputs(const InputSnapshot &snapshot) {
  GameInputs inputs{};
  inputs.bothButtonsPressed = snapshot.bothButtonsPressed;
  inputs.irConfirmationReceived = snapshot.irConfirmationReceived;
  inputs.hasKeypadDigit = snapshot.hasKeypadDigit;
  inputs.keypadDigit = snapshot.keypadDigit;
  return inputs;
}

static UiModel buildUiModel(const GameOutputs &outputs) {
  UiModel model{};
  model.theme = themeConfig;

  model.boot.show = getState() == ON && !network::isConfigPortalActive() && !network::hasReceivedApiResponse();
  model.boot.wifiSsid = network::getConfiguredWifiSsid();
  model.boot.wifiConnected = network::isWifiConnected();
  model.boot.wifiFailed = network::isConfigPortalActive() || network::hasWifiFailedPermanently();
  model.boot.configApSsid = network::getConfigPortalSsid();
  model.boot.configApAddress = network::getConfigPortalAddress();
  model.boot.ipAddress = network::getWifiIpString();
  model.boot.apiEndpoint = network::getConfiguredApiEndpoint();
  model.boot.hasApiResponse = network::hasReceivedApiResponse();

  model.configPortal.show = network::isConfigPortalActive();
  model.configPortal.ssid = network::getConfigPortalSsid();
  model.configPortal.password = network::getConfigPortalPassword();
  model.configPortal.portalAddress = network::getConfigPortalAddress();

  model.timers.bombDurationMs = configuredBombDurationMs;
  model.timers.bombRemainingMs = outputs.bombTimerActive ? outputs.bombTimerRemainingMs : configuredBombDurationMs;
  model.timers.bombTimerActive = outputs.bombTimerActive;
  model.timers.gameTimerValid = outputs.gameTimerValid;
  model.timers.gameTimerRemainingMs = outputs.gameTimerRemainingMs;

  model.arming.progress01 = outputs.armingProgress01;
  model.arming.awaitingIrConfirm = outputs.awaitingIrConfirmation;

  model.game.state = outputs.state;
  model.game.codeLength = DEFUSE_CODE_LENGTH;
  model.game.enteredDigits = getEnteredDigits();
  model.game.defuseBuffer = String(getDefuseBuffer());
  model.game.gameOverActive = outputs.gameOverActive;
  model.game.matchStatus = getMatchStatus();
  model.game.ipAddress = network::getWifiIpString();

  return model;
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
  }
}
#endif

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
  ui::resetUiState();
  network::beginWifi();
  configuredBombDurationMs = network::getConfiguredBombDurationMs();
}

void loop() {
  const uint32_t now = millis();

  if (now - lastInputsUpdateMs >= 5) {
    lastInputsUpdateMs = now;
    const InputSnapshot snapshot = readInputSnapshot();
    applyGameInputs(mapSnapshotToInputs(snapshot));
  }

  if (now - lastWifiUpdateMs >= 200) {
    lastWifiUpdateMs = now;
    network::updateWifi();
  }

  if (now - lastStateUpdateMs >= 10) {
    lastStateUpdateMs = now;
    updateState();
  }

  if (now - lastEffectsUpdateMs >= 42) {
    lastEffectsUpdateMs = now;
    effects::update(now);
  }

  if (now - lastUiUpdateMs >= 42) {
    lastUiUpdateMs = now;

    // Keep the cached bomb duration aligned with any persisted updates.
    configuredBombDurationMs = network::getConfiguredBombDurationMs();

    GameOutputs outputs = getGameOutputs();
    effects::setArmingProgress(outputs.armingProgress01);
    UiModel model = buildUiModel(outputs);
    ui::renderUi(model);

#ifdef APP_DEBUG
    handleDebugSerialStateChange();
#endif
  }

  // Throttle web UI servicing to reduce load during ACTIVE/ARMING while keeping config responsive.
  if (now - lastWebUiUpdateMs >= 200) {
    lastWebUiUpdateMs = now;
    network::updateConfigPortal(now, getState());
  }

  if (network::isWifiConnected() && now - lastApiUpdateMs >= API_POST_INTERVAL_MS) {
    lastApiUpdateMs = now;
    network::updateApi();
  }

  // Drive WiFi-dependent state changes without blocking the loop.
  if (getState() == ON) {
    if (network::hasReceivedApiResponse()) {
      setState(READY);
    } else if (network::hasWifiFailedPermanently()) {
      setState(ERROR_STATE);
    }
  }
}
