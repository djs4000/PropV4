#include "ui.h"

#include <TFT_eSPI.h>
#include <algorithm>
#include <cmath>

#include "network.h"
#include "state_machine.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t FOREGROUND_COLOR = TFT_WHITE;

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

TFT_eSPI tft = TFT_eSPI();
bool screenInitialized = false;
bool layoutDrawn = false;
bool bootLayoutDrawn = false;

// Cache of the last rendered dynamic values so we avoid unnecessary redraws.
FlameState lastRenderedState = ON;
String lastTimerText;
String lastStatusText;
int16_t lastBarFill = -1;
uint32_t lastArmingBarUpdateMs = 0;
float lastArmingProgress = -1.0f;
uint8_t lastCodeLength = 0;
bool renderCacheInvalidated = true;
String lastDebugTimerText;
String lastDebugMatchText;
String lastDebugIpText;
uint16_t lastTimerColor = FOREGROUND_COLOR;
String lastTimerSecondsText;
String lastTimerCentisecondsText;

String lastBootWifiLine;
String lastBootStatusLine;
String lastBootEndpointLine;

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

void drawStatusLine(const String &text) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(STATUS_TEXT_SIZE);
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

  if (!colorChanged && !secondsChanged && !centisecondsChanged) {
    return;
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TIMER_TEXT_SIZE);
  tft.setTextColor(timerColor, BACKGROUND_COLOR);

  const int16_t textHeight = TIMER_TEXT_SIZE * 8;
  const int16_t secondsWidth = tft.textWidth(secondsWithColon, TIMER_TEXT_SIZE);
  const int16_t centisecondsWidth = tft.textWidth(centisecondsPart, TIMER_TEXT_SIZE);
  const int16_t totalWidth = secondsWidth + centisecondsWidth;
  const int16_t startX = std::max<int16_t>(0, (tft.width() - totalWidth) / 2);
  const int16_t baseY = TIMER_Y - textHeight / 2;
  const int16_t secondsX = startX;
  const int16_t centisecondsX = secondsX + secondsWidth;

  if (colorChanged || secondsChanged) {
    const int16_t clearY = TIMER_Y - TIMER_CLEAR_HEIGHT / 2;
    const int16_t clearWidth = std::min<int16_t>(tft.width(), totalWidth);
    const int16_t clearX = (clearWidth == tft.width()) ? 0 : startX;

    tft.fillRect(clearX, clearY, clearWidth, TIMER_CLEAR_HEIGHT, BACKGROUND_COLOR);
    tft.drawString(secondsWithColon, secondsX, baseY);
    tft.drawString(centisecondsPart, centisecondsX, baseY);
  } else if (centisecondsChanged) {
    const int16_t previousWidth = tft.textWidth(lastTimerCentisecondsText, TIMER_TEXT_SIZE);
    const int16_t clearWidth = std::max<int16_t>(centisecondsWidth, previousWidth);
    tft.fillRect(centisecondsX, baseY, clearWidth, textHeight, BACKGROUND_COLOR);
    tft.drawString(centisecondsPart, centisecondsX, baseY);
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

void renderBootScreen(const String &wifiSsid, bool wifiConnected, bool wifiFailed,
                      const String &configApSsid, const String &configApAddress,
                      const String &ipAddress, const String &apiEndpoint,
                      bool hasApiResponse) {
  ensureDisplayReady();
  drawBootLayout();

  String wifiLine;
  if (wifiFailed) {
    const String apLabel = configApSsid.isEmpty() ? String("config AP") : configApSsid;
    wifiLine = String("failed → AP ") + apLabel;
  } else if (wifiConnected) {
    wifiLine = String("connected (") + (ipAddress.isEmpty() ? "IP pending" : ipAddress) + ")";
  } else {
    wifiLine = String("connecting to ") + wifiSsid;
  }
  const String apiStatusValue =
      wifiFailed ? String("Open ") + (configApAddress.isEmpty() ? String("http://192.168.4.1") : configApAddress) +
                       " to configure"
                 : hasApiResponse ? "API response received" : "waiting for API response";

  drawBootBlock("WiFi:", wifiLine, 60, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootWifiLine);
  drawBootBlock("Status:", apiStatusValue, 95, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootStatusLine);
  drawBootBlock("Endpoint:", apiEndpoint, 150, STATUS_TEXT_SIZE, BOOT_DETAIL_TEXT_SIZE, lastBootEndpointLine);
}

void initMainScreen() {
  ensureDisplayReady();
  bootLayoutDrawn = false;  // Force boot overlay to redraw if revisited later.
  layoutDrawn = false;  // Force redraw of static layout if called again.
  drawStaticLayout();

  // Prime dynamic regions to a clean state.
  drawCenteredText("--:--", TIMER_Y, TIMER_TEXT_SIZE, 48);
  drawStatusLine("Status:");

  // Clear progress fill area.
  tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, BAR_WIDTH - 2 * BAR_BORDER,
               BAR_HEIGHT - 2 * BAR_BORDER, BACKGROUND_COLOR);

  // Clear potential defuse code area.
  tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);

  // Ensure the next render repaints all dynamic elements after the reset.
  renderCacheInvalidated = true;
}

void renderState(FlameState state, uint32_t bombDurationMs, uint32_t remainingMs, float armingProgress01,
                 uint8_t codeLength, uint8_t /*enteredDigits*/) {
  ensureDisplayReady();
  drawStaticLayout();

  // If the layout was reinitialized (e.g., after leaving config mode), drop the
  // caches so every dynamic element is redrawn on this pass.
  if (renderCacheInvalidated) {
    lastTimerText = "";
    lastStatusText = "";
    lastBarFill = -1;
    lastArmingBarUpdateMs = 0;
    lastArmingProgress = -1.0f;
    lastCodeLength = 0;
    lastRenderedState = ON;
    lastDebugTimerText = "";
    lastDebugMatchText = "";
    lastDebugIpText = "";
    lastTimerColor = FOREGROUND_COLOR;
    lastTimerSecondsText = "";
    lastTimerCentisecondsText = "";
    renderCacheInvalidated = false;
  }

  // Timer
  const bool bombTimerExpired = (state == DETONATED) && (getBombTimerRemainingMs() == 0);
  const bool bombTimerDisplay = ((state == ARMED) && isBombTimerActive()) || bombTimerExpired;
  uint32_t timerSource = 0;
  uint16_t timerColor = FOREGROUND_COLOR;

  char timeBuffer[8] = {0};
  if (bombTimerDisplay) {
    timerSource = getBombTimerRemainingMs();
    timerColor = (timerSource <= 10000) ? TFT_RED : FOREGROUND_COLOR;
  } else {
    timerSource = bombDurationMs;
  }

  formatTimeSSMM(timerSource, timeBuffer, sizeof(timeBuffer));

  const String timerText = String(timeBuffer);
  drawSegmentedTimer(timerText, timerColor);

  // Status line
  String statusText = "Status: ";
  statusText += flameStateToString(state);
  if (statusText != lastStatusText) {
    drawStatusLine(statusText);
    lastStatusText = statusText;
  }

  // Progress bar fill (only visible during arming, empty otherwise)
  const float clampedProgress = (state == ARMING) ? constrain(armingProgress01, 0.0f, 1.0f) : 0.0f;
  const int16_t innerWidth = BAR_WIDTH - 2 * BAR_BORDER;
  const int16_t fillWidth = static_cast<int16_t>(innerWidth * clampedProgress);
  const uint32_t now = millis();
  const bool progressChanged = std::fabs(clampedProgress - lastArmingProgress) >= 0.01f;
  const bool timeElapsed = (now - lastArmingBarUpdateMs) >= ARMING_BAR_FRAME_INTERVAL_MS;
  const bool stateChanged = (state != lastRenderedState);

  if (progressChanged || timeElapsed || stateChanged || lastBarFill == -1) {
    tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, innerWidth, BAR_HEIGHT - 2 * BAR_BORDER,
                 BACKGROUND_COLOR);
    if (fillWidth > 0) {
      tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, fillWidth, BAR_HEIGHT - 2 * BAR_BORDER,
                   FOREGROUND_COLOR);
    }
    lastBarFill = fillWidth;
    lastArmingBarUpdateMs = now;
    lastArmingProgress = clampedProgress;
  }

  // Defuse code placeholders are only shown when armed.
  if (state == ARMED) {
    if (lastRenderedState != ARMED || lastCodeLength != codeLength) {
      tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);

      String placeholders;
      placeholders.reserve(codeLength * 2);
      for (uint8_t i = 0; i < codeLength; ++i) {
        placeholders += "_";
        if (i < codeLength - 1) {
          placeholders += " ";
        }
      }

      drawCenteredText(placeholders, CODE_Y, CODE_TEXT_SIZE, 24);
      lastCodeLength = codeLength;
    }
  } else if (lastRenderedState == ARMED) {
    // Clear placeholders when leaving ARMED.
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);
    lastCodeLength = 0;
  }

#ifdef APP_DEBUG
  // Two-line debug overlay along the bottom of the screen.
  const int16_t debugHeight = 22;
  const int16_t debugY = tft.height() - debugHeight;
  const String ipOverlay = "IP: " + network::getWifiIpString();
  const String matchOverlay = String("Match ") + matchStatusToString(network::getRemoteMatchStatus());
  char gameTimerBuffer[8] = {0};
  if (isGameTimerValid()) {
    formatTimeMMSS(getGameTimerRemainingMs(), gameTimerBuffer, sizeof(gameTimerBuffer));
  } else {
    snprintf(gameTimerBuffer, sizeof(gameTimerBuffer), "--:--");
  }
  const String timerOverlay = String("T ") + String(gameTimerBuffer);

  if (ipOverlay != lastDebugIpText || matchOverlay != lastDebugMatchText || timerOverlay != lastDebugTimerText) {
    tft.fillRect(0, debugY, tft.width(), debugHeight, BACKGROUND_COLOR);
    tft.setTextSize(1);

    // Match status on the upper line.
    tft.setTextDatum(TL_DATUM);
    tft.drawString(matchOverlay, 2, debugY + 2);

    // IP on the bottom-left, timer on the bottom-right.
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

void renderConfigPortalScreen(const String &ssid, const String &password) {
  ensureDisplayReady();

  // Force the main UI to redraw after leaving config mode since we repaint the
  // full canvas here.
  layoutDrawn = false;
  bootLayoutDrawn = false;

  static String lastRenderedSsid;
  static String lastRenderedPassword;

  if (lastRenderedSsid == ssid && lastRenderedPassword == password) {
    return;
  }

  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(TITLE_TEXT_SIZE);
  tft.drawString("Config Mode", 10, 10);

  tft.setTextSize(STATUS_TEXT_SIZE);
  tft.drawString("Connect to:", 10, 50);
  tft.drawString(ssid, 10, 70);

  tft.drawString("Password:", 10, 100);
  tft.drawString(password, 10, 120);

  tft.setTextSize(BOOT_DETAIL_TEXT_SIZE);
  const String portalAddress = network::getConfigPortalAddress();
  tft.drawString("Open " + (portalAddress.isEmpty() ? String("http://192.168.4.1") : portalAddress) +
                     " in a browser to update settings.",
                 10, 160);

  lastRenderedSsid = ssid;
  lastRenderedPassword = password;
}

void showArmingConfirmPrompt() {
  ensureDisplayReady();
  drawStaticLayout();
  drawCenteredText("Confirm activation", STATUS_Y, STATUS_TEXT_SIZE, 24);
}

void initUI() {
  // Additional UI initialization (web/touch) will be added later.
}

void updateUI() {
  // Non-blocking UI updates and touch handling will be implemented later.
}
}  // namespace ui
