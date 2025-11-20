#include <Arduino.h>

#include "effects.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"
#include "util.h"

// Cooperative scheduler timestamps
static unsigned long lastStateUpdateMs = 0;

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
  ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently());

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

  // Drive WiFi-dependent state changes without blocking the loop.
  if (getState() == ON) {
    if (network::isWifiConnected()) {
      setState(READY);
      ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently());
    } else if (network::hasWifiFailedPermanently()) {
      setState(ERROR_STATE);
      ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently());
    }
  }

  // State machine should run frequently but cheaply; simple guard in case
  // future logic needs throttling.
  if (now - lastStateUpdateMs >= 10) {
    lastStateUpdateMs = now;
    updateState();
    ui::renderStatus(getState(), network::isWifiConnected(), network::hasWifiFailedPermanently());
  }
}
