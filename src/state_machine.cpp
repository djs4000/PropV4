#include "state_machine.h"

#include "core/game_state.h"
#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "util.h"

namespace {
GameInputs buildGameInputs(const InputSnapshot &inputSnapshot) {
  GameInputs inputs{};
  inputs.nowMs = inputSnapshot.nowMs;
  inputs.wifiConnected = network::isWifiConnected();
  inputs.lastSuccessfulApiMs = network::getLastSuccessfulApiMs();
  inputs.apiResponseReceived = network::hasReceivedApiResponse();
  inputs.remoteMatchStatus = network::getRemoteMatchStatus();
  inputs.configuredBombDurationMs = network::getConfiguredBombDurationMs();
  inputs.configuredDefuseCode = network::getConfiguredDefuseCode();
  inputs.bothButtonsPressed = inputSnapshot.bothButtonsPressed;
  inputs.keypadDigitAvailable = inputSnapshot.keypadDigitAvailable;
  inputs.keypadDigit = inputSnapshot.keypadDigit;
  inputs.irConfirmationReceived = inputSnapshot.irConfirmationReceived;
  return inputs;
}

void applyOutputs(const GameOutputs &outputs) {
  if (outputs.clearIrConfirmation) {
    clearIrConfirmation();
  }

  if (outputs.armingConfirmNeededEffect) {
    effects::onArmingConfirmNeeded();
  }

  if (outputs.armingConfirmedEffect) {
    effects::onArmingConfirmed();
  }

  if (outputs.keypadDigitEffect) {
    effects::onKeypadKey();
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
  GameOutputs outputs{};
  game_state::set_state(newState, &outputs);
  applyOutputs(outputs);
}

void updateState(const InputSnapshot &inputSnapshot, GameOutputs &outputs) {
  GameInputs inputs = buildGameInputs(inputSnapshot);
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

bool isButtonHoldActive() { return game_state::is_button_hold_active(); }

uint32_t getButtonHoldStartMs() { return game_state::get_button_hold_start_ms(); }

bool isIrConfirmationWindowActive() { return game_state::is_ir_confirmation_window_active(); }

uint8_t getDefuseEnteredDigits() { return game_state::get_defuse_entered_digits(); }

const char *getDefuseBuffer() { return game_state::get_defuse_buffer(); }

float getArmingProgress(uint32_t nowMs) { return game_state::get_arming_progress(nowMs); }

const char *flameStateToString(FlameState state) { return game_state::flame_state_to_string(state); }

const char *matchStatusToString(MatchStatus status) { return game_state::match_status_to_string(status); }
