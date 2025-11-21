#include "ui.h"

#include <TFT_eSPI.h>

#include "state_machine.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t TITLE_COLOR = TFT_ORANGE;
constexpr uint16_t TEXT_COLOR = TFT_WHITE;

TFT_eSPI tft = TFT_eSPI();

String formatTimer(uint32_t ms) {
  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
  return String(buffer);
}

void drawSplashBase() {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TITLE_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Digital Flame", 10, 10);
  tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  tft.drawString("Booting...", 10, 40);
}
}

namespace ui {
void initDisplay() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(0);  // Rotate display 90 degrees relative to previous orientation - use 0 or 2 for portrait

  drawSplashBase();
  renderStatus(getState(), false, false, "", WaitingOnStart, 0);
}

void renderStatus(FlameState state, bool wifiConnected, bool wifiError, const String &wifiIp,
                  MatchStatus matchStatus, uint32_t matchTimerMs) {
  static bool hasRendered = false;
  static FlameState lastRenderedState = ON;
  static bool lastWifiConnected = false;
  static bool lastWifiError = false;
  static String lastIp = "";
  static MatchStatus lastMatchStatus = WaitingOnStart;
  static uint32_t lastMatchTimerBucket = 0;
  static String lastStateLine;
  static String lastWifiLine;
  static String lastIpLine;
  static String lastWifiErrorLine;
  static String lastMatchLine;
  static String lastTimerLine;

  const uint32_t timerBucket = matchTimerMs / 1000;  // Only update UI when visible seconds change.

  if (hasRendered && state == lastRenderedState && wifiConnected == lastWifiConnected &&
      wifiError == lastWifiError && wifiIp == lastIp && matchStatus == lastMatchStatus &&
      timerBucket == lastMatchTimerBucket) {
    // Avoid unnecessary redraws that can cause visible flicker when nothing changed.
    return;
  }

  // Small status area to show current state without redrawing the whole screen.
  constexpr int16_t statusY = 60;
  tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  tft.setTextDatum(TL_DATUM);

  auto updateLine = [&](int16_t y, uint8_t textSize, const String &text, String &lastText) {
    if (!hasRendered || text != lastText) {
      const int16_t lineHeight = static_cast<int16_t>(textSize * 12 + 6);
      tft.setTextSize(textSize);
      tft.fillRect(0, y, tft.width(), lineHeight, BACKGROUND_COLOR);
      if (!text.isEmpty()) {
        tft.drawString(text, 10, y);
      }
      lastText = text;
    }
  };

  String stateText = "State: ";
  stateText += flameStateToString(state);
  updateLine(statusY + 0, 2, stateText, lastStateLine);

  String wifiText = "WiFi: ";
  if (wifiConnected) {
    wifiText += "Connected";
  } else if (wifiError) {
    wifiText += "Error";
  } else {
    wifiText += "Connecting...";
  }
  updateLine(statusY + 30, 2, wifiText, lastWifiLine);

  String ipText;
  if (wifiConnected) {
    ipText = "IP: ";
    ipText += wifiIp;
  }
  updateLine(statusY + 60, 1, ipText, lastIpLine);

  // Show clear recovery instructions when WiFi failures push us into ERROR_STATE.
  String wifiErrorText;
  if (wifiError) {
    wifiErrorText = "WiFi Error - Hold both buttons to reset";
  }
  updateLine(statusY + 80, 1, wifiErrorText, lastWifiErrorLine);

  String matchText = "Match: ";
  matchText += matchStatusToString(matchStatus);
  updateLine(statusY + 110, 2, matchText, lastMatchLine);

  String timerText = "Timer: ";
  timerText += formatTimer(timerBucket * 1000);
  updateLine(statusY + 140, 2, timerText, lastTimerLine);

  hasRendered = true;
  lastRenderedState = state;
  lastWifiConnected = wifiConnected;
  lastWifiError = wifiError;
  lastIp = wifiIp;
  lastMatchStatus = matchStatus;
  lastMatchTimerBucket = timerBucket;
}

void initUI() {
  // Additional UI initialization (web/touch) will be added later.
}

void updateUI() {
  // Non-blocking UI updates and touch handling will be implemented later.
}
}  // namespace ui
