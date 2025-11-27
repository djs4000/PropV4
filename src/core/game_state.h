#pragma once

#include <Arduino.h>

#include "state_machine.h"

struct GameInputs {
  bool bothButtonsHeld = false;
  bool irConfirmationReceived = false;
  bool wifiConnected = false;
  bool apiTimedOut = false;
  bool gameOverRemoteStatus = false;
};

struct GameOutputs {
  FlameState state = ON;
  MatchStatus matchStatus = WaitingOnStart;
  uint32_t gameRemainingMs = 0;
  uint32_t bombRemainingMs = 0;
  bool showGameOverOverlay = false;
  bool irWindowActive = false;
  bool armingHoldActive = false;
};

void game_init();
void game_tick(const GameInputs &in, GameOutputs &out, uint32_t nowMs);
