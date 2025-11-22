#include "state_machine.h"

#include "game_config.h"
#include "inputs.h"
#include "network.h"

namespace {
FlameState currentState = ON;
MatchStatus currentMatchStatus = WaitingOnStart;
bool irWindowActive = false;
uint32_t irWindowStartMs = 0;

bool isGlobalTimeoutTriggered() {
  if (getApiMode() != ApiMode::FullOnline) {
    return false;
  }
  const uint64_t now = millis();
  const uint64_t lastSuccess = network::getLastSuccessfulApiMs();
  return network::isWifiConnected() && (now - lastSuccess >= API_TIMEOUT_MS);
}

void clearIrWindow() {
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
  if (newState != ARMING) {
    clearIrWindow();
  }
}

void updateState() {
  if (currentState != ERROR_STATE && isGlobalTimeoutTriggered()) {
#ifdef APP_DEBUG
    Serial.println("STATE: Global API timeout -> ERROR_STATE");
#endif
    setState(ERROR_STATE);
    return;
  }

  if (network::hasReceivedApiResponse()) {
    setMatchStatus(network::getRemoteMatchStatus());
  }

  const uint32_t now = millis();

  switch (currentState) {
    case ON:
      if (network::hasReceivedApiResponse()) {
        setState(READY);
      }
      break;

    case READY:
      if (currentMatchStatus == Running) {
        setState(ACTIVE);
      }
      break;

    case ACTIVE:
      if (inputs::isArmingGestureActive()) {
        setState(ARMING);
      }
      if (currentMatchStatus == WaitingOnStart || currentMatchStatus == Countdown ||
          currentMatchStatus == WaitingOnFinalData) {
        setState(READY);
      }
      break;

    case ARMING: {
      if (!irWindowActive && !inputs::isArmingGestureActive()) {
        setState(ACTIVE);
        break;
      }

      if (!irWindowActive && inputs::consumeArmingHoldComplete()) {
        irWindowActive = true;
        irWindowStartMs = now;
      }

      if (irWindowActive) {
        if (inputs::consumeIrConfirmation()) {
          setState(ARMED);
          clearIrWindow();
        } else if (now - irWindowStartMs >= IR_CONFIRM_WINDOW_MS) {
          setState(ACTIVE);
          clearIrWindow();
        }
      }
      break;
    }

    case ARMED:
      if (currentMatchStatus == Completed || currentMatchStatus == Cancelled) {
        setState(READY);
      }
      break;

    case DEFUSED:
    case DETONATED:
      if (currentMatchStatus == WaitingOnStart) {
        setState(READY);
      }
      break;

    case ERROR_STATE:
      if (inputs::consumeArmingHoldComplete()) {
        clearIrWindow();
        setState(ON);
      }
      break;
  }
}

void setMatchStatus(MatchStatus status) { currentMatchStatus = status; }

MatchStatus getMatchStatus() { return currentMatchStatus; }

bool isIrConfirmWindowActive() { return irWindowActive; }

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
      return "Unknown";
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

