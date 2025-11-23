#pragma once

#include <Arduino.h>

namespace effects {
// Initialize LED strip and audio output hardware.
void initEffects();

// Startup hardware self-checks.
void startStartupTest();
void updateStartupTest();

void startStartupBeep();
void updateStartupBeep();

// Short acknowledgment when IR confirmation is received.
void startConfirmationBeep();

// Placeholder for ongoing animation/audio updates beyond startup.
void updateEffects();
}  // namespace effects
