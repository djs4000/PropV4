#pragma once

#include <Arduino.h>

#include "game_config.h"

// Flame state definitions follow agents.md strictly.
enum FlameState {
  ON,
  READY,
  ACTIVE,
  ARMING,
  ARMED,
  DEFUSED,
  DETONATED,
  ERROR_STATE
};

// Match status values returned by the backend API.
enum MatchStatus {
  WaitingOnStart,
  Countdown,
  Running,
  WaitingOnFinalData,
  Completed,
  Cancelled
};

namespace state_machine {

void init();
void updateState();

FlameState getState();
void setState(FlameState newState);

void setMatchStatus(MatchStatus status);
MatchStatus getMatchStatus();

// Track IR confirmation window for UI cues
bool isIrConfirmationPending();

// Provide armed timer placeholder (kept for compatibility)
void setArmedTimerMs(uint32_t remainingMs);
uint32_t getArmedTimerMs();

const char *flameStateToString(FlameState state);
const char *matchStatusToString(MatchStatus status);

}  // namespace state_machine
