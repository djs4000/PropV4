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

  initEffects();
  initInputs();
  initUI();
  initNetwork();

  // Initial state on boot
  setState(ON);
}

void loop() {
  const unsigned long now = millis();

  updateInputs();
  updateEffects();
  updateUI();

  // Networking cadence is controlled internally based on API_POST_INTERVAL_MS.
  updateNetwork();

  // State machine should run frequently but cheaply; simple guard in case
  // future logic needs throttling.
  if (now - lastStateUpdateMs >= 10) {
    lastStateUpdateMs = now;
    updateState();
  }
}
