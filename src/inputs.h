#pragma once

#include <Arduino.h>

namespace inputs {
void init();
void update();

bool isArmingGestureActive();
float getArmingProgress01();
bool consumeArmingHoldComplete();

bool consumeIrConfirmation();

const char *getDefuseBuffer();
uint8_t getDefuseLength();
void clearDefuseBuffer();
}  // namespace inputs

