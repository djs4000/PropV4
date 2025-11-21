#include "ui.h"

#include <TFT_eSPI.h>

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

// Shifted downward to keep the title fully visible and better use the canvas height.
constexpr int16_t TITLE_Y = 20;
constexpr int16_t TIMER_Y = 80;
constexpr int16_t STATUS_Y = 150;
constexpr int16_t BAR_Y = 185;
constexpr int16_t BAR_WIDTH = 200;
constexpr int16_t BAR_HEIGHT = 16;
constexpr int16_t BAR_BORDER = 2;
constexpr int16_t CODE_Y = 260;

TFT_eSPI tft = TFT_eSPI();
bool screenInitialized = false;
bool layoutDrawn = false;
bool bootLayoutDrawn = false;

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

void drawCenteredText(const String &text, int16_t y, uint8_t textSize, int16_t clearHeight) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(textSize);
  const int16_t clearY = y - clearHeight / 2;
  tft.fillRect(0, clearY, tft.width(), clearHeight, BACKGROUND_COLOR);
  tft.drawString(text, tft.width() / 2, y);
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

  snprintf(buffer, len, "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
}

void renderBootScreen(const String &wifiSsid, bool wifiConnected, bool wifiFailed,
                      const String &configApSsid, const String &ipAddress,
                      const String &apiEndpoint, bool hasApiResponse) {
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
  const String apiStatusValue = hasApiResponse ? "API response received" : "waiting for API response";

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
  drawCenteredText("Status:", STATUS_Y, STATUS_TEXT_SIZE, 24);

  // Clear progress fill area.
  tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, BAR_WIDTH - 2 * BAR_BORDER,
               BAR_HEIGHT - 2 * BAR_BORDER, BACKGROUND_COLOR);

  // Clear potential defuse code area.
  tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);
}

void renderState(FlameState state, uint32_t bombDurationMs, uint32_t remainingMs, float armingProgress01,
                 uint8_t codeLength, uint8_t /*enteredDigits*/) {
  ensureDisplayReady();
  drawStaticLayout();

  static FlameState lastState = ON;
  static String lastTimerText;
  static String lastStatusText;
  static int16_t lastBarFill = -1;
  static uint8_t lastCodeLength = 0;

  // Timer
  char timeBuffer[8] = {0};
  const uint32_t timerSource = (remainingMs == 0) ? bombDurationMs : remainingMs;
  formatTimeMMSS(timerSource, timeBuffer, sizeof(timeBuffer));
  const String timerText = String(timeBuffer);
  if (timerText != lastTimerText) {
    drawCenteredText(timerText, TIMER_Y, TIMER_TEXT_SIZE, 48);
    lastTimerText = timerText;
  }

  // Status line
  String statusText = "Status: ";
  statusText += flameStateToString(state);
  if (statusText != lastStatusText) {
    drawCenteredText(statusText, STATUS_Y, STATUS_TEXT_SIZE, 24);
    lastStatusText = statusText;
  }

  // Progress bar fill (only visible during arming, empty otherwise)
  const float clampedProgress = (state == ARMING) ? constrain(armingProgress01, 0.0f, 1.0f) : 0.0f;
  const int16_t innerWidth = BAR_WIDTH - 2 * BAR_BORDER;
  const int16_t fillWidth = static_cast<int16_t>(innerWidth * clampedProgress);
  if (fillWidth != lastBarFill) {
    tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, innerWidth, BAR_HEIGHT - 2 * BAR_BORDER,
                 BACKGROUND_COLOR);
    if (fillWidth > 0) {
      tft.fillRect(barX() + BAR_BORDER, BAR_Y + BAR_BORDER, fillWidth, BAR_HEIGHT - 2 * BAR_BORDER,
                   FOREGROUND_COLOR);
    }
    lastBarFill = fillWidth;
  }

  // Defuse code placeholders are only shown when armed.
  if (state == ARMED) {
    if (lastState != ARMED || lastCodeLength != codeLength) {
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
  } else if (lastState == ARMED) {
    // Clear placeholders when leaving ARMED.
    tft.fillRect(0, CODE_Y - 16, tft.width(), 32, BACKGROUND_COLOR);
    lastCodeLength = 0;
  }

#ifdef DEBUG
  // Tiny IP overlay for debugging along the bottom of the screen.
  const int16_t debugHeight = 12;
  const int16_t debugY = tft.height() - debugHeight;
  tft.fillRect(0, debugY, tft.width(), debugHeight, BACKGROUND_COLOR);
  tft.setTextDatum(BL_DATUM);
  tft.setTextSize(1);
  const String ipOverlay = "IP: " + network::getWifiIpString();
  tft.drawString(ipOverlay, 2, tft.height() - 2);
#endif

  lastState = state;
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
  tft.drawString("Open http://192.168.4.1 in a browser to update settings.", 10, 160);

  lastRenderedSsid = ssid;
  lastRenderedPassword = password;
}

void initUI() {
  // Additional UI initialization (web/touch) will be added later.
}

void updateUI() {
  // Non-blocking UI updates and touch handling will be implemented later.
}
}  // namespace ui
