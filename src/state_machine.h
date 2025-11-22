#pragma once

#include <Arduino.h>

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

// Accessors and update routine
FlameState getState();
void setState(FlameState newState);
void updateState();

// Match status helpers (populated by the networking layer)
void setMatchStatus(MatchStatus status);
MatchStatus getMatchStatus();

// Timer helpers for ARMED countdown (placeholder for later implementation)
void setArmedTimerMs(uint32_t remainingMs);
uint32_t getArmedTimerMs();

// Button hold helpers (driven by inputs module)
void startButtonHold();
void stopButtonHold();
bool isButtonHoldActive();
uint32_t getButtonHoldStartMs();

// Utility conversion helpers
const char *flameStateToString(FlameState state);
const char *matchStatusToString(MatchStatus status);
