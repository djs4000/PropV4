#pragma once

#include <Arduino.h>

struct TimeSyncState {
  bool valid = false;
  int64_t baseServerTicks = 0;
  uint32_t baseMillis = 0;
};

// Returns true once at least one server timestamp has been observed.
bool timeSync_isValid();

// Updates the local sync baseline using the latest server-provided ticks and
// the local millis() value recorded when the response was received.
void timeSync_updateFromServer(int64_t serverTicks, uint32_t nowMs);

// Estimates the current server ticks based on the last sync point and the
// elapsed local milliseconds since then. Returns 0 when unsynced.
int64_t timeSync_getCurrentServerTicks(uint32_t nowMs);

