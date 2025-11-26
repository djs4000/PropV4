#include "time_sync.h"

static TimeSyncState g_timeSync;

bool timeSync_isValid() { return g_timeSync.valid; }

void timeSync_updateFromServer(int64_t serverTicks, uint32_t nowMs) {
  g_timeSync.valid = true;
  g_timeSync.baseServerTicks = serverTicks;
  g_timeSync.baseMillis = nowMs;
}

int64_t timeSync_getCurrentServerTicks(uint32_t nowMs) {
  if (!g_timeSync.valid) {
    return 0;
  }

  const uint32_t deltaMs = nowMs - g_timeSync.baseMillis;
  const int64_t deltaTicks = static_cast<int64_t>(deltaMs) * 10000;
  return g_timeSync.baseServerTicks + deltaTicks;
}

