#include "inputs.h"

#include <IRremote.hpp>
#include <Wire.h>
#include <cstring>

#include "game_config.h"

namespace {
constexpr uint8_t KEYPAD_ADDR = 0x20;   // PCF8574 for 4x4 keypad
constexpr uint8_t BUTTON_ADDR = 0x21;   // PCF8574 for dual buttons
constexpr uint8_t I2C_SDA = 23;
constexpr uint8_t I2C_SCL = 18;
constexpr uint32_t I2C_FREQ = 100000;  // 100kHz

constexpr uint32_t KEY_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

constexpr char KEY_MAP[4][4] = {{'1', '2', '3', 'A'},
                                {'4', '5', '6', 'B'},
                                {'7', '8', '9', 'C'},
                                {'*', '0', '#', 'D'}};

char defuseBuffer[DEFUSE_CODE_LENGTH + 1] = {0};
uint8_t defuseLength = 0;
bool defuseEntryComplete = false;

bool lastButtonsRaw = false;
bool debouncedButtons = false;
uint32_t buttonsChangeMs = 0;
bool armingGestureStartFlag = false;
bool buttonHoldCompleteFlag = false;
uint32_t buttonHoldStartMs = 0;

char lastKeyRaw = '\0';
char debouncedKey = '\0';
uint32_t keyChangeMs = 0;

bool irConfirmationPending = false;

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

void resetDefuseInternal() {
  memset(defuseBuffer, 0, sizeof(defuseBuffer));
  defuseLength = 0;
  defuseEntryComplete = false;
}

bool areBothButtonsPressedRaw() {
  uint8_t value = 0xFF;
  if (!readPcf(BUTTON_ADDR, value)) {
    return false;
  }
  const bool buttonA = (value & (1 << 0)) == 0;  // Active low
  const bool buttonB = (value & (1 << 1)) == 0;  // Active low
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

void handleDigitPress(char digit) {
  if (defuseLength >= DEFUSE_CODE_LENGTH) {
    resetDefuseInternal();
  }
  if (defuseLength < DEFUSE_CODE_LENGTH) {
    defuseBuffer[defuseLength++] = digit;
    defuseBuffer[defuseLength] = '\0';
  }
  if (defuseLength == DEFUSE_CODE_LENGTH) {
    defuseEntryComplete = true;
  }
}
}  // namespace

namespace inputs {

void init() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  writePcf(KEYPAD_ADDR, 0xFF);
  writePcf(BUTTON_ADDR, 0xFF);
  resetDefuseInternal();
  IrReceiver.begin(27, ENABLE_LED_FEEDBACK);
}

void update() {
  const uint32_t now = millis();

  // Buttons
  const bool rawButtonsPressed = areBothButtonsPressedRaw();
  if (rawButtonsPressed != lastButtonsRaw) {
    buttonsChangeMs = now;
    lastButtonsRaw = rawButtonsPressed;
  }
  if (now - buttonsChangeMs >= BUTTON_DEBOUNCE_MS && debouncedButtons != rawButtonsPressed) {
    debouncedButtons = rawButtonsPressed;
    if (debouncedButtons) {
      armingGestureStartFlag = true;
      buttonHoldStartMs = now;
      buttonHoldCompleteFlag = false;
    } else {
      buttonHoldStartMs = 0;
      buttonHoldCompleteFlag = false;
    }
  }

  if (debouncedButtons && buttonHoldStartMs != 0 && !buttonHoldCompleteFlag) {
    if (now - buttonHoldStartMs >= BUTTON_HOLD_MS) {
      buttonHoldCompleteFlag = true;
    }
  }

  // Keypad
  const char rawKey = scanKeypadRaw();
  if (rawKey != lastKeyRaw) {
    keyChangeMs = now;
    lastKeyRaw = rawKey;
  }
  if (now - keyChangeMs >= KEY_DEBOUNCE_MS && debouncedKey != rawKey) {
    debouncedKey = rawKey;
    if (debouncedKey >= '0' && debouncedKey <= '9') {
      handleDigitPress(debouncedKey);
    }
  }

  // IR receiver
  if (IrReceiver.decode()) {
    irConfirmationPending = true;
    IrReceiver.resume();
  }
}

bool consumeArmingGestureStart() {
  if (!armingGestureStartFlag) {
    return false;
  }
  armingGestureStartFlag = false;
  return true;
}

bool areButtonsPressed() { return debouncedButtons; }

float armingProgress01() {
  if (!debouncedButtons || buttonHoldStartMs == 0) {
    return 0.0f;
  }
  const uint32_t now = millis();
  const float progress = static_cast<float>(now - buttonHoldStartMs) / static_cast<float>(BUTTON_HOLD_MS);
  return constrain(progress, 0.0f, 1.0f);
}

bool consumeButtonHoldComplete() {
  if (!buttonHoldCompleteFlag) {
    return false;
  }
  buttonHoldCompleteFlag = false;
  return true;
}

bool consumeIrConfirmation() {
  if (!irConfirmationPending) {
    return false;
  }
  irConfirmationPending = false;
  return true;
}

const char *getDefuseBuffer() { return defuseBuffer; }

uint8_t getDefuseLength() { return defuseLength; }

bool consumeDefuseEntryComplete() {
  if (!defuseEntryComplete) {
    return false;
  }
  defuseEntryComplete = false;
  return true;
}

void resetDefuseBuffer() { resetDefuseInternal(); }

}  // namespace inputs
