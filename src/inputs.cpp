#include "inputs.h"

#include "game_config.h"
#include "state_machine.h"

// Placeholder implementation to maintain non-blocking structure. The actual
// keypad/button logic will be added later.
void initInputs() {
  // TODO: Verify that no keypad or button inputs are stuck on during boot to catch malfunctions.
}

void updateInputs() {
  // TODO: Poll keypad and buttons, then invoke state changes accordingly.
  // TODO: Suppress arming/reset transitions if buttons were already held when booted.
}
