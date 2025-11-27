#include "core/scheduler.h"

#include <vector>

namespace {
std::vector<Task> tasks;
}

void scheduler_addTask(TaskFn fn, uint32_t intervalMs) {
  tasks.push_back({fn, intervalMs, 0});
}

void scheduler_run(uint32_t now) {
  for (auto &task : tasks) {
    if (task.fn == nullptr) {
      continue;
    }
    if (now - task.lastRunMs >= task.intervalMs) {
      task.lastRunMs = now;
      task.fn(now);
    }
  }
}
