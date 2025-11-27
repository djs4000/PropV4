#include "ui.h"

#include <TFT_eSPI.h>
#include <algorithm>
#include <climits>
#include <cmath>

#include "state_machine.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
uint16_t BACKGROUND_COLOR = TFT_BLACK;
uint16_t FOREGROUND_COLOR = TFT_WHITE;
uint16_t DEFUSED_COLOR = TFT_GREEN;
uint16_t DETONATED_BG_COLOR = TFT_RED;
uint16_t DETONATED_TEXT_COLOR = TFT_BLACK;
uint16_t ARMING_BAR_YELLOW = TFT_YELLOW;
uint16_t ARMING_BAR_RED = TFT_RED;

// Layout constants tuned for a 240x320 portrait canvas (rotation 0).
constexpr uint8_t TITLE_TEXT_SIZE = 2;
constexpr uint8_t TIMER_TEXT_SIZE = 5;
constexpr uint8_t STATUS_TEXT_SIZE = 2;
constexpr uint8_t BOOT_DETAIL_TEXT_SIZE = 1;
constexpr uint8_t CODE_TEXT_SIZE = 2;
constexpr int16_t STATUS_CLEAR_EXTRA = 3;
constexpr int16_t STATUS_CLEAR_HEIGHT = STATUS_TEXT_SIZE * 8 + 10 + STATUS_CLEAR_EXTRA;

// Shifted downward to keep the title fully visible and better use the canvas height.
constexpr int16_t TITLE_Y = 20;
constexpr int16_t TIMER_Y = 80;
constexpr int16_t TIMER_CLEAR_HEIGHT = 48;
constexpr int16_t STATUS_Y = 150;
constexpr int16_t BAR_Y = 185;
constexpr int16_t BAR_WIDTH = 200;
constexpr int16_t BAR_HEIGHT = 16;
constexpr int16_t BAR_BORDER = 2;
constexpr int16_t CODE_Y = 260;
constexpr uint32_t ARMING_BAR_FRAME_INTERVAL_MS = 40;
constexpr int16_t ARMING_BAR_CHUNK_PX = 8;
constexpr uint32_t TIMER_REDRAW_MIN_INTERVAL_MS = 200;
constexpr uint32_t TIMER_REDRAW_MIN_CS_STEP = 50;  // ~500 ms step for centiseconds

TFT_eSPI tft = TFT_eSPI();
bool screenInitialized = false;
bool layoutDrawn = false;
bool bootLayoutDrawn = false;
UiThemeConfig activeTheme;
enum class UiView { Boot, Config, Main };
UiView currentView = UiView::Boot;

// Cache of the last rendered dynamic values so we avoid unnecessary redraws.
FlameState lastRenderedState = ON;
String lastTimerText;
String lastStatusText;
int16_t lastBarFill = -1;
uint32_t lastArmingBarUpdateMs = 0;
float lastArmingProgress = -1.0f;
uint8_t lastCodeLength = 0;
String lastRenderedCode;
bool renderCacheInvalidated = true;
String lastDebugTimerText;
String lastDebugMatchText;
String lastDebugIpText;
uint16_t lastTimerColor = FOREGROUND_COLOR;
uint16_t lastStatusColor = FOREGROUND_COLOR;
uint16_t lastArmingColor = FOREGROUND_COLOR;
String lastTimerSecondsText;
String lastTimerCentisecondsText;
int16_t lastTimerStartX = -1;
int16_t lastTimerSecondsWidth = 0;
int16_t lastTimerCentisecondsWidth = 0;
uint32_t lastTimerRedrawMs = 0;
uint32_t lastShownCentiseconds = UINT32_MAX;
bool lastGameOverFlag = false;
bool themeInitialized = false;

String lastBootWifiLine;
String lastBootStatusLine;
String lastBootEndpointLine;

bool themesEqual(const UiThemeConfig &a, const UiThemeConfig &b) {
  return a.backgroundColor == b.backgroundColor && a.foregroundColor == b.foregroundColor &&
         a.defusedColor == b.defusedColor && a.detonatedBackground == b.detonatedBackground &&
         a.detonatedText == b.detonatedText && a.armingYellow == b.armingYellow &&
         a.armingRed == b.armingRed;
}

void invalidateRenderCache() {
  layoutDrawn = false;
  bootLayoutDrawn = false;
  renderCacheInvalidated = true;
  lastRenderedState = ON;
  lastTimerText = "";
  lastStatusText = "";
  lastBarFill = -1;
  lastArmingBarUpdateMs = 0;
  lastArmingProgress = -1.0f;
  lastCodeLength = 0;
  lastRenderedCode = "";
  lastDebugTimerText = "";
  lastDebugMatchText = "";
  lastDebugIpText = "";
  lastTimerColor = FOREGROUND_COLOR;
  lastStatusColor = FOREGROUND_COLOR;
  lastArmingColor = FOREGROUND_COLOR;
  lastTimerSecondsText = "";
  lastTimerCentisecondsText = "";
  lastTimerStartX = -1;
  lastTimerSecondsWidth = 0;
  lastTimerCentisecondsWidth = 0;
  lastTimerRedrawMs = 0;
  lastShownCentiseconds = UINT32_MAX;
  lastGameOverFlag = false;
}

void applyTheme(const UiThemeConfig &theme) {
  if (!themeInitialized || !themesEqual(theme, activeTheme)) {
    activeTheme = theme;
    BACKGROUND_COLOR = theme.backgroundColor;
    FOREGROUND_COLOR = theme.foregroundColor;
    DEFUSED_COLOR = theme.defusedColor;
    DETONATED_BG_COLOR = theme.detonatedBackground;
    DETONATED_TEXT_COLOR = theme.detonatedText;
    ARMING_BAR_YELLOW = theme.armingYellow;
    ARMING_BAR_RED = theme.armingRed;
    themeInitialized = true;
    invalidateRenderCache();
  }
}

void setView(UiView view) {
  if (currentView != view) {
    currentView = view;
    invalidateRenderCache();
  }
}

int16_t barX() { return (tft.width() - BAR_WIDTH) / 2; }

void ensureDisplayReady() {
  if (screenInitialized) {
    return;
  }

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(0);  // Portrait orientation matches the mockup.
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
  screenInitialized = true;
}

void drawBootLayout() {
  if (bootLayoutDrawn) {
    return;
  }

  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", 10, 10);

  bootLayoutDrawn = true;
  lastBootWifiLine = "";
  lastBootStatusLine = "";
  lastBootEndpointLine = "";
}

void drawBootLine(const String &text, int16_t y, uint8_t textSize, String &cache) {
  if (text == cache) {
    return;
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(textSize);
  const int16_t clearHeight = textSize * 8 + 2;
  tft.fillRect(0, y, tft.width(), clearHeight, BACKGROUND_COLOR);
  tft.drawString(text, 10, y);
  cache = text;
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

  tft.fillRect(0, y, tft.width(), blockHeight, BACKGROUND_COLOR);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(labelTextSize);
  tft.drawString(label, 10, y);

  tft.setTextSize(valueTextSize);
  tft.drawString(value, 10, y + labelHeight + padding / 2);

  cache = combined;
}

void drawStaticLayout() {
  if (layoutDrawn) {
    return;
  }

  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);

  // Title
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Digital Flame", tft.width() / 2, TITLE_Y);

  // Progress bar outline
  for (int i = 0; i < BAR_BORDER; ++i) {
    tft.drawRect(barX() + i, BAR_Y + i, BAR_WIDTH - 2 * i, BAR_HEIGHT - 2 * i, FOREGROUND_COLOR);
  }

  layoutDrawn = true;
}

void drawCenteredText(const String &text, int16_t y, uint8_t textSize, int16_t clearHeight,
                     uint16_t color = FOREGROUND_COLOR) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(textSize);
  tft.setTextColor(color, BACKGROUND_COLOR);
  const int16_t clearY = y - clearHeight / 2;
  tft.fillRect(0, clearY, tft.width(), clearHeight, BACKGROUND_COLOR);
  tft.drawString(text, tft.width() / 2, y);
}

void clearStatusArea() {
  const int16_t clearY = STATUS_Y - STATUS_CLEAR_HEIGHT / 2;
  tft.fillRect(0, clearY, tft.width(), STATUS_CLEAR_HEIGHT, BACKGROUND_COLOR);
}

void drawStatusLine(const String &text, uint16_t color = FOREGROUND_COLOR) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(STATUS_TEXT_SIZE);
  tft.setTextColor(color, BACKGROUND_COLOR);
  clearStatusArea();
  tft.drawString(text, tft.width() / 2, STATUS_Y);
}

void drawSegmentedTimer(const String &timerText, uint16_t timerColor) {
  const int colonIndex = timerText.indexOf(':');
  const String secondsPart = (colonIndex >= 0) ? timerText.substring(0, colonIndex) : timerText;
  const String centisecondsPart = (colonIndex >= 0 && colonIndex + 1 < timerText.length())
                                      ? timerText.substring(colonIndex + 1)
                                      : String("");
  const String secondsWithColon = secondsPart + ":";

  const bool colorChanged = (timerColor != lastTimerColor);
  const bool secondsChanged = (secondsPart != lastTimerSecondsText);
  const bool centisecondsChanged = (centisecondsPart != lastTimerCentisecondsText);

  // Force a full redraw if layout anchors are unknown so the timer recenters.
  const bool layoutChanged = colorChanged || secondsChanged || lastTimerStartX < 0;

  if (!colorChanged && !secondsChanged && !centisecondsChanged) {
    return;
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TIMER_TEXT_SIZE);
  tft.setTextColor(timerColor, BACKGROUND_COLOR);

  const int16_t textHeight = tft.fontHeight();
  const int16_t secondsWidth = tft.textWidth(secondsWithColon);
  const int16_t centisecondsWidth = tft.textWidth(centisecondsPart);
  const int16_t totalWidth = secondsWidth + centisecondsWidth;
  const int16_t startX = std::max<int16_t>(0, (tft.width() - totalWidth) / 2);
  const int16_t baseY = TIMER_Y - textHeight / 2;

  if (layoutChanged || centisecondsWidth != lastTimerCentisecondsWidth) {
    const int16_t previousTotalWidth = lastTimerSecondsWidth + lastTimerCentisecondsWidth;
    int16_t minStartX = startX;
    int16_t maxEndX = startX + totalWidth;
    if (lastTimerStartX >= 0) {
      minStartX = std::min(minStartX, lastTimerStartX);
      maxEndX = std::max<int16_t>(maxEndX, lastTimerStartX + previousTotalWidth);
    }

    const int16_t clearX = std::max<int16_t>(0, minStartX);
    const int16_t clearWidth = std::min<int16_t>(tft.width() - clearX, maxEndX - minStartX);
    const int16_t clearY = TIMER_Y - TIMER_CLEAR_HEIGHT / 2;

    tft.fillRect(clearX, clearY, clearWidth, TIMER_CLEAR_HEIGHT, BACKGROUND_COLOR);
    tft.drawString(secondsWithColon, startX, baseY);
    tft.drawString(centisecondsPart, startX + secondsWidth, baseY);

    lastTimerStartX = startX;
    lastTimerSecondsWidth = secondsWidth;
    lastTimerCentisecondsWidth = centisecondsWidth;
  } else if (centisecondsChanged) {
    const int16_t clearHeight = TIMER_CLEAR_HEIGHT;
    const int16_t clearY = TIMER_Y - clearHeight / 2;
    const int16_t centisecondsX = lastTimerStartX + lastTimerSecondsWidth;
    const int16_t clearWidth = std::max<int16_t>(centisecondsWidth, lastTimerCentisecondsWidth);
    const int16_t combinedWidth = lastTimerSecondsWidth + clearWidth;
    tft.fillRect(lastTimerStartX, clearY, combinedWidth, clearHeight, BACKGROUND_COLOR);
    tft.drawString(secondsWithColon, lastTimerStartX, baseY);
    tft.drawString(centisecondsPart, centisecondsX, baseY);
    lastTimerCentisecondsWidth = centisecondsWidth;
  }

  lastTimerSecondsText = secondsPart;
  lastTimerCentisecondsText = centisecondsPart;
  lastTimerColor = timerColor;
  lastTimerText = timerText;
}
}  // namespace

namespace ui {
void formatTimeMMSS(uint32_t ms, char *buffer, size_t len) {
  if (len == 0 || buffer == nullptr) {
    return;
  }

  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;

  // Display as MM:SS (minutes first, then seconds) to match the requested format.
  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
}

void formatTimeSSMM(uint32_t ms, char *buffer, size_t len) {
  if (len == 0 || buffer == nullptr) {
    return;
  }

  const uint32_t totalSeconds = ms / 1000;
  const uint32_t centiseconds = (ms % 1000) / 10;

  // Bomb timer uses SS:cc (seconds first, then centiseconds) per UI request.
  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(totalSeconds), static_cast<unsigned>(centiseconds));
}

UiThemeConfig defaultUiTheme() { return UiThemeConfig{}; }

void renderBootView(const UiModel &model) {
  ensureDisplayReady();
  drawBootLayout();

  String wifiLine;
  if (model.boot.wifiFailed) {
    const String apLabel = model.boot.configApSsid.isEmpty() ? String("config AP") : model.boot.configApSsid;
    wifiLine = String("failed → AP ") + apLabel;
  } else if (model.boot.wifiConnected) {
    wifiLine = String("connected (") + (model.boot.ipAddress.isEmpty() ? "IP pending" : model.boot.ipAddress) + ")";
  } else {
    wifiLine = String("connecting to ") + model.boot.wifiSsid;
  }
  const String apiStatusValue =
      model.boot.wifiFailed ? String("Open ") + (model.boot.configApAddress.isEmpty() ? String("http://192.168.4.1")
                                                                                       : model.boot.configApAddress) +
                                   " to configure"
                             : model.boot.hasApiResponse ? "API response received" : "waiting for API response";

  drawBootBlock("WiFi:", wifiLine, 60, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootWifiLine);
  drawBootBlock("Status:", apiStatusValue, 95, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootStatusLine);
  drawBootBlock("Endpoint:", model.boot.apiEndpoint, 150, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE,
                lastBootEndpointLine);
}

void renderConfigPortalView(const UiModel &model) {
  ensureDisplayReady();

  layoutDrawn = false;
  bootLayoutDrawn = false;

  static String lastRenderedSsid;
  static String lastRenderedPassword;
  static String lastRenderedAddress;

  if (lastRenderedSsid == model.configPortal.ssid && lastRenderedPassword == model.configPortal.password &&
      lastRenderedAddress == model.configPortal.portalAddress) {
    return;
  }

  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Config Mode", 10, 10);

  tft.setTextSize(STATUS_TEXT_SIZE);
  tft.drawString("Connect to:", 10, 50);
  tft.drawString(model.configPortal.ssid, 10, 70);

  tft.drawString("Password:", 10, 100);
  tft.drawString(model.configPortal.password, 10, 120);

  tft.setTextSize(BOOT_DETAIL_TEXT_SIZE);
  const String portalAddress = model.configPortal.portalAddress.isEmpty()
                                   ? String("http://192.168.4.1")
                                   : model.configPortal.portalAddress;
  tft.drawString("Open " + portalAddress + " in a browser to update settings.", 10, 160);

  lastRenderedSsid = model.configPortal.ssid;
  lastRenderedPassword = model.configPortal.password;
  lastRenderedAddress = portalAddress;
}

void renderMainView(const UiModel &model) {
  ensureDisplayReady();
  const FlameState state = model.game.state;
  const uint32_t now = millis();

  if (state == DETONATED) {
    if (lastRenderedState != DETONATED || renderCacheInvalidated) {
      layoutDrawn = false;
      renderCacheInvalidated = true;
      tft.fillScreen(DETONATED_BG_COLOR);
      tft.setTextDatum(TC_DATUM);
      tft.setTextSize(TIMER_TEXT_SIZE - 1);
      tft.setTextColor(DETONATED_TEXT_COLOR, DETONATED_BG_COLOR);
      tft.drawString("DETONATED", tft.width() / 2, tft.height() / 2);
    }
    lastRenderedState = state;
    return;
  }

  if (lastRenderedState == DETONATED && state != DETONATED) {
    tft.fillScreen(BACKGROUND_COLOR);
    layoutDrawn = false;
    renderCacheInvalidated = true;
  }

  drawStaticLayout();

  if (renderCacheInvalidated) {
    lastTimerText = "";
    lastStatusText = "";
    lastBarFill = -1;
    lastArmingBarUpdateMs = 0;
    lastArmingProgress = -1.0f;
    lastCodeLength = 0;
    lastRenderedCode = "";
    lastRenderedState = ON;
    lastDebugTimerText = "";
    lastDebugMatchText = "";
    lastDebugIpText = "";
    lastTimerColor = FOREGROUND_COLOR;
    lastStatusColor = FOREGROUND_COLOR;
    lastArmingColor = FOREGROUND_COLOR;
    lastTimerSecondsText = "";
    lastTimerCentisecondsText = "";
    lastTimerStartX = -1;
    lastTimerSecondsWidth = 0;
    lastTimerCentisecondsWidth = 0;
    lastTimerRedrawMs = 0;
    lastShownCentiseconds = UINT32_MAX;
    renderCacheInvalidated = false;
  }

  if (model.arming.awaitingIrConfirm) {
    drawCenteredText("Confirm activation", STATUS_Y, STATUS_TEXT_SIZE, 24);
    lastRenderedState = state;
    return;
  }

  uint32_t timerSource = model.timers.bombDurationMs;
  uint16_t timerColor = (state == DEFUSED) ? DEFUSED_COLOR : FOREGROUND_COLOR;
  const bool bombTimerExpired = (state == DETONATED) && (model.timers.bombRemainingMs == 0);
  const bool bombTimerHeld = (state == DEFUSED);
  const bool bombTimerDisplay = ((state == ARMED) && model.timers.bombTimerActive) || bombTimerExpired ||
                                bombTimerHeld;
  if (bombTimerDisplay) {
    timerSource = model.timers.bombRemainingMs;
    if (state != DEFUSED && timerSource <= 10000) {
      timerColor = TFT_RED;
    }
  } else if (model.timers.gameTimerValid) {
    timerSource = model.timers.gameTimerRemainingMs;
  }

  const uint32_t currentCentiseconds = timerSource / 10;
  const uint32_t csDelta = (lastShownCentiseconds == UINT32_MAX)
                               ? TIMER_REDRAW_MIN_CS_STEP
                               : static_cast<uint32_t>(abs(static_cast<int32_t>(currentCentiseconds -
                                                                                 lastShownCentiseconds)));
  const bool intervalElapsed = (now - lastTimerRedrawMs) >= TIMER_REDRAW_MIN_INTERVAL_MS;
  const bool timerChanged = (currentCentiseconds != lastShownCentiseconds) || (timerColor != lastTimerColor) ||
                            renderCacheInvalidated;
  const bool coarseCentisecondStep = csDelta >= TIMER_REDRAW_MIN_CS_STEP;

  if (timerChanged && (intervalElapsed || coarseCentisecondStep)) {
    char timeBuffer[8] = {0};
    formatTimeSSMM(timerSource, timeBuffer, sizeof(timeBuffer));
    const String timerText = String(timeBuffer);
    drawSegmentedTimer(timerText, timerColor);
    lastTimerRedrawMs = now;
    lastShownCentiseconds = currentCentiseconds;
  }

  String statusText = "Status: ";
  statusText += flameStateToString(state);
  if (model.game.gameOverActive) {
    statusText += " (Game Over)";
  }
  uint16_t statusColor = (state == DEFUSED) ? DEFUSED_COLOR : FOREGROUND_COLOR;
  if (model.game.gameOverActive) {
    statusColor = DETONATED_BG_COLOR;
  }
  if (statusText != lastStatusText || statusColor != lastStatusColor || model.game.gameOverActive != lastGameOverFlag) {
    drawStatusLine(statusText, statusColor);
    lastStatusText = statusText;
    lastStatusColor = statusColor;
    lastGameOverFlag = model.game.gameOverActive;
  }

  const bool stateChanged = (state != lastRenderedState);
  const bool barActive = (state == ARMING) || (state == ARMED);
  const float clampedProgress =
      (state == ARMING) ? constrain(model.arming.progress01, 0.0f, 1.0f) : ((state == ARMED) ? 1.0f : 0.0f);
  const int16_t innerWidth = BAR_WIDTH - 2 * BAR_BORDER;
  const int16_t desiredFill = barActive ? static_cast<int16_t>(innerWidth * clampedProgress) : 0;
  const int16_t quantizedFill =
      (state == ARMING)
          ? static_cast<int16_t>((desiredFill / ARMING_BAR_CHUNK_PX) * ARMING_BAR_CHUNK_PX)
          : desiredFill;
  const bool progressChanged = std::fabs(clampedProgress - lastArmingProgress) >= 0.01f;
  const bool timeElapsed = (now - lastArmingBarUpdateMs) >= ARMING_BAR_FRAME_INTERVAL_MS;
  uint16_t armingColor = FOREGROUND_COLOR;
  if (state == ARMING || state == ARMED) {
    if (state == ARMED || clampedProgress >= 0.75f) {
      armingColor = ARMING_BAR_RED;
    } else if (clampedProgress >= 0.5f) {
      armingColor = ARMING_BAR_YELLOW;
    }
  }

  if (stateChanged && !barActive && lastBarFill > 0) {
    tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, innerWidth, BAR_HEIGHT - 2 * BAR_BORDER,
                 BACKGROUND_COLOR);
    lastBarFill = 0;
    lastArmingProgress = 0.0f;
    lastArmingBarUpdateMs = now;
    lastArmingColor = FOREGROUND_COLOR;
  }

  if (barActive &&
      (progressChanged || timeElapsed || stateChanged || lastBarFill == -1 || armingColor != lastArmingColor)) {
    const int16_t clampedFill = constrain(quantizedFill, static_cast<int16_t>(0), innerWidth);
    const int16_t fillY = BAR_Y + BAR_BORDER;
    const int16_t fillHeight = BAR_HEIGHT - 2 * BAR_BORDER;
    const int16_t fillX = barX() + BAR_BORDER;

    tft.fillRect(fillX, fillY, innerWidth, fillHeight, BACKGROUND_COLOR);
    if (clampedFill > 0) {
      tft.fillRect(fillX, fillY, clampedFill, fillHeight, armingColor);
    }

    lastBarFill = clampedFill;
    lastArmingBarUpdateMs = now;
    lastArmingProgress = clampedProgress;
    lastArmingColor = armingColor;
  }

  if (state == ARMED) {
    const String codeString = model.game.defuseBuffer;
    if (lastRenderedState != ARMED || lastCodeLength != model.game.codeLength || lastRenderedCode != codeString) {
      tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);

      String codeDisplay;
      codeDisplay.reserve(model.game.codeLength * 2);
      for (uint8_t i = 0; i < model.game.codeLength; ++i) {
        if (i < codeString.length()) {
          codeDisplay += codeString[i];
        } else {
          codeDisplay += "_";
        }
        if (i < model.game.codeLength - 1) {
          codeDisplay += " ";
        }
      }

      drawCenteredText(codeDisplay, CODE_Y, CODE_TEXT_SIZE, 24);
      lastCodeLength = model.game.codeLength;
      lastRenderedCode = codeString;
    }
  } else if (lastRenderedState == ARMED) {
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);
    lastCodeLength = 0;
    lastRenderedCode = "";
  }

#ifdef APP_DEBUG
  const int16_t debugHeight = 22;
  const int16_t debugY = tft.height() - debugHeight;
  const String ipOverlay = "IP: " + model.game.ipAddress;
  const String matchOverlay = String("Match ") + matchStatusToString(model.game.matchStatus);
  char gameTimerBuffer[8] = {0};
  if (model.timers.gameTimerValid) {
    formatTimeMMSS(model.timers.gameTimerRemainingMs, gameTimerBuffer, sizeof(gameTimerBuffer));
  } else {
    snprintf(gameTimerBuffer, sizeof(gameTimerBuffer), "--:--");
  }
  const String timerOverlay = String("T ") + String(gameTimerBuffer);

  if (ipOverlay != lastDebugIpText || matchOverlay != lastDebugMatchText || timerOverlay != lastDebugTimerText) {
    tft.fillRect(0, debugY, tft.width(), debugHeight, BACKGROUND_COLOR);
    tft.setTextSize(1);

    tft.setTextDatum(TL_DATUM);
    tft.drawString(matchOverlay, 2, debugY + 2);

    tft.setTextDatum(BL_DATUM);
    tft.drawString(ipOverlay, 2, tft.height() - 2);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(timerOverlay, tft.width() - 2, tft.height() - 2);

    lastDebugIpText = ipOverlay;
    lastDebugMatchText = matchOverlay;
    lastDebugTimerText = timerOverlay;
  }
#endif

  lastRenderedState = state;
}

void renderUi(const UiModel &model) {
  applyTheme(model.theme);
  if (model.configPortal.show) {
    setView(UiView::Config);
  } else if (model.boot.show) {
    setView(UiView::Boot);
  } else {
    setView(UiView::Main);
  }

  switch (currentView) {
    case UiView::Boot:
      renderBootView(model);
      break;
    case UiView::Config:
      renderConfigPortalView(model);
      break;
    case UiView::Main:
      renderMainView(model);
      break;
  }
}

void resetUiState() {
  screenInitialized = false;
  invalidateRenderCache();
  currentView = UiView::Boot;
}

}  // namespace ui
