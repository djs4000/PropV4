#pragma once

#include <Arduino.h>
#include "state_machine.h"

namespace effects {
void init();
void update(uint32_t now);

// Optional helpers triggered by events.
void onBoot();                 // boot beep + flash
void onStateChanged(FlameState oldState, FlameState newState);
void onKeypadKey();            // short click beep
void onWrongCode();            // double beep for incorrect code
void onIrConfirmationPrompt(); // prompt when IR confirmation is required
void onArmingConfirmed();      // IR-confirmed arm beep
void setArmingProgress(float progress01);
uint16_t getWrongCodeBeepDurationMs();

// Simple tone helper.
void playBeep(uint16_t frequencyHz, uint16_t durationMs, uint8_t volume = 200, bool sawtooth = false);
}  // namespace effects
