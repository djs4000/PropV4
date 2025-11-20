#include "state_machine.h"

#include "game_config.h"
#include "network.h"
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
}

FlameState getState() { return currentState; }

void setState(FlameState newState) {
  if (newState == currentState) {
    return;
  }
#ifdef DEBUG
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
#ifdef DEBUG
    Serial.println("Global API timeout triggered; entering ERROR_STATE");
#endif
    setState(ERROR_STATE);
    return;
  }

  // Sync the latest match status from the network module.
  setMatchStatus(network::getRemoteMatchStatus());

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
      // TODO: replace with actual button-hold detection from inputs.cpp.
      if (armingHoldActive && (now - armingHoldStartMs >= BUTTON_HOLD_MS)) {
        setState(ARMING);
      }
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown ||
          currentMatchStatus == WaitingOnFinalData) {
        setState(READY);
      }
      break;

    case ARMING:
      // Hold timer continues; if released early, revert to ACTIVE.
      if (armingHoldActive) {
        if (now - armingHoldStartMs >= BUTTON_HOLD_MS) {
          setState(ARMED);
          armingHoldActive = false;
        }
      } else {
        setState(ACTIVE);
      }

      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown ||
          currentMatchStatus == WaitingOnFinalData) {
        setState(READY);
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
        armingHoldActive = false;
        setState(ON);
      }
      break;
  }
}

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
