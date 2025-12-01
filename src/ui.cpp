#include "ui.h"

#include <TFT_eSPI.h>
#include <algorithm>
#include <cstring>

namespace {
constexpr uint32_t FRAME_INTERVAL_MS = 1000 / 24;  // target ~24 FPS
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint8_t TITLE_TEXT_SIZE = 2;
constexpr uint8_t TIMER_TEXT_SIZE = 5;
constexpr int16_t TIMER_CLEAR_HEIGHT = 72;
constexpr uint8_t STATUS_TEXT_SIZE = 2;
constexpr int16_t STATUS_CLEAR_HEIGHT = 36;
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
TFT_eSprite timerSprite = TFT_eSprite(&tft);
TFT_eSprite statusSprite = TFT_eSprite(&tft);
UiThemeConfig activeTheme{};
bool screenInitialized = false;
bool spritesInitialized = false;

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
  char wifiLine[96] = {0};
  char statusLine[96] = {0};
  char endpointLine[96] = {0};
} bootCache;

struct ConfigCache {
  bool layoutDrawn = false;
  String ssid;
  String password;
} configCache;

struct MainCache {
  bool layoutDrawn = false;
  bool detonatedDrawn = false;
  char timerText[8] = {0};
  uint16_t timerColor = TFT_BLACK;
  char statusText[48] = {0};
  uint16_t statusColor = TFT_BLACK;
  float armingProgress = -1.0f;
  uint16_t armingColor = TFT_BLACK;
  char codeDisplay[32] = {0};
  bool codeVisible = false;
  bool showArmingPrompt = false;
#ifdef APP_DEBUG
  char debugMatchStatus[32] = {0};
  char debugIp[32] = {0};
  char debugTimer[16] = {0};
  int32_t debugTimerSeconds = -1;
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
  tft.setRotation(2);
  screenInitialized = true;
}

void ensureSpritesReady() {
  if (spritesInitialized || !screenInitialized) {
    return;
  }

  timerSprite.createSprite(tft.width(), TIMER_CLEAR_HEIGHT);
  statusSprite.createSprite(tft.width(), STATUS_CLEAR_HEIGHT);

  spritesInitialized = true;
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
  mainCache.timerText[0] = '\0';
  mainCache.statusText[0] = '\0';
  mainCache.armingProgress = -1.0f;
  mainCache.codeVisible = false;
  mainCache.codeDisplay[0] = '\0';
  mainCache.showArmingPrompt = false;
#ifdef APP_DEBUG
  mainCache.debugMatchStatus[0] = '\0';
  mainCache.debugIp[0] = '\0';
  mainCache.debugTimer[0] = '\0';
  mainCache.debugTimerSeconds = -1;
#endif
}

int16_t barX() { return (tft.width() - BAR_WIDTH) / 2; }

int16_t timerSpriteX() { return 0; }

int16_t timerSpriteY() { return TIMER_Y - (TIMER_CLEAR_HEIGHT / 2); }

int16_t statusSpriteX() { return 0; }

int16_t statusSpriteY() { return STATUS_Y - (STATUS_CLEAR_HEIGHT / 2); }

void drawBootLayout() {
  if (bootCache.layoutDrawn) {
    return;
  }

  tft.fillScreen(activeTheme.backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", 10, 10);

  bootCache.layoutDrawn = true;
  bootCache.wifiLine[0] = '\0';
  bootCache.statusLine[0] = '\0';
  bootCache.endpointLine[0] = '\0';
}

template <size_t N>
void drawBootBlock(const char *label, const char *value, int16_t y, uint8_t labelTextSize, uint8_t valueTextSize,
                   char (&cache)[N]) {
  if (strncmp(value, cache, N) == 0) {
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

  strlcpy(cache, value, N);
}

void drawCenteredText(const char *text, int16_t y, uint8_t textSize, int16_t clearHeight, uint16_t color) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(textSize);
  tft.setTextColor(color, activeTheme.backgroundColor);
  const int16_t clearY = y - clearHeight / 2;
  tft.fillRect(0, clearY, tft.width(), clearHeight, activeTheme.backgroundColor);
  tft.drawString(text, tft.width() / 2, y);
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
}

void drawTimerText(const char *text, uint16_t color) {
  ensureSpritesReady();

  timerSprite.fillSprite(activeTheme.backgroundColor);
  timerSprite.setTextDatum(TC_DATUM);
  timerSprite.setTextSize(TIMER_TEXT_SIZE);
  timerSprite.setTextColor(color, activeTheme.backgroundColor);
  timerSprite.drawString(text, timerSprite.width() / 2, TIMER_Y - timerSpriteY());
  timerSprite.pushSprite(timerSpriteX(), timerSpriteY());
  tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
}

void drawStatusText(const char *text, uint16_t color) {
  ensureSpritesReady();

  statusSprite.fillSprite(activeTheme.backgroundColor);
  statusSprite.setTextDatum(TC_DATUM);
  statusSprite.setTextSize(STATUS_TEXT_SIZE);
  statusSprite.setTextColor(color, activeTheme.backgroundColor);
  statusSprite.drawString(text, statusSprite.width() / 2, STATUS_Y - statusSpriteY());
  statusSprite.pushSprite(statusSpriteX(), statusSpriteY());
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

void formatTimeMMSS(uint32_t ms, char *buffer, size_t len) {
  if (len == 0 || buffer == nullptr) {
    return;
  }

  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;
  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
}

void renderBootScreen(const UiModel &model) {
  drawBootLayout();

  char wifiLine[96] = {0};
  if (model.wifiFailed) {
    const char *apName = model.configApSsid.isEmpty() ? "config AP" : model.configApSsid.c_str();
    snprintf(wifiLine, sizeof(wifiLine), "failed → AP %s", apName);
  } else if (model.wifiConnected) {
    const char *ipValue = model.ipAddress.isEmpty() ? "IP pending" : model.ipAddress.c_str();
    snprintf(wifiLine, sizeof(wifiLine), "connected (%s)", ipValue);
  } else {
    snprintf(wifiLine, sizeof(wifiLine), "connecting to %s", model.wifiSsid.c_str());
  }

  char apiStatusValue[96] = {0};
  if (model.wifiFailed) {
    const char *address = model.configApAddress.isEmpty() ? "http://192.168.4.1" : model.configApAddress.c_str();
    snprintf(apiStatusValue, sizeof(apiStatusValue), "Open %s to configure", address);
  } else if (model.hasApiResponse) {
    strlcpy(apiStatusValue, "API response received", sizeof(apiStatusValue));
  } else {
    strlcpy(apiStatusValue, "waiting for API response", sizeof(apiStatusValue));
  }

  drawBootBlock("WiFi:", wifiLine, 60, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, bootCache.wifiLine);
  drawBootBlock("Status:", apiStatusValue, 95, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, bootCache.statusLine);
  drawBootBlock("Endpoint:", model.apiEndpoint.c_str(), 150, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE,
                bootCache.endpointLine);
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
  if (model.state == DETONATED) {
    if (!mainCache.detonatedDrawn) {
      tft.fillScreen(activeTheme.detonatedBackgroundColor);
      tft.setTextDatum(MC_DATUM);
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
    mainCache.timerText[0] = '\0';
    mainCache.statusText[0] = '\0';
  }

  drawStaticLayout();

  char timerBuffer[8] = {0};
  formatTimeSSMM(model.timerRemainingMs, timerBuffer, sizeof(timerBuffer));
  uint16_t timerColor = (model.state == DEFUSED) ? activeTheme.defusedColor : activeTheme.foregroundColor;
  if (model.bombTimerActive && model.state != DEFUSED && model.timerRemainingMs <= 10000) {
    timerColor = activeTheme.detonatedBackgroundColor;
  }
  if (strcmp(timerBuffer, mainCache.timerText) != 0 || timerColor != mainCache.timerColor) {
    drawTimerText(timerBuffer, timerColor);
    strlcpy(mainCache.timerText, timerBuffer, sizeof(mainCache.timerText));
    mainCache.timerColor = timerColor;
  }

  char statusText[48] = {0};
  uint16_t statusColor = model.gameOver ? activeTheme.detonatedBackgroundColor : activeTheme.foregroundColor;
  if (model.showArmingPrompt) {
    strlcpy(statusText, "Confirm activation", sizeof(statusText));
    statusColor = activeTheme.foregroundColor;
  } else {
    snprintf(statusText, sizeof(statusText), "Status: %s%s", flameStateToString(model.state),
             model.gameOver ? " (Game Over)" : "");
  }
  if (strncmp(statusText, mainCache.statusText, sizeof(mainCache.statusText)) != 0 ||
      statusColor != mainCache.statusColor || mainCache.showArmingPrompt != model.showArmingPrompt) {
    drawStatusText(statusText, statusColor);
    strlcpy(mainCache.statusText, statusText, sizeof(mainCache.statusText));
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
    char codeDisplay[32] = {0};
    size_t idx = 0;
    for (uint8_t i = 0; i < model.codeLength && idx + 1 < sizeof(codeDisplay); ++i) {
      char toWrite = '_';
      if (i < model.defuseBuffer.length()) {
        toWrite = model.defuseBuffer[i];
      } else if (i < model.enteredDigits) {
        toWrite = '_';
      }
      codeDisplay[idx++] = toWrite;
      if (i < model.codeLength - 1 && idx + 1 < sizeof(codeDisplay)) {
        codeDisplay[idx++] = ' ';
      }
    }
    codeDisplay[idx] = '\0';

    if (!mainCache.codeVisible || strncmp(codeDisplay, mainCache.codeDisplay, sizeof(mainCache.codeDisplay)) != 0) {
      drawCenteredText(codeDisplay, CODE_Y, CODE_TEXT_SIZE, 24, activeTheme.foregroundColor);
      strlcpy(mainCache.codeDisplay, codeDisplay, sizeof(mainCache.codeDisplay));
      mainCache.codeVisible = true;
    }
  } else if (mainCache.codeVisible) {
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, activeTheme.backgroundColor);
    mainCache.codeVisible = false;
    mainCache.codeDisplay[0] = '\0';
  }

#ifdef APP_DEBUG
  const int16_t debugHeight = 22;
  const int16_t debugY = tft.height() - debugHeight;
  int32_t debugTimerSeconds = -1;
  char debugTimerValue[8] = {0};
  if (model.debugTimerValid) {
    char debugTimerBuffer[8] = {0};
    formatTimeMMSS(model.debugTimerRemainingMs, debugTimerBuffer, sizeof(debugTimerBuffer));
    strlcpy(debugTimerValue, debugTimerBuffer, sizeof(debugTimerValue));
    debugTimerSeconds = static_cast<int32_t>(model.debugTimerRemainingMs / 1000);
  } else {
    strlcpy(debugTimerValue, "--:--", sizeof(debugTimerValue));
  }

  if (strncmp(model.debugMatchStatus.c_str(), mainCache.debugMatchStatus, sizeof(mainCache.debugMatchStatus)) != 0 ||
      strncmp(model.debugIp.c_str(), mainCache.debugIp, sizeof(mainCache.debugIp)) != 0 ||
      debugTimerSeconds != mainCache.debugTimerSeconds) {
    tft.fillRect(0, debugY, tft.width(), debugHeight, activeTheme.backgroundColor);
    tft.setTextSize(1);
    tft.setTextColor(activeTheme.foregroundColor, activeTheme.backgroundColor);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(model.debugMatchStatus, 2, debugY + 2);
    tft.setTextDatum(BL_DATUM);
    tft.drawString(model.debugIp, 2, tft.height() - 2);
    tft.setTextDatum(BR_DATUM);
    char timerLine[16] = {0};
    snprintf(timerLine, sizeof(timerLine), "T %s", debugTimerValue);
    tft.drawString(timerLine, tft.width() - 2, tft.height() - 2);
    strlcpy(mainCache.debugMatchStatus, model.debugMatchStatus.c_str(), sizeof(mainCache.debugMatchStatus));
    strlcpy(mainCache.debugIp, model.debugIp.c_str(), sizeof(mainCache.debugIp));
    strlcpy(mainCache.debugTimer, debugTimerValue, sizeof(mainCache.debugTimer));
    mainCache.debugTimerSeconds = debugTimerSeconds;
  }
#endif
}

}  // namespace

namespace ui {
UiThemeConfig defaultTheme() { return defaultThemeInternal(); }

void initUI() {
  ensureDisplayReady();
  ensureSpritesReady();
}

void render(const UiModel &model) {
  ensureDisplayReady();
  ensureSpritesReady();

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
