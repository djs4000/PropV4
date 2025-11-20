#pragma once

#include <Arduino.h>

#include "state_machine.h"

// Helper utilities for logging, conversions, and timing.
namespace util {

// Convert string from API to MatchStatus; returns true on successful mapping.
bool parseMatchStatus(const char *statusStr, MatchStatus &outStatus);

}  // namespace util
