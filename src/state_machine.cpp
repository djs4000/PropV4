#include "state_machine.h"

#include "game_config.h"
#include "inputs.h"
#include "network.h"

namespace {
FlameState currentState = ON;
MatchStatus currentMatchStatus = WaitingOnStart;
uint32_t armedTimerMs = DEFAULT_BOMB_DURATION_MS;

bool irWindowActive = false;
uint32_t irWindowStartMs = 0;

void logTransition(FlameState from, FlameState to) {
#ifdef DEBUG
  Serial.print("STATE: ");
  Serial.print(state_machine::flameStateToString(from));
  Serial.print(" -> ");
  Serial.println(state_machine::flameStateToString(to));
#endif
}

bool isGlobalTimeoutTriggered() {
  const uint64_t now = millis();
  const uint64_t lastSuccess = network::getLastSuccessfulApiMs();
  return (now - lastSuccess) >= API_TIMEOUT_MS;
}

void resetIrWindow() {
  irWindowActive = false;
  irWindowStartMs = 0;
}
}  // namespace

namespace state_machine {

void init() { currentState = ON; }

FlameState getState() { return currentState; }

void setState(FlameState newState) {
  if (newState == currentState) {
    return;
  }
  logTransition(currentState, newState);
  currentState = newState;
  if (newState != ARMING) {
    resetIrWindow();
  }
}

void updateState() {
  if (currentState != ERROR_STATE && isGlobalTimeoutTriggered()) {
#ifdef DEBUG
    Serial.println("STATE: Global API timeout");
#endif
    setState(ERROR_STATE);
    return;
  }

  const uint32_t now = millis();

  // keep match status aligned with network results when available
  if (network::hasReceivedApiResponse()) {
    setMatchStatus(network::getRemoteMatchStatus());
  }

  switch (currentState) {
    case ON:
      if (network::hasReceivedApiResponse()) {
        setState(READY);
      } else if (network::hasWifiFailedPermanently()) {
        setState(ERROR_STATE);
      }
      break;

    case READY:
      if (currentMatchStatus == Running) {
        setState(ACTIVE);
      }
      break;

    case ACTIVE:
      if (inputs::consumeArmingGestureStart()) {
        resetIrWindow();
        setState(ARMING);
      }
      if (currentMatchStatus != Running && currentMatchStatus != Countdown &&
          currentMatchStatus != WaitingOnFinalData) {
        setState(READY);
      }
      break;

    case ARMING: {
      if (!irWindowActive && !inputs::areButtonsPressed()) {
        resetIrWindow();
        setState(ACTIVE);
        break;
      }

      if (!irWindowActive && inputs::consumeButtonHoldComplete()) {
        irWindowActive = true;
        irWindowStartMs = now;
      }

      if (irWindowActive) {
        if (inputs::consumeIrConfirmation()) {
          setState(ARMED);
          resetIrWindow();
          break;
        }
        if (now - irWindowStartMs >= IR_CONFIRM_WINDOW_MS) {
          resetIrWindow();
          setState(ACTIVE);
        }
      }
      break;
    }

    case ARMED: {
      if (inputs::consumeDefuseEntryComplete()) {
        const String configured = network::getConfiguredDefuseCode();
        const bool matches = configured.length() == DEFUSE_CODE_LENGTH &&
                             configured.equals(inputs::getDefuseBuffer());
        inputs::resetDefuseBuffer();
        if (matches) {
          setState(DEFUSED);
        }
      }
      break;
    }

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
      if (inputs::consumeButtonHoldComplete()) {
        setState(ON);
      }
      break;
  }
}

void setMatchStatus(MatchStatus status) { currentMatchStatus = status; }

MatchStatus getMatchStatus() { return currentMatchStatus; }

bool isIrConfirmationPending() { return irWindowActive; }

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

}  // namespace state_machine
