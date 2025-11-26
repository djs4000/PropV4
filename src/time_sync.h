#pragma once

#include <Arduino.h>

struct TimeSyncState {
  bool valid = false;
  int64_t baseServerTicks = 0;
  uint32_t baseMillis = 0;
};

namespace time_sync {

void updateFromServer(int64_t serverTicks, uint32_t nowMs);
int64_t getCurrentServerTicks(uint32_t nowMs);
bool isValid();

}  // namespace time_sync

