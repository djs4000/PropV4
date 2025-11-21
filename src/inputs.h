#pragma once

// Stub input module for keypad and buttons.
void initInputs();
void updateInputs();

// Helper for UI to know how many defuse digits are currently buffered.
uint8_t getEnteredDigits();
