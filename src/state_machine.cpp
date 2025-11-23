#include "state_machine.h"

#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "ui.h"
#include "util.h"

namespace {
FlameState currentState = ON;
MatchStatus currentMatchStatus = WaitingOnStart;
uint32_t armedTimerMs = DEFAULT_BOMB_DURATION_MS;

// Tracks how long both buttons have been held during ARMING/ERROR recovery.
uint32_t armingHoldStartMs = 0;
bool armingHoldActive = false;

bool isGlobalTimeoutTriggered() {
  const uint64_t now = millis();
  const uint64_t lastSuccess = network::getLastSuccessfulApiMs();
  return network::isWifiConnected() && (now - lastSuccess >= API_TIMEOUT_MS);
}

bool armingHoldComplete = false;
uint32_t irWindowStartMs = 0;
bool irWindowActive = false;

void resetArmingFlow() {
  armingHoldComplete = false;
  irWindowActive = false;
  irWindowStartMs = 0;
}
}

FlameState getState() { return currentState; }

void setState(FlameState newState) {
  if (newState == currentState) {
    return;
  }
#ifdef APP_DEBUG
  Serial.print("STATE: ");
  Serial.print(flameStateToString(currentState));
  Serial.print(" -> ");
  Serial.println(flameStateToString(newState));
#endif
  currentState = newState;
  // Future: trigger effects/UI hooks here.
}

void updateState() {
  // Apply global networking timeout rule first for all non-error states.
  if (currentState != ERROR_STATE && isGlobalTimeoutTriggered()) {
#ifdef APP_DEBUG
    Serial.println("Global API timeout triggered; entering ERROR_STATE");
#endif
    setState(ERROR_STATE);
    return;
  }

  // Sync the latest match status from the network module only after a
  // successful API response. This prevents manual/debug overrides from being
  // immediately reset when the API is disabled or hasn't provided data yet.
  if (network::hasReceivedApiResponse()) {
    setMatchStatus(network::getRemoteMatchStatus());
  }

  // Placeholder button handling: actual input module will call into the state
  // machine to start/stop arming. For now we maintain stub timing logic to
  // illustrate the non-blocking pattern without implementing hardware reads.
  const uint32_t now = millis();

  switch (currentState) {
    case ON:
      break;

    case READY:
      if (currentMatchStatus == Running) {
        setState(ACTIVE);
      }
      break;

    case ACTIVE:
      // Both buttons pressed immediately transitions to ARMING; hold timer is
      // tracked for the subsequent ARMING->ARMED promotion.
      if (armingHoldActive) {
        setState(ARMING);
      }
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown ||
          currentMatchStatus == WaitingOnFinalData) {
        setState(READY);
      }
      break;

    case ARMING:
      // Hold timer continues; if released early, revert to ACTIVE.
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown ||
          currentMatchStatus == WaitingOnFinalData) {
        resetArmingFlow();
        stopButtonHold();
        setState(READY);
        break;
      }

      if (!armingHoldActive) {
        resetArmingFlow();
        setState(ACTIVE);
        break;
      }

      if (!armingHoldComplete && (now - armingHoldStartMs >= BUTTON_HOLD_MS)) {
        armingHoldComplete = true;
        irWindowActive = true;
        irWindowStartMs = now;
        ui::showArmingConfirmPrompt();
      }

      if (irWindowActive) {
        if (consumeIrConfirmation()) {
          effects::startConfirmationBeep();
          setState(ARMED);
          resetArmingFlow();
          stopButtonHold();
          break;
        }

        if (now - irWindowStartMs >= IR_CONFIRM_WINDOW_MS) {
          resetArmingFlow();
          stopButtonHold();
          setState(ACTIVE);
        }
      }
      break;

    case ARMED:
      // Countdown placeholder. Actual timing will be integrated with inputs/effects.
      if (armedTimerMs == 0) {
        setState(DETONATED);
      }
      if (currentMatchStatus == Completed || currentMatchStatus == Cancelled) {
        setState(READY);
      }
      break;

    case DEFUSED:
      if (currentMatchStatus == WaitingOnStart) {
        setState(READY);
      }
      break;

    case DETONATED:
      if (currentMatchStatus == WaitingOnStart) {
        setState(READY);
      }
      break;

    case ERROR_STATE:
      // Placeholder: reset to ON after BUTTON_HOLD_MS of both buttons pressed.
      if (armingHoldActive && (now - armingHoldStartMs >= BUTTON_HOLD_MS)) {
        stopButtonHold();
        setState(ON);
      }
      break;
  }
}

void startButtonHold() {
  if (armingHoldActive) {
    return;
  }
  armingHoldActive = true;
  armingHoldStartMs = millis();
}

void stopButtonHold() {
  armingHoldActive = false;
  armingHoldStartMs = 0;
  resetArmingFlow();
}

bool isButtonHoldActive() { return armingHoldActive; }

uint32_t getButtonHoldStartMs() { return armingHoldStartMs; }

void setMatchStatus(MatchStatus status) { currentMatchStatus = status; }

MatchStatus getMatchStatus() { return currentMatchStatus; }

void setArmedTimerMs(uint32_t remainingMs) { armedTimerMs = remainingMs; }

uint32_t getArmedTimerMs() { return armedTimerMs; }

const char *flameStateToString(FlameState state) {
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
      return "Unknown"; // Should not happen; kept for safety.
  }
}

const char *matchStatusToString(MatchStatus status) {
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
