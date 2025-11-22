#include <Arduino.h>

#include "effects.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "ui.h"

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  state_machine::init();
  inputs::init();
  effects::initEffects();
  effects::startStartupTest();
  effects::startStartupBeep();
  ui::initUI();
  network::beginWifi();
}

void loop() {
  inputs::update();
  network::updateWifi();
  network::updateApi();
  network::updateConfigPortal();
  state_machine::updateState();
  effects::updateEffects();
  ui::updateUI();
}
