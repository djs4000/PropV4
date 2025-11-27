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

struct GameInputs {
  uint32_t nowMs = 0;
  bool wifiConnected = false;
  uint64_t lastSuccessfulApiMs = 0;
  bool apiResponseReceived = false;
  MatchStatus remoteMatchStatus = WaitingOnStart;
  uint32_t configuredBombDurationMs = DEFAULT_BOMB_DURATION_MS;
  String configuredDefuseCode;
  bool bothButtonsPressed = false;
  bool keypadDigitAvailable = false;
  char keypadDigit = '\0';
  bool irConfirmationReceived = false;
};

struct GameOutputs {
  bool stateChanged = false;
  FlameState previousState = ON;
  FlameState newState = ON;

  bool showArmingConfirmPrompt = false;
  bool armingConfirmNeededEffect = false;
  bool armingConfirmedEffect = false;
  bool wrongCodeEffect = false;
  bool keypadDigitEffect = false;

  bool clearIrConfirmation = false;

  bool gameOverSet = false;
  bool gameOver = false;
};

namespace game_state {

void game_init();
void game_tick(const GameInputs &inputs, GameOutputs &outputs);

FlameState get_state();
void set_state(FlameState newState, GameOutputs *outputs = nullptr);

void set_match_status(MatchStatus status);
MatchStatus get_match_status();

void update_game_timer_from_api(uint32_t remainingMs, uint32_t nowMs);
bool is_game_timer_valid();
uint32_t get_game_timer_remaining_ms();

bool is_bomb_timer_active();
uint32_t get_bomb_timer_remaining_ms();
uint32_t get_bomb_timer_duration_ms();

bool is_button_hold_active();
uint32_t get_button_hold_start_ms();
bool is_ir_confirmation_window_active();

uint8_t get_defuse_entered_digits();
const char *get_defuse_buffer();

float get_arming_progress(uint32_t nowMs);

const char *flame_state_to_string(FlameState state);
const char *match_status_to_string(MatchStatus status);

}  // namespace game_state

