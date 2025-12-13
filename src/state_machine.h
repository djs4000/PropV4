#pragma once

#include <Arduino.h>

#include "core/game_state.h"
#include "inputs.h"

// Accessors and update routine
FlameState getState();
void setState(FlameState newState);
void updateState(const InputSnapshot &inputSnapshot, GameOutputs &outputs);

// Match status helpers (populated by the networking layer)
void setMatchStatus(MatchStatus status);
MatchStatus getMatchStatus();

// Game timer synchronization (authoritative data from API with local backup countdown).
void updateGameTimerFromApi(uint32_t remainingMs, uint32_t nowMs, uint32_t rttMs = 0,
                            MatchStatus status = WaitingOnStart);
bool isGameTimerValid();
uint32_t getGameTimerRemainingMs();

// Bomb timer helpers for ARMED countdown.
bool isBombTimerActive();
uint32_t getBombTimerRemainingMs();
uint32_t getBombTimerDurationMs();

// Button hold helpers (state managed internally)
bool isButtonHoldActive();
uint32_t getButtonHoldStartMs();
bool isIrConfirmationWindowActive();

uint8_t getDefuseEnteredDigits();
const char *getDefuseBuffer();
float getArmingProgress(uint32_t nowMs);

// Utility conversion helpers
const char *flameStateToString(FlameState state);
const char *matchStatusToString(MatchStatus status);
