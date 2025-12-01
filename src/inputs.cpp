#include <Arduino.h>
#define USE_IRREMOTE_HPP_AS_PLAIN_INCLUDE
#include <IRremote.hpp>

#include "inputs.h"

#include <Wire.h>
#include <cstring>

#include "game_config.h"

namespace {
constexpr uint8_t KEYPAD_ADDR = 0x20;   // PCF8574 for 4x4 keypad
constexpr uint8_t BUTTON_ADDR = 0x21;   // PCF8574 for dual buttons
constexpr uint8_t I2C_SDA = 18; //blue wire
constexpr uint8_t I2C_SCL = 23; //white wire
constexpr uint8_t IR_PIN = 35; //yellow wire

constexpr uint32_t I2C_FREQ = 100000;   // 100kHz per agents.md

constexpr uint32_t KEY_DEBOUNCE_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

// Key map matches a standard 4x4 matrix; only numeric keys are acted upon.
//constexpr char KEY_MAP[4][4] = {{'1', '2', '3', 'A'},
//                                {'4', '5', '6', 'B'},
//                                {'7', '8', '9', 'C'},
//                                {'*', '0', '#', 'D'}};
//
//Rotated to match wiring
constexpr char KEY_MAP[4][4] = {{'1', '4', '7', '*'},
                                {'2', '5', '8', '0'},
                                {'3', '6', '9', '#'},
                                {'A', 'B', 'C', 'D'}};


bool lastButtonsRaw = false;
bool debouncedButtons = false;
uint32_t buttonsChangeMs = 0;

char lastKeyRaw = '\0';
char debouncedKey = '\0';
uint32_t keyChangeMs = 0;

InputSnapshot lastSnapshot{};

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
  // Drive one column low at a time and read the rows.
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

static bool irConfirmationPending = false;

void initIr() { IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK); }

void updateIr() {
  if (IrReceiver.decode()) {
    const bool validPayload = IrReceiver.decodedIRData.protocol != UNKNOWN && IrReceiver.decodedIRData.numberOfBits > 0;
    if (validPayload) {
      irConfirmationPending = true;
    }
    IrReceiver.resume();
  }
}

bool consumeIrConfirmation() {
  if (irConfirmationPending) {
    irConfirmationPending = false;
    return true;
  }
  return false;
}

void clearIrConfirmation() { irConfirmationPending = false; }

void consumeKeypadDigit() {
  debouncedKey = '\0';
  lastKeyRaw = '\0';
}

void initInputs() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);

  // Default all PCF8574 pins high so rows/columns float and buttons read as idle.
  writePcf(KEYPAD_ADDR, 0xFF);
  writePcf(BUTTON_ADDR, 0xFF);

  initIr();
}

InputSnapshot updateInputs() {
  const uint32_t now = millis();

  updateIr();

  // Debounce both-button hold detection for ARMING/ERROR reset.
  const bool rawButtonsPressed = areBothButtonsPressedRaw();
  if (rawButtonsPressed != lastButtonsRaw) {
    buttonsChangeMs = now;
    lastButtonsRaw = rawButtonsPressed;
  }

  if (now - buttonsChangeMs >= BUTTON_DEBOUNCE_MS && debouncedButtons != rawButtonsPressed) {
    debouncedButtons = rawButtonsPressed;
#ifdef APP_DEBUG
    Serial.println(debouncedButtons ? "BUTTONS: both pressed" : "BUTTONS: released");
#endif
  }

  // Debounce keypad entries and surface raw numeric digits without altering
  // game state.
  const char rawKey = scanKeypadRaw();
  if (rawKey != lastKeyRaw) {
    keyChangeMs = now;
    lastKeyRaw = rawKey;
  }

  if (now - keyChangeMs >= KEY_DEBOUNCE_MS && debouncedKey != rawKey) {
    debouncedKey = rawKey;
#ifdef APP_DEBUG
    if (debouncedKey != '\0') {
      Serial.print("KEYPAD: ");
      Serial.println(debouncedKey);
    }
#endif
  }

  lastSnapshot.nowMs = now;
  lastSnapshot.bothButtonsPressed = debouncedButtons;
  lastSnapshot.irConfirmationReceived = irConfirmationPending;
  lastSnapshot.keypadDigitAvailable = (debouncedKey >= '0' && debouncedKey <= '9');
  lastSnapshot.keypadDigit = lastSnapshot.keypadDigitAvailable ? debouncedKey : '\0';

  if (lastSnapshot.irConfirmationReceived) {
    irConfirmationPending = false;
  }

  return lastSnapshot;
}

InputSnapshot getLastInputSnapshot() { return lastSnapshot; }
