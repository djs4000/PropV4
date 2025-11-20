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

}  // namespace util
