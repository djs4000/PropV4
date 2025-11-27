#include "core/game_state.h"

namespace game_state {
namespace {
FlameState currentState = ON;
MatchStatus currentMatchStatus = WaitingOnStart;

bool gameTimerValid = false;
uint32_t gameTimerRemainingMs = 0;
uint32_t gameTimerLastUpdateMs = 0;

bool bombTimerActive = false;
uint32_t bombTimerDurationMs = 0;
uint32_t bombTimerRemainingMs = 0;
uint32_t bombTimerLastUpdateMs = 0;

uint32_t armingHoldStartMs = 0;
bool armingHoldActive = false;
bool armingHoldComplete = false;
uint32_t irWindowStartMs = 0;
bool irWindowActive = false;
bool pendingClearIrConfirmation = false;

uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;

bool isGameOverStatus(MatchStatus status) {
  return status == WaitingOnFinalData || status == Completed || status == Cancelled;
}

bool isGameTimerCountdownAllowed() {
  return currentState == ACTIVE || currentState == ARMING || currentState == ARMED;
}

bool isGlobalTimeoutTriggered(const GameInputs &inputs) {
  return inputs.wifiConnected && (inputs.nowMs - inputs.lastSuccessfulApiMs >= API_TIMEOUT_MS);
}

void resetArmingFlow(GameOutputs &outputs) {
  armingHoldComplete = false;
  irWindowActive = false;
  irWindowStartMs = 0;
  outputs.clearIrConfirmation = true;
}

void stopButtonHoldInternal(GameOutputs &outputs) {
  armingHoldActive = false;
  armingHoldStartMs = 0;
  resetArmingFlow(outputs);
}

void updateGameTimerCountdown(uint32_t nowMs) {
  if (!gameTimerValid) {
    return;
  }

  if (!isGameTimerCountdownAllowed()) {
    gameTimerLastUpdateMs = nowMs;
    return;
  }

  const uint32_t delta = nowMs - gameTimerLastUpdateMs;
  gameTimerLastUpdateMs = nowMs;

  if (delta == 0 || gameTimerRemainingMs == 0) {
    return;
  }

  if (delta >= gameTimerRemainingMs) {
    gameTimerRemainingMs = 0;
  } else {
    gameTimerRemainingMs -= delta;
  }
}

void transitionTo(FlameState newState, GameOutputs &outputs, uint32_t nowMs) {
  if (newState == currentState) {
    return;
  }

  const FlameState oldState = currentState;
  currentState = newState;

  outputs.stateChanged = true;
  outputs.previousState = oldState;
  outputs.newState = newState;

  if (oldState == ARMING && newState != ARMING) {
    stopButtonHoldInternal(outputs);
  }

  if (newState == ARMED && oldState != ARMED) {
    bombTimerActive = true;
    bombTimerDurationMs = configuredBombDurationMs == 0 ? DEFAULT_BOMB_DURATION_MS : configuredBombDurationMs;
    bombTimerRemainingMs = bombTimerDurationMs;
    bombTimerLastUpdateMs = nowMs;
  } else if (oldState == ARMED && newState != ARMED) {
    bombTimerActive = false;
    if (newState != DEFUSED) {
      bombTimerRemainingMs = 0;
    }
  }
}

void updateBombTimerCountdown(uint32_t nowMs, GameOutputs &outputs) {
  if (!bombTimerActive) {
    return;
  }

  if (currentState != ARMED) {
    bombTimerActive = false;
    return;
  }

  const uint32_t delta = nowMs - bombTimerLastUpdateMs;
  bombTimerLastUpdateMs = nowMs;

  if (delta == 0 || bombTimerRemainingMs == 0) {
    return;
  }

  if (delta >= bombTimerRemainingMs) {
    bombTimerRemainingMs = 0;
  } else {
    bombTimerRemainingMs -= delta;
  }

  if (bombTimerRemainingMs == 0 && currentState == ARMED) {
    bombTimerActive = false;
    transitionTo(DETONATED, outputs, nowMs);
  }
}

void updateTimers(uint32_t nowMs, GameOutputs &outputs) {
  updateGameTimerCountdown(nowMs);
  updateBombTimerCountdown(nowMs, outputs);
}

void handleArmingFlow(uint32_t nowMs, bool irConfirmationReceived, GameOutputs &outputs) {
  if (!armingHoldActive) {
    resetArmingFlow(outputs);
    return;
  }

  if (!armingHoldComplete && (nowMs - armingHoldStartMs >= BUTTON_HOLD_MS)) {
    armingHoldComplete = true;
    irWindowActive = true;
    irWindowStartMs = nowMs;
    outputs.showArmingConfirmPrompt = true;
    outputs.armingConfirmNeededEffect = true;
  }

  if (!irWindowActive) {
    return;
  }

  if (irConfirmationReceived) {
    outputs.armingConfirmedEffect = true;
    transitionTo(ARMED, outputs, nowMs);
    resetArmingFlow(outputs);
    return;
  }

  if (nowMs - irWindowStartMs >= IR_CONFIRM_WINDOW_MS) {
    outputs.wrongCodeEffect = true;
    resetArmingFlow(outputs);
    transitionTo(ACTIVE, outputs, nowMs);
  }
}
}  // namespace

void game_init() { currentState = ON; }

void game_tick(const GameInputs &inputs, GameOutputs &outputs) {
  configuredBombDurationMs = inputs.configuredBombDurationMs;

  if (pendingClearIrConfirmation) {
    outputs.clearIrConfirmation = true;
    pendingClearIrConfirmation = false;
  }

  if (currentState != ERROR_STATE && isGlobalTimeoutTriggered(inputs)) {
    transitionTo(ERROR_STATE, outputs, inputs.nowMs);
    return;
  }

  if (inputs.apiResponseReceived) {
    set_match_status(inputs.remoteMatchStatus);
  }

  const bool gameOver = isGameOverStatus(currentMatchStatus) && currentState != DEFUSED && currentState != DETONATED;
  outputs.gameOverSet = true;
  outputs.gameOver = gameOver;
  if (gameOver) {
    gameTimerValid = false;
    gameTimerRemainingMs = 0;
    gameTimerLastUpdateMs = inputs.nowMs;
    bombTimerActive = false;
    bombTimerRemainingMs = 0;
    resetArmingFlow(outputs);
    armingHoldActive = false;
    armingHoldStartMs = 0;
    if (currentState == ARMED || currentState == ARMING || currentState == ACTIVE) {
      transitionTo(READY, outputs, inputs.nowMs);
      return;
    }
  }

  if (currentMatchStatus == WaitingOnStart) {
    resetArmingFlow(outputs);
    outputs.clearDefuseBuffer = true;
  }

  updateTimers(inputs.nowMs, outputs);
  if (outputs.stateChanged) {
    return;
  }

  switch (currentState) {
    case ON:
      break;

    case READY:
      if (currentMatchStatus == Running) {
        transitionTo(ACTIVE, outputs, inputs.nowMs);
      }
      break;

    case ACTIVE:
      if (armingHoldActive) {
        transitionTo(ARMING, outputs, inputs.nowMs);
      }
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown || currentMatchStatus == WaitingOnFinalData) {
        transitionTo(READY, outputs, inputs.nowMs);
      }
      break;

    case ARMING:
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown || currentMatchStatus == WaitingOnFinalData) {
        resetArmingFlow(outputs);
        armingHoldActive = false;
        armingHoldStartMs = 0;
        transitionTo(READY, outputs, inputs.nowMs);
        break;
      }

      if (!armingHoldActive) {
        resetArmingFlow(outputs);
        transitionTo(ACTIVE, outputs, inputs.nowMs);
        break;
      }

      handleArmingFlow(inputs.nowMs, inputs.irConfirmationReceived, outputs);
      break;

    case ARMED:
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown) {
        transitionTo(READY, outputs, inputs.nowMs);
      }
      break;

    case DEFUSED:
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown) {
        transitionTo(READY, outputs, inputs.nowMs);
      }
      break;

    case DETONATED:
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown) {
        transitionTo(READY, outputs, inputs.nowMs);
      }
      break;

    case ERROR_STATE:
      if (armingHoldActive && (inputs.nowMs - armingHoldStartMs >= BUTTON_HOLD_MS)) {
        armingHoldActive = false;
        armingHoldStartMs = 0;
        transitionTo(ON, outputs, inputs.nowMs);
      }
      break;
  }
}

FlameState get_state() { return currentState; }

void set_state(FlameState newState, GameOutputs *outputs) {
  if (outputs) {
    transitionTo(newState, *outputs, millis());
  } else {
    GameOutputs localOutputs;
    transitionTo(newState, localOutputs, millis());
  }
}

void set_match_status(MatchStatus status) { currentMatchStatus = status; }

MatchStatus get_match_status() { return currentMatchStatus; }

void update_game_timer_from_api(uint32_t remainingMs, uint32_t nowMs) {
  gameTimerValid = true;
  gameTimerRemainingMs = remainingMs;
  gameTimerLastUpdateMs = nowMs;
}

bool is_game_timer_valid() { return gameTimerValid; }

uint32_t get_game_timer_remaining_ms() { return gameTimerRemainingMs; }

bool is_bomb_timer_active() { return bombTimerActive; }

uint32_t get_bomb_timer_remaining_ms() { return bombTimerRemainingMs; }

uint32_t get_bomb_timer_duration_ms() { return bombTimerDurationMs; }

void start_button_hold(uint32_t nowMs) {
  if (armingHoldActive) {
    return;
  }
  armingHoldActive = true;
  armingHoldStartMs = nowMs;
}

void stop_button_hold() {
  armingHoldActive = false;
  armingHoldStartMs = 0;
  armingHoldComplete = false;
  irWindowActive = false;
  irWindowStartMs = 0;
  pendingClearIrConfirmation = true;
}

bool is_button_hold_active() { return armingHoldActive; }

uint32_t get_button_hold_start_ms() { return armingHoldStartMs; }

bool is_ir_confirmation_window_active() { return irWindowActive; }

const char *flame_state_to_string(FlameState state) {
  switch (state) {
    case ON:
      return "On";
    case READY:
      return "Ready";
    case ACTIVE:
      return "Active";
    case ARMING:
      return "Arming";
    case ARMED:
      return "Armed";
    case DEFUSED:
      return "Defused";
    case DETONATED:
      return "Detonated";
    case ERROR_STATE:
      return "Error";
    default:
      return "Unknown";
  }
}

const char *match_status_to_string(MatchStatus status) {
  switch (status) {
    case WaitingOnStart:
      return "WaitingOnStart";
    case Countdown:
      return "Countdown";
    case Running:
      return "Running";
    case WaitingOnFinalData:
      return "WaitingOnFinalData";
    case Completed:
      return "Completed";
    case Cancelled:
      return "Cancelled";
    default:
      return "Unknown";
  }
}

}  // namespace game_state

