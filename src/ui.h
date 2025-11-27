#pragma once

#include <Arduino.h>

#include "state_machine.h"

struct UiThemeConfig {
  uint16_t backgroundColor;
  uint16_t foregroundColor;
  uint16_t defusedColor;
  uint16_t detonatedBackgroundColor;
  uint16_t detonatedTextColor;
  uint16_t armingBarYellow;
  uint16_t armingBarRed;
};

struct UiModel {
  bool showBootScreen = false;
  bool showConfigPortal = false;
  bool showArmingPrompt = false;
  bool gameOver = false;

  FlameState state = ON;
  uint32_t bombDurationMs = 0;
  uint32_t timerRemainingMs = 0;
  bool bombTimerActive = false;
  bool bombTimerExpired = false;
  float armingProgress01 = 0.0f;
  uint8_t codeLength = DEFUSE_CODE_LENGTH;
  uint8_t enteredDigits = 0;
  String defuseBuffer;

  String wifiSsid;
  bool wifiConnected = false;
  bool wifiFailed = false;
  String configApSsid;
  String configApAddress;
  String configApPassword;
  String ipAddress;
  String apiEndpoint;
  bool hasApiResponse = false;

  String debugIp;
  String debugMatchStatus;
  bool debugTimerValid = false;
  uint32_t debugTimerRemainingMs = 0;

  UiThemeConfig theme{};
};

namespace ui {
UiThemeConfig defaultTheme();

void initUI();
void render(const UiModel &model);
}  // namespace ui
