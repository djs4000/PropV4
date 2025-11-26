#pragma once

// Stub input module for keypad and buttons.
#include <Arduino.h>

void initInputs();
void updateInputs();
void initIr();
void updateIr();

// Returns true once when an IR confirmation blast is received.
bool consumeIrConfirmation();
void clearIrConfirmation();

// Helper for UI to know how many defuse digits are currently buffered.
uint8_t getEnteredDigits();

// Expose the current defuse buffer for on-screen rendering and reset control.
const char *getDefuseBuffer();
void clearDefuseBuffer();
