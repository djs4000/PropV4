#pragma once

#include <Arduino.h>

struct TimeSyncState {
  bool valid = false;
  int64_t baseServerEpochMs = 0;
  uint32_t baseMillis = 0;
};

namespace time_sync {

void updateFromServer(int64_t serverEpochMs, uint32_t requestStartMs, uint32_t responseNowMs);
int64_t getCurrentEpochMs(uint32_t nowMs);
bool isValid();

}  // namespace time_sync

