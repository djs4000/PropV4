#pragma once

#include <Arduino.h>
#include <functional>

namespace scheduler {
using TaskCallback = std::function<void(uint32_t)>;

struct Task {
  TaskCallback callback;
  uint32_t intervalMs;
  uint32_t lastRunMs;
};

bool addTask(const TaskCallback &callback, uint32_t intervalMs);
void run();
}

