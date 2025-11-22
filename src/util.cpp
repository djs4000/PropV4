#include "util.h"

#include <cstring>

namespace util {

bool parseMatchStatus(const char *statusStr, MatchStatus &outStatus) {
  if (statusStr == nullptr) {
    return false;
  }

  if (strcmp(statusStr, "WaitingOnStart") == 0) {
    outStatus = WaitingOnStart;
  } else if (strcmp(statusStr, "Countdown") == 0) {
    outStatus = Countdown;
  } else if (strcmp(statusStr, "Running") == 0) {
    outStatus = Running;
  } else if (strcmp(statusStr, "WaitingOnFinalData") == 0) {
    outStatus = WaitingOnFinalData;
  } else if (strcmp(statusStr, "Completed") == 0) {
    outStatus = Completed;
  } else if (strcmp(statusStr, "Cancelled") == 0) {
    outStatus = Cancelled;
  } else {
    return false;
  }

  return true;
}

void formatTimeMMSS(uint32_t ms, char *buffer, size_t len) {
  if (buffer == nullptr || len == 0) {
    return;
  }
  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;
  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(minutes),
           static_cast<unsigned>(seconds));
}

}  // namespace util

