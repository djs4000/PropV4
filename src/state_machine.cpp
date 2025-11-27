#include "state_machine.h"

#include "config/config_store.h"
#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "ui.h"
#include "util.h"

namespace {
GameInputs buildGameInputs() {
  GameInputs inputs{};
  inputs.nowMs = millis();
  inputs.wifiConnected = network::isWifiConnected();
  inputs.lastSuccessfulApiMs = network::getLastSuccessfulApiMs();
  inputs.apiResponseReceived = network::hasReceivedApiResponse();
  inputs.remoteMatchStatus = network::getRemoteMatchStatus();
  inputs.configuredBombDurationMs = network::getConfiguredBombDurationMs();
  inputs.irConfirmationReceived = consumeIrConfirmation();
  return inputs;
}

void applyOutputs(const GameOutputs &outputs) {
  if (outputs.gameOverSet) {
    ui::setGameOver(outputs.gameOver);
  }

  if (outputs.clearIrConfirmation) {
    clearIrConfirmation();
  }

  if (outputs.clearDefuseBuffer) {
    clearDefuseBuffer();
  }

  if (outputs.showArmingConfirmPrompt) {
    ui::showArmingConfirmPrompt();
  }

  if (outputs.armingConfirmNeededEffect) {
    effects::onArmingConfirmNeeded();
  }

  if (outputs.armingConfirmedEffect) {
    effects::onArmingConfirmed();
  }

  if (outputs.wrongCodeEffect) {
    effects::onWrongCode();
  }

  if (outputs.stateChanged) {
#ifdef APP_DEBUG
    Serial.print("STATE: ");
    Serial.print(flameStateToString(outputs.previousState));
    Serial.print(" -> ");
    Serial.println(flameStateToString(outputs.newState));
#endif
    effects::onStateChanged(outputs.previousState, outputs.newState);
  }
}
}  // namespace

FlameState getState() { return game_state::get_state(); }

void setState(FlameState newState) {
  if (newState == currentState) {
    return;
  }
  const FlameState oldState = currentState;
#ifdef APP_DEBUG
  Serial.print("STATE: ");
  Serial.print(flameStateToString(currentState));
  Serial.print(" -> ");
  Serial.println(flameStateToString(newState));
#endif
  currentState = newState;

  if (oldState == ARMING && newState != ARMING) {
    resetArmingFlow();
    stopButtonHold();
  }
  // Timer lifecycle hooks based on state transitions.
  if (newState == ARMED && oldState != ARMED) {
    bombTimerActive = true;
    bombTimerDurationMs = config_store::getBombDurationMs();
    if (bombTimerDurationMs == 0) {
      bombTimerDurationMs = DEFAULT_BOMB_DURATION_MS;
    }
    bombTimerRemainingMs = bombTimerDurationMs;
    bombTimerLastUpdateMs = millis();
  } else if (oldState == ARMED && newState != ARMED) {
    bombTimerActive = false;
    if (newState != DEFUSED) {
      bombTimerRemainingMs = 0;
    }
  }

  effects::onStateChanged(oldState, newState);
}

void updateState() {
  GameInputs inputs = buildGameInputs();
  GameOutputs outputs{};
  game_state::game_tick(inputs, outputs);
  applyOutputs(outputs);
}

void setMatchStatus(MatchStatus status) { game_state::set_match_status(status); }

MatchStatus getMatchStatus() { return game_state::get_match_status(); }

void updateGameTimerFromApi(uint32_t remainingMs, uint32_t nowMs) {
  game_state::update_game_timer_from_api(remainingMs, nowMs);
}

bool isGameTimerValid() { return game_state::is_game_timer_valid(); }

uint32_t getGameTimerRemainingMs() { return game_state::get_game_timer_remaining_ms(); }

bool isBombTimerActive() { return game_state::is_bomb_timer_active(); }

uint32_t getBombTimerRemainingMs() { return game_state::get_bomb_timer_remaining_ms(); }

uint32_t getBombTimerDurationMs() { return game_state::get_bomb_timer_duration_ms(); }

void startButtonHold(uint32_t nowMs) { game_state::start_button_hold(nowMs); }

void stopButtonHold() { game_state::stop_button_hold(); }

bool isButtonHoldActive() { return game_state::is_button_hold_active(); }

uint32_t getButtonHoldStartMs() { return game_state::get_button_hold_start_ms(); }

bool isIrConfirmationWindowActive() { return game_state::is_ir_confirmation_window_active(); }

const char *flameStateToString(FlameState state) { return game_state::flame_state_to_string(state); }

const char *matchStatusToString(MatchStatus status) { return game_state::match_status_to_string(status); }
