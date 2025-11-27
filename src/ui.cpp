#include "ui.h"

#include <TFT_eSPI.h>
#include <algorithm>

namespace {
constexpr uint32_t FRAME_INTERVAL_MS = 1000 / 24;  // target ~24 FPS
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

enum class ScreenMode {
  Boot,
  Config,
  Main,
};

struct RenderState {
  bool hasLastScreen = false;
  ScreenMode lastScreen = ScreenMode::Boot;
  uint32_t lastRenderMs = 0;
  bool themeInitialized = false;
  UiThemeConfig theme{};
} renderState;

struct BootCache {
  bool layoutDrawn = false;
  String wifiLine;
  String statusLine;
  String endpointLine;
} bootCache;

struct ConfigCache {
  bool layoutDrawn = false;
  String ssid;
  String password;
} configCache;

struct MainCache {
  bool layoutDrawn = false;
  bool detonatedDrawn = false;
  String timerText;
  uint16_t timerColor = TFT_BLACK;
  String statusText;
  uint16_t statusColor = TFT_BLACK;
  float armingProgress = -1.0f;
  uint16_t armingColor = TFT_BLACK;
  String codeDisplay;
  bool codeVisible = false;
  bool showArmingPrompt = false;
#ifdef APP_DEBUG
  String debugMatchStatus;
  String debugIp;
  String debugTimer;
#endif
} mainCache;

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

bool themesEqual(const UiThemeConfig &a, const UiThemeConfig &b) {
  return a.backgroundColor == b.backgroundColor && a.foregroundColor == b.foregroundColor &&
         a.defusedColor == b.defusedColor && a.detonatedBackgroundColor == b.detonatedBackgroundColor &&
         a.detonatedTextColor == b.detonatedTextColor && a.armingBarYellow == b.armingBarYellow &&
         a.armingBarRed == b.armingBarRed;
}

void markAllLayoutsDirty() {
  bootCache.layoutDrawn = false;
  configCache.layoutDrawn = false;
  mainCache.layoutDrawn = false;
  mainCache.detonatedDrawn = false;
  mainCache.timerText = "";
  mainCache.statusText = "";
  mainCache.armingProgress = -1.0f;
  mainCache.codeVisible = false;
  mainCache.codeDisplay = "";
  mainCache.showArmingPrompt = false;
#ifdef APP_DEBUG
  mainCache.debugMatchStatus = "";
  mainCache.debugIp = "";
  mainCache.debugTimer = "";
#endif
}

int16_t barX() { return (tft.width() - BAR_WIDTH) / 2; }

void drawBootLayout() {
  if (bootCache.layoutDrawn) {
    return;
  }

  tft.fillScreen(activeTheme.backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", 10, 10);

  bootCache.layoutDrawn = true;
  bootCache.wifiLine = "";
  bootCache.statusLine = "";
  bootCache.endpointLine = "";
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
  if (mainCache.layoutDrawn) {
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

  mainCache.layoutDrawn = true;
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

  drawBootBlock("WiFi:", wifiLine, 60, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, bootCache.wifiLine);
  drawBootBlock("Status:", apiStatusValue, 95, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, bootCache.statusLine);
  drawBootBlock("Endpoint:", model.apiEndpoint, 150, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, bootCache.endpointLine);
}

void renderConfigPortalScreen(const UiModel &model) {
  if (!configCache.layoutDrawn) {
    tft.fillScreen(activeTheme.backgroundColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(TITLE_TEXT_SIZE);
    tft.drawString("Config Mode", 10, 10);

    tft.setTextSize(STATUS_TEXT_SIZE);
    tft.drawString("Connect to:", 10, 50);
    tft.drawString(model.configApSsid, 10, 70);

    tft.drawString("Password:", 10, 100);
    tft.drawString(model.configApPassword, 10, 120);

    configCache.layoutDrawn = true;
    configCache.ssid = model.configApSsid;
    configCache.password = model.configApPassword;
    return;
  }

  if (model.configApSsid != configCache.ssid) {
    tft.fillRect(0, 70, tft.width(), STATUS_TEXT_SIZE * 10, activeTheme.backgroundColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(STATUS_TEXT_SIZE);
    tft.drawString(model.configApSsid, 10, 70);
    configCache.ssid = model.configApSsid;
  }

  if (model.configApPassword != configCache.password) {
    tft.fillRect(0, 120, tft.width(), STATUS_TEXT_SIZE * 10, activeTheme.backgroundColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(STATUS_TEXT_SIZE);
    tft.drawString(model.configApPassword, 10, 120);
    configCache.password = model.configApPassword;
  }
}

void renderMainUi(const UiModel &model) {
  drawStaticLayout();

  if (model.state == DETONATED) {
    if (!mainCache.detonatedDrawn) {
      tft.fillScreen(activeTheme.detonatedBackgroundColor);
      tft.setTextDatum(TC_DATUM);
      tft.setTextSize(TIMER_TEXT_SIZE - 1);
      tft.setTextColor(activeTheme.detonatedTextColor, activeTheme.detonatedBackgroundColor);
      tft.drawString("DETONATED", tft.width() / 2, tft.height() / 2);
      tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
      mainCache.detonatedDrawn = true;
      mainCache.layoutDrawn = false;
    }
    return;
  }

  if (mainCache.detonatedDrawn) {
    mainCache.detonatedDrawn = false;
    mainCache.layoutDrawn = false;
    mainCache.armingProgress = -1.0f;
    mainCache.codeVisible = false;
    mainCache.timerText = "";
    mainCache.statusText = "";
  }

  char timerBuffer[8] = {0};
  formatTimeSSMM(model.timerRemainingMs, timerBuffer, sizeof(timerBuffer));
  uint16_t timerColor = (model.state == DEFUSED) ? activeTheme.defusedColor : activeTheme.foregroundColor;
  if (model.bombTimerActive && model.state != DEFUSED && model.timerRemainingMs <= 10000) {
    timerColor = activeTheme.detonatedBackgroundColor;
  }
  const String timerString = String(timerBuffer);
  if (timerString != mainCache.timerText || timerColor != mainCache.timerColor) {
    drawCenteredText(timerString, TIMER_Y, TIMER_TEXT_SIZE, 48, timerColor);
    mainCache.timerText = timerString;
    mainCache.timerColor = timerColor;
  }

  String statusText = String("Status: ") + flameStateToString(model.state);
  if (model.gameOver) {
    statusText += " (Game Over)";
  }
  uint16_t statusColor = model.gameOver ? activeTheme.detonatedBackgroundColor : activeTheme.foregroundColor;
  if (model.showArmingPrompt) {
    statusText = "Confirm activation";
    statusColor = activeTheme.foregroundColor;
  }
  if (statusText != mainCache.statusText || statusColor != mainCache.statusColor ||
      mainCache.showArmingPrompt != model.showArmingPrompt) {
    drawCenteredText(statusText, STATUS_Y, STATUS_TEXT_SIZE, 24, statusColor);
    mainCache.statusText = statusText;
    mainCache.statusColor = statusColor;
    mainCache.showArmingPrompt = model.showArmingPrompt;
  }

  const float clampedProgress = model.state == ARMING ? std::max(0.0f, std::min(1.0f, model.armingProgress01))
                                                      : (model.state == ARMED ? 1.0f : 0.0f);
  uint16_t armingColor = activeTheme.foregroundColor;
  if (model.state == ARMING || model.state == ARMED) {
    if (clampedProgress >= 0.75f) {
      armingColor = activeTheme.armingBarRed;
    } else if (clampedProgress >= 0.5f) {
      armingColor = activeTheme.armingBarYellow;
    }
  }
  if (clampedProgress != mainCache.armingProgress || armingColor != mainCache.armingColor || !mainCache.layoutDrawn) {
    const int16_t innerWidth = BAR_WIDTH - 2 * BAR_BORDER;
    const int16_t fillWidth = static_cast<int16_t>(innerWidth * clampedProgress);
    const int16_t fillY = BAR_Y + BAR_BORDER;
    const int16_t fillHeight = BAR_HEIGHT - 2 * BAR_BORDER;
    const int16_t fillX = barX() + BAR_BORDER;
    tft.fillRect(fillX, fillY, innerWidth, fillHeight, activeTheme.backgroundColor);
    if (fillWidth > 0) {
      tft.fillRect(fillX, fillY, fillWidth, fillHeight, armingColor);
    }
    mainCache.armingProgress = clampedProgress;
    mainCache.armingColor = armingColor;
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
    if (!mainCache.codeVisible || codeDisplay != mainCache.codeDisplay) {
      drawCenteredText(codeDisplay, CODE_Y, CODE_TEXT_SIZE, 24, activeTheme.foregroundColor);
      mainCache.codeDisplay = codeDisplay;
      mainCache.codeVisible = true;
    }
  } else if (mainCache.codeVisible) {
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, activeTheme.backgroundColor);
    mainCache.codeVisible = false;
    mainCache.codeDisplay = "";
  }

#ifdef APP_DEBUG
  const int16_t debugHeight = 22;
  const int16_t debugY = tft.height() - debugHeight;
  String debugTimerValue;
  if (model.debugTimerValid) {
    char debugTimerBuffer[8] = {0};
    formatTimeSSMM(model.debugTimerRemainingMs, debugTimerBuffer, sizeof(debugTimerBuffer));
    debugTimerValue = String(debugTimerBuffer);
  } else {
    debugTimerValue = "--:--";
  }

  if (model.debugMatchStatus != mainCache.debugMatchStatus || model.debugIp != mainCache.debugIp ||
      debugTimerValue != mainCache.debugTimer) {
    tft.fillRect(0, debugY, tft.width(), debugHeight, activeTheme.backgroundColor);
    tft.setTextSize(1);
    tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(model.debugMatchStatus, 2, debugY + 2);
    tft.setTextDatum(BL_DATUM);
    tft.drawString(model.debugIp, 2, tft.height() - 2);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(String("T ") + String(debugTimerValue), tft.width() - 2, tft.height() - 2);
    mainCache.debugMatchStatus = model.debugMatchStatus;
    mainCache.debugIp = model.debugIp;
    mainCache.debugTimer = debugTimerValue;
  }
#endif
}

}  // namespace

namespace ui {
UiThemeConfig defaultTheme() { return defaultThemeInternal(); }

void initUI() { ensureDisplayReady(); }

void render(const UiModel &model) {
  ensureDisplayReady();

  const ScreenMode currentScreen = model.showConfigPortal
                                       ? ScreenMode::Config
                                       : (model.showBootScreen ? ScreenMode::Boot : ScreenMode::Main);

  const bool themeChanged = !renderState.themeInitialized || !themesEqual(renderState.theme, model.theme);
  if (themeChanged) {
    applyTheme(model.theme);
    renderState.theme = model.theme;
    renderState.themeInitialized = true;
    markAllLayoutsDirty();
  } else if (!renderState.themeInitialized) {
    applyTheme(model.theme);
    renderState.theme = model.theme;
    renderState.themeInitialized = true;
  }

  const bool screenChanged = !renderState.hasLastScreen || renderState.lastScreen != currentScreen;
  const uint32_t now = millis();
  if (!screenChanged && !themeChanged && renderState.hasLastScreen &&
      (now - renderState.lastRenderMs) < FRAME_INTERVAL_MS) {
    return;
  }

  if (screenChanged) {
    markAllLayoutsDirty();
  }

  switch (currentScreen) {
    case ScreenMode::Config:
      renderConfigPortalScreen(model);
      break;
    case ScreenMode::Boot:
      renderBootScreen(model);
      break;
    case ScreenMode::Main:
      renderMainUi(model);
      break;
  }

  renderState.lastScreen = currentScreen;
  renderState.hasLastScreen = true;
  renderState.lastRenderMs = now;
}

}  // namespace ui
