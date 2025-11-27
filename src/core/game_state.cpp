#include "core/game_state.h"

#include "game_config.h"
#include "inputs.h"
#include "network.h"

void game_init() { setState(ON); }

void game_tick(const GameInputs &in, GameOutputs &out, uint32_t nowMs) {
  (void)nowMs;

  if (in.apiTimedOut && getState() != ERROR_STATE) {
    setState(ERROR_STATE);
  }

  if (in.bothButtonsHeld) {
    startButtonHold();
  } else if (!in.bothButtonsHeld) {
    stopButtonHold();
  }

  if (getState() == ARMING && in.irConfirmationReceived) {
    setState(ARMED);
    clearIrConfirmation();
  }

  updateState();

  out.state = getState();
  out.matchStatus = getMatchStatus();
  out.gameRemainingMs = isGameTimerValid() ? getGameTimerRemainingMs() : 0;
  out.bombRemainingMs = isBombTimerActive() ? getBombTimerRemainingMs() : 0;
  out.showGameOverOverlay = in.gameOverRemoteStatus;
  out.irWindowActive = isIrConfirmationWindowActive();
  out.armingHoldActive = isButtonHoldActive();
}
