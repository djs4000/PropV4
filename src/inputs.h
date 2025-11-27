#pragma once

#include <Arduino.h>

struct InputSnapshot {
  bool bothButtonsHeld = false;
  bool bothButtonsJustReleased = false;
  bool keypadDigitAvailable = false;
  char keypadDigit = '\0';
  bool irBlastReceived = false;
};

// Legacy helpers retained for compatibility with the existing rendering paths.
bool consumeIrConfirmation();
void clearIrConfirmation();
uint8_t getEnteredDigits();
const char *getDefuseBuffer();
void clearDefuseBuffer();

// Initializes keypad, buttons, and IR receiver.
void initInputs();

// Polls keypad/buttons/IR with debouncing. Non-blocking and writes the latest
// snapshot for consumption by the game core.
void updateInputs(InputSnapshot &snapshot);
