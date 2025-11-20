#include <Arduino.h>

#include "effects.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"
#include "util.h"

// Cooperative scheduler timestamps
static unsigned long lastStateUpdateMs = 0;

#ifdef DEBUG
// Allows manual state overrides for testing API POST behavior without waiting on
// backend-driven transitions.
static void handleDebugSerialStateChange() {
  if (!Serial.available()) {
    return;
  }

  const int incoming = Serial.read();
  FlameState requestedState = getState();
  bool shouldUpdate = true;

  switch (incoming) {
    case '0':
      requestedState = ON;
      break;
    case '1':
      requestedState = READY;
      break;
    case '2':
      requestedState = ACTIVE;
      break;
    case '3':
      requestedState = ARMING;
      break;
    case '4':
      requestedState = ARMED;
      break;
    case '5':
      requestedState = DEFUSED;
      break;
    case '6':
      requestedState = DETONATED;
      break;
    case '7':
      requestedState = ERROR_STATE;
      break;
    default:
      shouldUpdate = false;
      break;
  }

  if (shouldUpdate) {
    setState(requestedState);
    ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently(),
                     network::getWifiIpString());
  }
}
#endif

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  while (!Serial) {
    // Wait for serial to be ready (non-blocking for ESP32, loop exits immediately).
    break;
  }
#endif

  // Initial state on boot
  setState(ON);

  ui::initDisplay();
  ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently(),
                   network::getWifiIpString());

  effects::initEffects();
  effects::startStartupTest();
  effects::startStartupBeep();

  initInputs();
  ui::initUI();
  network::beginWifi();
}

void loop() {
  const unsigned long now = millis();

  updateInputs();

  // Startup hardware self-checks (LEDs + audio) remain non-blocking via effects::updateEffects().
  effects::updateEffects();

  ui::updateUI();

  // Networking cadence is controlled internally based on API_POST_INTERVAL_MS.
  network::updateWifi();

#ifdef DEBUG
  handleDebugSerialStateChange();
#endif

  if (network::isWifiConnected()) {
    network::updateApi();
  }

  // Drive WiFi-dependent state changes without blocking the loop.
  if (getState() == ON) {
    if (network::isWifiConnected()) {
      setState(READY);
      ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently(),
                       network::getWifiIpString());
    } else if (network::hasWifiFailedPermanently()) {
      setState(ERROR_STATE);
      ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently(),
                       network::getWifiIpString());
    }
  }

  // State machine should run frequently but cheaply; simple guard in case
  // future logic needs throttling.
  if (now - lastStateUpdateMs >= 10) {
    lastStateUpdateMs = now;
    updateState();
    ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently(),
                     network::getWifiIpString());
  }
}
