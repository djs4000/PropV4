#pragma once

#include <Arduino.h>

namespace inputs {

void init();
void update();

// Button/arming helpers
bool consumeArmingGestureStart();
bool areButtonsPressed();
float armingProgress01();
bool consumeButtonHoldComplete();

// IR confirmation helpers
bool consumeIrConfirmation();

// Defuse buffer helpers
const char *getDefuseBuffer();
uint8_t getDefuseLength();
bool consumeDefuseEntryComplete();
void resetDefuseBuffer();

}  // namespace inputs
