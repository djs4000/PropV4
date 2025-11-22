#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace util {
// Convert string from API to MatchStatus; returns true on successful mapping.
bool parseMatchStatus(const char *statusStr, MatchStatus &outStatus);

void formatTimeMMSS(uint32_t ms, char *buffer, size_t len);
}

