#include "time_sync.h"

namespace {
TimeSyncState g_timeSync;
}  // namespace

namespace time_sync {

void updateFromServer(int64_t serverEpochMs, uint32_t requestStartMs, uint32_t responseNowMs) {
  const uint32_t rttMs = responseNowMs - requestStartMs;
  const int64_t oneWayMs = static_cast<int64_t>(rttMs) / 2;

  g_timeSync.valid = true;
  g_timeSync.baseServerEpochMs = serverEpochMs + oneWayMs;
  g_timeSync.baseMillis = responseNowMs;
}

int64_t getCurrentEpochMs(uint32_t nowMs) {
  if (!g_timeSync.valid) {
    return 0;
  }

  const uint32_t deltaMs = nowMs - g_timeSync.baseMillis;
  return g_timeSync.baseServerEpochMs + static_cast<int64_t>(deltaMs);
}

bool isValid() { return g_timeSync.valid; }

}  // namespace time_sync

