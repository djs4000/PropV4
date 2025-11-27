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

struct GameInputs {
  bool bothButtonsPressed = false;
  bool irConfirmationReceived = false;
  bool hasKeypadDigit = false;
  char keypadDigit = '\0';
};

struct GameOutputs {
  FlameState state;
  bool gameOverActive = false;
  bool bombTimerActive = false;
  uint32_t bombTimerDurationMs = 0;
  uint32_t bombTimerRemainingMs = 0;
  bool gameTimerValid = false;
  uint32_t gameTimerRemainingMs = 0;
  float armingProgress01 = 0.0f;
  bool awaitingIrConfirmation = false;
};

// Accessors and update routine
FlameState getState();
void setState(FlameState newState);
void updateState();
void applyGameInputs(const GameInputs &inputs);

// Match status helpers (populated by the networking layer)
void setMatchStatus(MatchStatus status);
MatchStatus getMatchStatus();

GameOutputs getGameOutputs();
uint8_t getEnteredDigits();
const char *getDefuseBuffer();

// Game timer synchronization (authoritative data from API with local backup countdown).
void updateGameTimerFromApi(uint32_t remainingMs);
bool isGameTimerValid();
uint32_t getGameTimerRemainingMs();

// Bomb timer helpers for ARMED countdown.
bool isBombTimerActive();
uint32_t getBombTimerRemainingMs();
uint32_t getBombTimerDurationMs();

// Utility conversion helpers
const char *flameStateToString(FlameState state);
const char *matchStatusToString(MatchStatus status);
