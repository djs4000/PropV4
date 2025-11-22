#include "inputs.h"

#define USE_IRREMOTE_HPP_AS_PLAIN_INCLUDE
#include <IRremote.hpp>
#include <Wire.h>
#include <cstring>

#include "effects.h"
#include "game_config.h"

namespace {
constexpr uint8_t KEYPAD_ADDR = 0x20;
constexpr uint8_t BUTTON_ADDR = 0x21;
constexpr uint8_t I2C_SDA = 23;
constexpr uint8_t I2C_SCL = 18;
constexpr uint32_t I2C_FREQ = 100000;
constexpr uint32_t KEY_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

constexpr char KEY_MAP[4][4] = {{'1', '2', '3', 'A'},
                                {'4', '5', '6', 'B'},
                                {'7', '8', '9', 'C'},
                                {'*', '0', '#', 'D'}};

char defuseBuffer[DEFUSE_CODE_LENGTH + 1] = {0};
uint8_t defuseLength = 0;

bool lastButtonsRaw = false;
bool debouncedButtons = false;
uint32_t buttonsChangeMs = 0;
bool holdCompleteFlag = false;
uint32_t holdStartMs = 0;

char lastKeyRaw = '\0';
char debouncedKey = '\0';
uint32_t keyChangeMs = 0;

bool irConfirmFlag = false;

bool writePcf(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readPcf(uint8_t addr, uint8_t &value) {
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

void resetDefuseBufferInternal() {
  memset(defuseBuffer, 0, sizeof(defuseBuffer));
  defuseLength = 0;
}

bool areBothButtonsPressedRaw() {
  uint8_t value = 0xFF;
  if (!readPcf(BUTTON_ADDR, value)) {
    return false;
  }
  const bool buttonA = (value & (1 << 0)) == 0;
  const bool buttonB = (value & (1 << 1)) == 0;
  return buttonA && buttonB;
}

char scanKeypadRaw() {
  for (uint8_t col = 0; col < 4; ++col) {
    uint8_t mask = 0xFF;
    mask &= static_cast<uint8_t>(~(1 << (4 + col)));
    if (!writePcf(KEYPAD_ADDR, mask)) {
      continue;
    }

    uint8_t state = 0xFF;
    if (!readPcf(KEYPAD_ADDR, state)) {
      continue;
    }

    for (uint8_t row = 0; row < 4; ++row) {
      if ((state & (1 << row)) == 0) {
        writePcf(KEYPAD_ADDR, 0xFF);
        return KEY_MAP[row][col];
      }
    }
  }
  writePcf(KEYPAD_ADDR, 0xFF);
  return '\0';
}

void handleDigit(char digit) {
  if (digit < '0' || digit > '9') {
    return;
  }

  if (defuseLength < DEFUSE_CODE_LENGTH) {
    defuseBuffer[defuseLength++] = digit;
    defuseBuffer[defuseLength] = '\0';
  }
}
}  // namespace

namespace inputs {
void init() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  writePcf(KEYPAD_ADDR, 0xFF);
  writePcf(BUTTON_ADDR, 0xFF);
  resetDefuseBufferInternal();

  IrReceiver.begin(27, ENABLE_LED_FEEDBACK);
}

void update() {
  const uint32_t now = millis();

  if (IrReceiver.decode()) {
    irConfirmFlag = true;
    IrReceiver.resume();
  }

  const bool rawButtonsPressed = areBothButtonsPressedRaw();
  if (rawButtonsPressed != lastButtonsRaw) {
    buttonsChangeMs = now;
    lastButtonsRaw = rawButtonsPressed;
  }
  if (now - buttonsChangeMs >= BUTTON_DEBOUNCE_MS && debouncedButtons != rawButtonsPressed) {
    debouncedButtons = rawButtonsPressed;
    if (debouncedButtons) {
      holdStartMs = now;
      holdCompleteFlag = false;
    } else {
      holdStartMs = 0;
    }
  }

  if (debouncedButtons && !holdCompleteFlag && now - holdStartMs >= BUTTON_HOLD_MS) {
    holdCompleteFlag = true;
  }

  const char rawKey = scanKeypadRaw();
  if (rawKey != lastKeyRaw) {
    keyChangeMs = now;
    lastKeyRaw = rawKey;
  }
  if (now - keyChangeMs >= KEY_DEBOUNCE_MS && debouncedKey != rawKey) {
    debouncedKey = rawKey;
    if (debouncedKey != '\0') {
      handleDigit(debouncedKey);
      effects::playKeypadClick();
    }
  }
}

bool isArmingGestureActive() { return debouncedButtons; }

float getArmingProgress01() {
  if (!debouncedButtons || holdStartMs == 0) {
    return 0.0f;
  }
  const uint32_t now = millis();
  const float progress = static_cast<float>(now - holdStartMs) /
                         static_cast<float>(BUTTON_HOLD_MS);
  return progress > 1.0f ? 1.0f : progress;
}

bool consumeArmingHoldComplete() {
  if (holdCompleteFlag) {
    holdCompleteFlag = false;
    return true;
  }
  return false;
}

bool consumeIrConfirmation() {
  if (irConfirmFlag) {
    irConfirmFlag = false;
    return true;
  }
  return false;
}

const char *getDefuseBuffer() { return defuseBuffer; }

uint8_t getDefuseLength() { return defuseLength; }

void clearDefuseBuffer() { resetDefuseBufferInternal(); }
}  // namespace inputs

