#pragma once

#include <Arduino.h>

using TaskFn = void (*)(uint32_t now);

struct Task {
  TaskFn fn;
  uint32_t intervalMs;
  uint32_t lastRunMs;
};

void scheduler_addTask(TaskFn fn, uint32_t intervalMs);
void scheduler_run(uint32_t now);
