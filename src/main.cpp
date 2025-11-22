#include <Arduino.h>
#include <IRremote.hpp>

#include "effects.h"
#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"

void setup() {
#ifdef APP_DEBUG
  Serial.begin(115200);
#endif
  setState(ON);
  effects::init();
  inputs::init();
  ui::init();
  network::beginWifi();
}

void loop() {
  static FlameState lastState = ON;

  inputs::update();
  network::updateWifi();
  network::updateApi();
  network::updateConfigPortal();
  updateState();
  effects::update();
  ui::update();

  const FlameState current = getState();
  if (current != lastState) {
    effects::onStateChanged(current);
    lastState = current;
  }
}

