#pragma once

#include <Arduino.h>

#include "core/game_state.h"

// Accessors and update routine
FlameState getState();
void setState(FlameState newState);
void updateState();

// Match status helpers (populated by the networking layer)
void setMatchStatus(MatchStatus status);
MatchStatus getMatchStatus();

// Game timer synchronization (authoritative data from API with local backup countdown).
void updateGameTimerFromApi(uint32_t remainingMs, uint32_t nowMs);
bool isGameTimerValid();
uint32_t getGameTimerRemainingMs();

// Bomb timer helpers for ARMED countdown.
bool isBombTimerActive();
uint32_t getBombTimerRemainingMs();
uint32_t getBombTimerDurationMs();

// Button hold helpers (driven by inputs module)
void startButtonHold(uint32_t nowMs);
void stopButtonHold();
bool isButtonHoldActive();
uint32_t getButtonHoldStartMs();
bool isIrConfirmationWindowActive();

// Utility conversion helpers
const char *flameStateToString(FlameState state);
const char *matchStatusToString(MatchStatus status);
