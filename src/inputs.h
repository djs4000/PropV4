#pragma once

// Stub input module for keypad and buttons.
#include <Arduino.h>

struct InputSnapshot {
  bool bothButtonsPressed = false;
  bool irConfirmationReceived = false;
  bool hasKeypadDigit = false;
  char keypadDigit = '\0';
};

void initInputs();
InputSnapshot readInputSnapshot();
