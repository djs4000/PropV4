#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace effects {
void init();
void update();
void onStateChanged(FlameState state);
void playKeypadClick();
}

