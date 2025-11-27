#include "ui.h"

#include <TFT_eSPI.h>
#include <algorithm>

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint8_t TITLE_TEXT_SIZE = 2;
constexpr uint8_t TIMER_TEXT_SIZE = 5;
constexpr uint8_t STATUS_TEXT_SIZE = 2;
constexpr uint8_t BOOT_DETAIL_TEXT_SIZE = 1;
constexpr uint8_t CODE_TEXT_SIZE = 2;

constexpr int16_t TITLE_Y = 20;
constexpr int16_t TIMER_Y = 80;
constexpr int16_t STATUS_Y = 150;
constexpr int16_t BAR_Y = 185;
constexpr int16_t BAR_WIDTH = 200;
constexpr int16_t BAR_HEIGHT = 16;
constexpr int16_t BAR_BORDER = 2;
constexpr int16_t CODE_Y = 260;

TFT_eSPI tft = TFT_eSPI();
UiThemeConfig activeTheme{};
bool screenInitialized = false;
bool layoutDrawn = false;
bool bootLayoutDrawn = false;

String lastBootWifiLine;
String lastBootStatusLine;
String lastBootEndpointLine;

UiThemeConfig defaultThemeInternal() {
  UiThemeConfig theme{};
  theme.backgroundColor = TFT_BLACK;
  theme.foregroundColor = TFT_WHITE;
  theme.defusedColor = TFT_GREEN;
  theme.detonatedBackgroundColor = TFT_RED;
  theme.detonatedTextColor = TFT_BLACK;
  theme.armingBarYellow = TFT_YELLOW;
  theme.armingBarRed = TFT_RED;
  return theme;
}

void ensureDisplayReady() {
  if (screenInitialized) {
    return;
  }

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(0);
  screenInitialized = true;
}

void applyTheme(const UiThemeConfig &theme) {
  activeTheme = theme;
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
}

int16_t barX() { return (tft.width() - BAR_WIDTH) / 2; }

void drawBootLayout() {
  if (bootLayoutDrawn) {
    return;
  }

  tft.fillScreen(activeTheme.backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", 10, 10);

  bootLayoutDrawn = true;
  lastBootWifiLine = "";
  lastBootStatusLine = "";
  lastBootEndpointLine = "";
}

void drawBootBlock(const String &label, const String &value, int16_t y, uint8_t labelTextSize,
                   uint8_t valueTextSize, String &cache) {
  const String combined = label + "|" + value;
  if (combined == cache) {
    return;
  }

  const int16_t labelHeight = labelTextSize * 8;
  const int16_t valueHeight = valueTextSize * 8;
  const int16_t padding = 4;
  const int16_t blockHeight = labelHeight + valueHeight + padding;

  tft.fillRect(0, y, tft.width(), blockHeight, activeTheme.backgroundColor);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(labelTextSize);
  tft.drawString(label, 10, y);

  tft.setTextSize(valueTextSize);
  tft.drawString(value, 10, y + labelHeight + padding / 2);

  cache = combined;
}

void drawCenteredText(const String &text, int16_t y, uint8_t textSize, int16_t clearHeight,
                      uint16_t color) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(textSize);
  tft.setTextColor(color, activeTheme.backgroundColor);
  const int16_t clearY = y - clearHeight / 2;
  tft.fillRect(0, clearY, tft.width(), clearHeight, activeTheme.backgroundColor);
  tft.drawString(text, tft.width() / 2, y);
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
}

void drawStaticLayout() {
  if (layoutDrawn) {
    return;
  }

  tft.fillScreen(activeTheme.backgroundColor);
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);

  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", tft.width() / 2, TITLE_Y);

  for (int i = 0; i < BAR_BORDER; ++i) {
    tft.drawRect(barX() + i, BAR_Y + i, BAR_WIDTH - 2 * i, BAR_HEIGHT - 2 * i, activeTheme.foregroundColor);
  }

  layoutDrawn = true;
}

void formatTimeSSMM(uint32_t ms, char *buffer, size_t len) {
  if (len == 0 || buffer == nullptr) {
    return;
  }

  const uint32_t totalSeconds = ms / 1000;
  const uint32_t centiseconds = (ms % 1000) / 10;
  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(totalSeconds), static_cast<unsigned>(centiseconds));
}

void renderBootScreen(const UiModel &model) {
  drawBootLayout();

  const String wifiLine = model.wifiFailed
                              ? String("failed → AP ") + (model.configApSsid.isEmpty() ? String("config AP") : model.configApSsid)
                              : model.wifiConnected ? String("connected (") + (model.ipAddress.isEmpty() ? "IP pending" : model.ipAddress) + ")"
                                                   : String("connecting to ") + model.wifiSsid;
  const String apiStatusValue = model.wifiFailed
                                    ? String("Open ") + (model.configApAddress.isEmpty() ? String("http://192.168.4.1")
                                                                                           : model.configApAddress) +
                                          " to configure"
                                    : model.hasApiResponse ? "API response received" : "waiting for API response";

  drawBootBlock("WiFi:", wifiLine, 60, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootWifiLine);
  drawBootBlock("Status:", apiStatusValue, 95, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootStatusLine);
  drawBootBlock("Endpoint:", model.apiEndpoint, 150, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootEndpointLine);
}

void renderConfigPortalScreen(const UiModel &model) {
  tft.fillScreen(activeTheme.backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Config Mode", 10, 10);

  tft.setTextSize(STATUS_TEXT_SIZE);
  tft.drawString("Connect to:", 10, 50);
  tft.drawString(model.configApSsid, 10, 70);

  tft.drawString("Password:", 10, 100);
  tft.drawString(model.configApPassword, 10, 120);

  bootLayoutDrawn = false;
  layoutDrawn = false;
}

void renderMainUi(const UiModel &model) {
  drawStaticLayout();

  if (model.state == DETONATED) {
    tft.fillScreen(activeTheme.detonatedBackgroundColor);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(TIMER_TEXT_SIZE - 1);
    tft.setTextColor(activeTheme.detonatedTextColor, activeTheme.detonatedBackgroundColor);
    tft.drawString("DETONATED", tft.width() / 2, tft.height() / 2);
    tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
    layoutDrawn = false;
    return;
  }

  char timerBuffer[8] = {0};
  formatTimeSSMM(model.timerRemainingMs, timerBuffer, sizeof(timerBuffer));
  uint16_t timerColor = (model.state == DEFUSED) ? activeTheme.defusedColor : activeTheme.foregroundColor;
  if (model.bombTimerActive && model.state != DEFUSED && model.timerRemainingMs <= 10000) {
    timerColor = activeTheme.detonatedBackgroundColor;
  }
  drawCenteredText(timerBuffer, TIMER_Y, TIMER_TEXT_SIZE, 48, timerColor);

  String statusText = String("Status: ") + flameStateToString(model.state);
  if (model.gameOver) {
    statusText += " (Game Over)";
  }
  drawCenteredText(statusText, STATUS_Y, STATUS_TEXT_SIZE, 24,
                   model.gameOver ? activeTheme.detonatedBackgroundColor : activeTheme.foregroundColor);

  const float clampedProgress = model.state == ARMING ? std::max(0.0f, std::min(1.0f, model.armingProgress01))
                                                      : (model.state == ARMED ? 1.0f : 0.0f);
  const int16_t innerWidth = BAR_WIDTH - 2 * BAR_BORDER;
  const int16_t fillWidth = static_cast<int16_t>(innerWidth * clampedProgress);
  const int16_t fillY = BAR_Y + BAR_BORDER;
  const int16_t fillHeight = BAR_HEIGHT - 2 * BAR_BORDER;
  const int16_t fillX = barX() + BAR_BORDER;
  tft.fillRect(fillX, fillY, innerWidth, fillHeight, activeTheme.backgroundColor);
  if (fillWidth > 0) {
    uint16_t armingColor = activeTheme.foregroundColor;
    if (model.state == ARMING || model.state == ARMED) {
      if (clampedProgress >= 0.75f) {
        armingColor = activeTheme.armingBarRed;
      } else if (clampedProgress >= 0.5f) {
        armingColor = activeTheme.armingBarYellow;
      }
    }
    tft.fillRect(fillX, fillY, fillWidth, fillHeight, armingColor);
  }

  if (model.state == ARMED) {
    String codeDisplay;
    codeDisplay.reserve(model.codeLength * 2);
    for (uint8_t i = 0; i < model.codeLength; ++i) {
      if (i < model.defuseBuffer.length()) {
        codeDisplay += model.defuseBuffer[i];
      } else if (i < model.enteredDigits) {
        codeDisplay += "_";
      } else {
        codeDisplay += "_";
      }
      if (i < model.codeLength - 1) {
        codeDisplay += " ";
      }
    }
    drawCenteredText(codeDisplay, CODE_Y, CODE_TEXT_SIZE, 24, activeTheme.foregroundColor);
  } else {
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, activeTheme.backgroundColor);
  }

  if (model.showArmingPrompt) {
    drawCenteredText("Confirm activation", STATUS_Y, STATUS_TEXT_SIZE, 24, activeTheme.foregroundColor);
  }

#ifdef APP_DEBUG
  const int16_t debugHeight = 22;
  const int16_t debugY = tft.height() - debugHeight;
  tft.fillRect(0, debugY, tft.width(), debugHeight, activeTheme.backgroundColor);
  tft.setTextSize(1);
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(model.debugMatchStatus, 2, debugY + 2);
  tft.setTextDatum(BL_DATUM);
  tft.drawString(model.debugIp, 2, tft.height() - 2);
  tft.setTextDatum(BR_DATUM);
  char debugTimerBuffer[8] = {0};
  if (model.debugTimerValid) {
    formatTimeSSMM(model.debugTimerRemainingMs, debugTimerBuffer, sizeof(debugTimerBuffer));
  } else {
    snprintf(debugTimerBuffer, sizeof(debugTimerBuffer), "--:--");
  }
  tft.drawString(String("T ") + String(debugTimerBuffer), tft.width() - 2, tft.height() - 2);
#endif
}

}  // namespace

namespace ui {
UiThemeConfig defaultTheme() { return defaultThemeInternal(); }

void initUI() { ensureDisplayReady(); }

void render(const UiModel &model) {
  ensureDisplayReady();
  applyTheme(model.theme);

  if (model.showConfigPortal) {
    renderConfigPortalScreen(model);
    return;
  }

  if (model.showBootScreen) {
    renderBootScreen(model);
    return;
  }

  renderMainUi(model);
}

}  // namespace ui
