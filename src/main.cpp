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

static UiThemeConfig themeConfig = ui::defaultTheme();
static InputSnapshot lastInputSnapshot{};
static GameOutputs lastGameOutputs{};
static uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;

static UiModel buildUiModel() {
  UiModel model{};
  model.theme = themeConfig;

  const FlameState state = getState();
  model.state = state;
  model.bombDurationMs = configuredBombDurationMs;
  model.timerRemainingMs = configuredBombDurationMs;
  model.armingProgress01 = getArmingProgress(lastInputSnapshot.nowMs);
  model.codeLength = DEFUSE_CODE_LENGTH;
  model.enteredDigits = getDefuseEnteredDigits();
  model.defuseBuffer = (state == ARMED) ? String(getDefuseBuffer()) : String("");
  model.showArmingPrompt = lastGameOutputs.showArmingConfirmPrompt || (state == ARMING && isIrConfirmationWindowActive());
  model.gameOver = lastGameOutputs.gameOver;

  if (state == ARMED) {
    model.bombTimerActive = isBombTimerActive();
    model.timerRemainingMs = getBombTimerRemainingMs();
    model.bombTimerExpired = model.timerRemainingMs == 0;
  } else if (isGameTimerValid()) {
    model.timerRemainingMs = getGameTimerRemainingMs();
  }

  model.showBootScreen = (state == ON) && !network::isConfigPortalActive();
  model.wifiSsid = network::getConfiguredWifiSsid();
  model.wifiConnected = network::isWifiConnected();
  model.wifiFailed = network::isConfigPortalActive() || network::hasWifiFailedPermanently();
  model.configApSsid = network::getConfigPortalSsid();
  model.configApAddress = network::getConfigPortalAddress();
  model.configApPassword = network::getConfigPortalPassword();
  model.ipAddress = network::getWifiIpString();
  model.apiEndpoint = network::getConfiguredApiEndpoint();
  model.hasApiResponse = network::hasReceivedApiResponse();
  model.showConfigPortal = network::isConfigPortalActive();

#ifdef APP_DEBUG
  model.debugIp = network::getWifiIpString();
  model.debugMatchStatus = String("Match ") + matchStatusToString(network::getRemoteMatchStatus());
  model.debugTimerValid = isGameTimerValid();
  model.debugTimerRemainingMs = getGameTimerRemainingMs();
#endif

  return model;
}

#ifdef APP_DEBUG
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

static void handleStateTask(uint32_t) {
  lastGameOutputs = GameOutputs{};
  updateState(lastInputSnapshot, lastGameOutputs);

  if (getState() == ON) {
    if (network::hasReceivedApiResponse()) {
      setState(READY);
    } else if (network::hasWifiFailedPermanently()) {
      setState(ERROR_STATE);
    }
  }
}

static void handleEffectsTask(uint32_t now) {
  effects::setArmingProgress(getArmingProgress(now));
  effects::update(now);
}

static void handleUiTask(uint32_t) {
  configuredBombDurationMs = network::getConfiguredBombDurationMs();
  UiModel model = buildUiModel();
  ui::render(model);

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
    break;
  }
#endif

  setState(ON);

  effects::init();
  effects::onBoot();

  initInputs();
  ui::initUI();
  network::beginWifi();
  configuredBombDurationMs = network::getConfiguredBombDurationMs();

  scheduler::addTask([](uint32_t) { lastInputSnapshot = updateInputs(); }, 5);
  scheduler::addTask([](uint32_t) { network::updateWifi(); }, 200);
  scheduler::addTask(handleStateTask, 10);
  scheduler::addTask(handleEffectsTask, 42);
  scheduler::addTask(handleUiTask, 42);
  scheduler::addTask(handleConfigPortalTask, 200);
  scheduler::addTask(handleApiTask, API_POST_INTERVAL_MS);
}

void loop() { scheduler::run(); }
