#include "core/scheduler.h"

namespace {
constexpr size_t kMaxTasks = 16;
scheduler::Task tasks[kMaxTasks];
size_t taskCount = 0;
}  // namespace

namespace scheduler {

bool addTask(const TaskCallback &callback, uint32_t intervalMs) {
  if (!callback || intervalMs == 0 || taskCount >= kMaxTasks) {
    return false;
  }

  tasks[taskCount] = {callback, intervalMs, 0};
  ++taskCount;
  return true;
}

void run() {
  const uint32_t now = millis();
  for (size_t i = 0; i < taskCount; ++i) {
    Task &task = tasks[i];
    if (!task.callback) {
      continue;
    }

    if (now - task.lastRunMs >= task.intervalMs) {
      task.lastRunMs = now;
      task.callback(now);
    }
  }
}

}  // namespace scheduler

