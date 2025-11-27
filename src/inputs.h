#pragma once

// Stub input module for keypad and buttons.
#include <Arduino.h>

struct InputSnapshot {
  uint32_t nowMs = 0;
  bool bothButtonsPressed = false;
  bool irConfirmationReceived = false;
  bool keypadDigitAvailable = false;
  char keypadDigit = '\0';
};

void initInputs();
InputSnapshot updateInputs();
InputSnapshot getLastInputSnapshot();
void clearIrConfirmation();
