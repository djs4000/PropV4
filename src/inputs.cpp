#include <Arduino.h>
#define USE_IRREMOTE_HPP_AS_PLAIN_INCLUDE
#include <IRremote.hpp>

#include "inputs.h"

#include <Wire.h>

#include "game_config.h"

namespace {
constexpr uint8_t KEYPAD_ADDR = 0x20;   // PCF8574 for 4x4 keypad
constexpr uint8_t BUTTON_ADDR = 0x21;   // PCF8574 for dual buttons
constexpr uint8_t I2C_SDA = 23;
constexpr uint8_t I2C_SCL = 18;
constexpr uint32_t I2C_FREQ = 100000;   // 100kHz per agents.md

constexpr uint32_t KEY_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

// Key map matches a standard 4x4 matrix; only numeric keys are surfaced.
constexpr char KEY_MAP[4][4] = {{'1', '2', '3', 'A'},
                                {'4', '5', '6', 'B'},
                                {'7', '8', '9', 'C'},
                                {'*', '0', '#', 'D'}};

static char defuseBuffer[DEFUSE_CODE_LENGTH + 1] = {0};
static uint8_t enteredDigits = 0;
static bool irConfirmationPending = false;

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

bool areBothButtonsPressedRaw() {
  uint8_t value = 0;
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
    mask &= static_cast<uint8_t>(~(1 << (4 + col)));  // Pull selected column low

    if (!writePcf(KEYPAD_ADDR, mask)) {
      continue;
    }

    uint8_t state = 0xFF;
    if (!readPcf(KEYPAD_ADDR, state)) {
      continue;
    }

    for (uint8_t row = 0; row < 4; ++row) {
      if ((state & (1 << row)) == 0) {  // Active low row indicates press
        // Restore idle high state before returning.
        writePcf(KEYPAD_ADDR, 0xFF);
        return KEY_MAP[row][col];
      }
    }
  }

  // Release all lines high when no key is detected.
  writePcf(KEYPAD_ADDR, 0xFF);
  return '\0';
}
}  // namespace

void initInputs() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);

  // Default all PCF8574 pins high so rows/columns float and buttons read as idle.
  writePcf(KEYPAD_ADDR, 0xFF);
  writePcf(BUTTON_ADDR, 0xFF);

  IrReceiver.begin(27, ENABLE_LED_FEEDBACK);
}

void updateInputs(InputSnapshot &snapshot) {
  const uint32_t now = millis();
  static bool lastButtonsRaw = false;
  static bool debouncedButtons = false;
  static uint32_t buttonsChangeMs = 0;

  static char lastKeyRaw = '\0';
  static char debouncedKey = '\0';
  static uint32_t keyChangeMs = 0;

  if (IrReceiver.decode()) {
    const bool validPayload = IrReceiver.decodedIRData.protocol != UNKNOWN && IrReceiver.decodedIRData.numberOfBits > 0;
    if (validPayload) {
      irConfirmationPending = true;
    }
    IrReceiver.resume();
  }

  // Debounce both-button hold detection for ARMING/ERROR reset.
  const bool rawButtonsPressed = areBothButtonsPressedRaw();
  if (rawButtonsPressed != lastButtonsRaw) {
    buttonsChangeMs = now;
    lastButtonsRaw = rawButtonsPressed;
  }

  if (now - buttonsChangeMs >= BUTTON_DEBOUNCE_MS && debouncedButtons != rawButtonsPressed) {
    snapshot.bothButtonsJustReleased = debouncedButtons && !rawButtonsPressed;
    debouncedButtons = rawButtonsPressed;
#ifdef APP_DEBUG
    Serial.println(debouncedButtons ? "BUTTONS: both pressed" : "BUTTONS: released");
#endif
  } else {
    snapshot.bothButtonsJustReleased = false;
  }
  snapshot.bothButtonsHeld = debouncedButtons;

  // Debounce keypad entries to feed into the game core.
  const char rawKey = scanKeypadRaw();
  if (rawKey != lastKeyRaw) {
    keyChangeMs = now;
    lastKeyRaw = rawKey;
  }

  snapshot.keypadDigitAvailable = false;
  if (now - keyChangeMs >= KEY_DEBOUNCE_MS && debouncedKey != rawKey) {
    debouncedKey = rawKey;
    if (debouncedKey >= '0' && debouncedKey <= '9') {
      snapshot.keypadDigitAvailable = true;
      snapshot.keypadDigit = debouncedKey;
      if (enteredDigits < DEFUSE_CODE_LENGTH) {
        defuseBuffer[enteredDigits++] = debouncedKey;
        defuseBuffer[enteredDigits] = '\0';
      } else {
        enteredDigits = 0;
        defuseBuffer[enteredDigits] = '\0';
      }
#ifdef APP_DEBUG
      Serial.print("KEYPAD: ");
      Serial.println(debouncedKey);
#endif
    }
  }

  snapshot.irBlastReceived = irConfirmationPending;
}

bool consumeIrConfirmation() {
  if (irConfirmationPending) {
    irConfirmationPending = false;
    return true;
  }
  return false;
}

void clearIrConfirmation() { irConfirmationPending = false; }

uint8_t getEnteredDigits() { return enteredDigits; }

const char *getDefuseBuffer() { return defuseBuffer; }

void clearDefuseBuffer() {
  enteredDigits = 0;
  defuseBuffer[0] = '\0';
}
