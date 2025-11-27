#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace ui {
struct UiThemeConfig {
  uint16_t backgroundColor = TFT_BLACK;
  uint16_t foregroundColor = TFT_WHITE;
  uint16_t defusedColor = TFT_GREEN;
  uint16_t detonatedBackground = TFT_RED;
  uint16_t detonatedText = TFT_BLACK;
  uint16_t armingYellow = TFT_YELLOW;
  uint16_t armingRed = TFT_RED;
};

struct UiBootModel {
  bool show = false;
  String wifiSsid;
  bool wifiConnected = false;
  bool wifiFailed = false;
  String configApSsid;
  String configApAddress;
  String ipAddress;
  String apiEndpoint;
  bool hasApiResponse = false;
};

struct UiConfigPortalModel {
  bool show = false;
  String ssid;
  String password;
  String portalAddress;
};

struct UiTimerModel {
  uint32_t bombDurationMs = 0;
  uint32_t bombRemainingMs = 0;
  bool bombTimerActive = false;
  bool gameTimerValid = false;
  uint32_t gameTimerRemainingMs = 0;
};

struct UiArmingModel {
  float progress01 = 0.0f;
  bool awaitingIrConfirm = false;
};

struct UiGameModel {
  FlameState state = ON;
  uint8_t codeLength = 0;
  uint8_t enteredDigits = 0;
  String defuseBuffer;
  bool gameOverActive = false;
  MatchStatus matchStatus = WaitingOnStart;
  String ipAddress;
};

struct UiModel {
  UiThemeConfig theme;
  UiBootModel boot;
  UiConfigPortalModel configPortal;
  UiTimerModel timers;
  UiArmingModel arming;
  UiGameModel game;
};

UiThemeConfig defaultUiTheme();
void renderUi(const UiModel &model);
void resetUiState();
}  // namespace ui
